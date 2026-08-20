// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)

#include "System/DateOnly.hpp"
#include "System/detail/DateTimeTextScanner.hpp"
#include "System/DateTime.hpp"
#include "System/TimeOnly.hpp"
#include "System/FormatException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/detail/InvariantExactDateTimeParser.hpp"
#include <algorithm>
#include <cstdio>

namespace System {

DateOnly::DateOnly(intcs year, intcs month, intcs day)
    : year_(year), month_(month), day_(day) {
    // Delegates validation to DateTime's constructor (the single source of truth for
    // calendar-date validity in this codebase), matching .NET's own
    // DateOnly(int,int,int) => DayNumberFromDateTime(new DateTime(year, month, day)).
    (void)DateTime(year, month, day);
}

const DateOnly DateOnly::MinValue{1, 1, 1};
const DateOnly DateOnly::MaxValue{9999, 12, 31};

// Julian Day Number helpers for day-accurate date arithmetic
static int dateToJDN(int y, int m, int d) {
    return d - 32075
         + 1461 * (y + 4800 + (m - 14) / 12) / 4
         + 367  * (m - 2   - (m - 14) / 12 * 12) / 12
         - 3    * ((y + 4900 + (m - 14) / 12) / 100) / 4;
}

static void jdnToDate(int jdn, int& y, int& m, int& d) {
    int l = jdn + 68569;
    int n = 4 * l / 146097;
    l = l - (146097 * n + 3) / 4;
    int i = 4000 * (l + 1) / 1461001;
    l = l - 1461 * i / 4 + 31;
    int j = 80 * l / 2447;
    d = l - 2447 * j / 80;
    l = j / 11;
    m = j + 2 - 12 * l;
    y = 100 * (n - 49) + i + l;
}

// JDN of 0001-01-01 (DateOnly epoch)
static constexpr int JDN_EPOCH = 1721426;

// Largest valid day number, i.e. DateOnly::MaxValue.getDayNumberProperty(). Equal to
// .NET's `MaxDayNumber = DateTime.DaysTo10000 - 1` (DateOnly.cs:30, DateTime.cs:75),
// measured 3652058 for 9999-12-31 in this port too (build-probe/1837_dateonly_surface
// case 9). The valid domain is the closed range [0, kMaxDayNumber]; one unsigned
// compare against it rejects negative and overlarge day numbers together.
static constexpr SharpRuntime::intcs kMaxDayNumber = 3652058;

DayOfWeek DateOnly::getDayOfWeekProperty() const {
    // JDN % 7: 0=Monday … 6=Sunday → map to .NET Sunday=0
    int jdn = dateToJDN(year_, month_, day_);
    return static_cast<DayOfWeek>((jdn + 1) % 7);
}

intcs DateOnly::getDayOfYearProperty() const {
    return dateToJDN(year_, month_, day_) - dateToJDN(year_, 1, 1) + 1;
}

intcs DateOnly::getDayNumberProperty() const {
    return dateToJDN(year_, month_, day_) - JDN_EPOCH;
}

DateOnly DateOnly::FromDayNumber(intcs dayNumber) {
    // CCF-004 class C (SR-AUD-060, ticket #1837). Mirror .NET DateOnly.FromDayNumber
    // (DateOnly.cs:73-81): one unsigned compare rejects a negative and an overlarge day
    // number together, *before* the conversion, so jdnToDate below can never receive an
    // out-of-range argument. Before this, `dayNumber + JDN_EPOCH` overflowed at
    // DateOnly.cpp:65 for INTCS_MAX; for INTCS_MIN it did not overflow there but drove the
    // :35/:37/:39 multiplication cascade inside jdnToDate with a wildly out-of-range value
    // (build-probe/1837_dateonly_surface cases 1 and 2). This is the class C rejection: the
    // inputs it now rejects previously produced undefined behaviour, not a usable date.
    if (static_cast<SharpRuntime::uintcs>(dayNumber) > static_cast<SharpRuntime::uintcs>(kMaxDayNumber))
        throw ArgumentOutOfRangeException("dayNumber",
            "Day number must be between 0 and DateOnly.MaxValue.DayNumber.");
    int y, m, d;
    jdnToDate(static_cast<int>(dayNumber) + JDN_EPOCH, y, m, d);
    return DateOnly(y, m, d);
}

intcs DateOnly::CompareTo(const DateOnly& other) const {
    intcs a = getDayNumberProperty(), b = other.getDayNumberProperty();
    return (a < b) ? -1 : (a > b) ? 1 : 0;
}

DateOnly DateOnly::AddDays(intcs n) const {
    // CCF-004 class C (SR-AUD-060, ticket #1837). Mirror .NET DateOnly.AddDays
    // (DateOnly.cs:121-132): add in the unsigned domain (a defined wrap) and reject with a
    // single unsigned compare, which catches the overflowed and the merely out-of-range
    // result alike. Before this, `dateToJDN(...) + n` overflowed signed int at
    // DateOnly.cpp:76 (INTCS_MAX), and when it wrapped it drove jdnToDate's :35/:37/:39
    // cascade (build-probe/1837_dateonly_surface cases 3 and 4). The guard keeps jdnToDate's
    // argument in [JDN_EPOCH, JDN_EPOCH + kMaxDayNumber], where it cannot overflow. Adding
    // in the day-number domain (rather than the JDN domain) is exactly what .NET does, and
    // yields the identical result for every value that used to succeed.
    const intcs dayNumber = getDayNumberProperty();          // always in [0, kMaxDayNumber]
    const intcs newDayNumber = static_cast<intcs>(
        static_cast<SharpRuntime::uintcs>(dayNumber) + static_cast<SharpRuntime::uintcs>(n));
    if (static_cast<SharpRuntime::uintcs>(newDayNumber) > static_cast<SharpRuntime::uintcs>(kMaxDayNumber))
        throw ArgumentOutOfRangeException("value", "Value to add was out of range.");
    int y, m, d;
    jdnToDate(static_cast<int>(newDayNumber) + JDN_EPOCH, y, m, d);
    return DateOnly(y, m, d);
}

DateOnly DateOnly::AddMonths(intcs n) const {
    // CCF-004 class C (SR-AUD-060, ticket #1837). .NET routes DateOnly.AddMonths through
    // DateTime.AddMonths (DateOnly.cs:139), and this port's DateTime::AddMonths
    // (DateTime.cpp:183-201) already mirrors DateTime.cs:960-977: bound the delta to
    // +/-120000 *before* any arithmetic, then normalise with one division. That bound does
    // two jobs here -- it makes `month_ + n` unable to overflow (it did at DateOnly.cpp:81
    // for AddMonths(INTCS_MAX)) and it replaces the old `while (m < 1) { m += 12; --y; }`
    // loop, which ran about 179 million times for AddMonths(INTCS_MIN) before returning
    // (docs/DefinedArithmeticBoundaryPlan.md 16.5). paramName "months" matches
    // DateTime::AddMonths, the sibling this delegates its contract to.
    if (n < -120000 || n > 120000)
        throw ArgumentOutOfRangeException("months", "DateTime: months out of range");
    int m = month_ + static_cast<int>(n);                 // |n| <= 120000 => defined
    const int q = (m > 0) ? (m - 1) / 12 : m / 12 - 1;    // branchless normalisation
    const int y = year_ + q;
    m -= q * 12;
    if (y < 1 || y > 9999)
        throw ArgumentOutOfRangeException("months", "DateTime: resulting year out of range");
    static const int dpm[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
    const bool leap = (y % 4 == 0) && (y % 100 != 0 || y % 400 == 0);
    const int maxDay = (m == 2 && leap) ? 29 : dpm[m];
    int d = day_;
    if (d > maxDay) d = maxDay;
    return DateOnly(y, m, d);
}

DateOnly DateOnly::AddYears(intcs n) const {
    // CCF-004 class C (SR-AUD-060, ticket #1837). Mirror .NET DateTime.AddYears
    // (DateTime.cs:1020-1032): bound the delta to +/-10000 -- which makes `n * 12` unable to
    // overflow (it did at DateOnly.cpp:92) -- then reject an unrepresentable resulting year,
    // both naming "value". The pre-repair code was `AddMonths(n * 12)` with no bound: for
    // AddYears(INTCS_MIN), `n * 12` is exactly -6*2^32, which wrapped to ZERO and made
    // AddYears silently return this date unchanged -- a wrong answer, not merely UB, and the
    // second silent-wrong-answer member of CCF-004 (docs/DefinedArithmeticBoundaryPlan.md
    // 16.4). The resulting-year check here carries paramName "value" so an out-of-range
    // AddYears reports "value" rather than the "months" its AddMonths delegate would report,
    // matching .NET DateTime.AddYears's ThrowDateArithmetic(0).
    if (n < -10000 || n > 10000)
        throw ArgumentOutOfRangeException("value", "DateTime: years out of range");
    const int resultingYear = year_ + static_cast<int>(n);  // in [-9999, 19999] => defined
    if (resultingYear < 1 || resultingYear > 9999)
        throw ArgumentOutOfRangeException("value", "DateTime: resulting year out of range");
    return AddMonths(n * 12);                               // n in [-10000,10000] => defined
}

DateOnly DateOnly::FromDateTime(const DateTime& dt) {
    return DateOnly(dt.getYearProperty(), dt.getMonthProperty(), dt.getDayProperty());
}

DateTime DateOnly::ToDateTime(const TimeOnly& time) const {
    // #1929 row 6 correction: TimeOnly is tick-based; carrying only its millisecond
    // component discarded the approved fourth-through-seventh fractional digits.
    return DateTime(year_, month_, day_).AddTicks(time.getTicksProperty());
}

bool DateOnly::TryParse(const std::string& s, DateOnly& result) {
    // Ticket #1880: every false result assigns DateOnly.MinValue, matching
    // .NET's out contract while leaving the success path commit-only.
    const auto fail = [&result]() {
        result = DateOnly::MinValue;
        return false;
    };
    // CCF-002 class D (SR-AUD-061, ticket #1879, approved 2026-07-31). The
    // std::sscanf PREFIX conversion accepted "2024-06-15junk" and, worse,
    // "2024-06-15 10:20:30" -- a full timestamp silently truncated to its date.
    // The grammar is now required to match the WHOLE string:
    //
    //     yyyy '-' M{1,2} '-' d{1,2} [ 'Z'|'z' ]
    //
    // #1929 row 1 (decided 2026-08-18): the month and day admit one or two digits,
    // as .NET's do. The year stays exactly four -- three or fewer would collide with
    // .NET's two-digit-year century window, which is culture state this port has no
    // way to carry.
    detail::DateTimeTextScanner scanner(detail::trimDateTimeText(s));
    int y = 0, m = 0, d = 0;
    if (!scanner.takeDigits(4, 4, y) || !scanner.take('-') ||
        !scanner.takeDigits(1, 2, m) || !scanner.take('-') ||
        !scanner.takeDigits(1, 2, d))
        return fail();
    // A trailing UTC designator stays accepted and stays ignored, matching
    // DateTime::TryParse; "2024-06-15Z" parsed before this ticket and still does.
    if (!scanner.take('Z')) (void)scanner.take('z');
    if (!scanner.atEnd()) return fail();
    if (y < 1 || y > 9999 || m < 1 || m > 12 || d < 1 || d > 31) return fail();
    try {
        result = DateOnly(y, m, d);
    } catch (...) {
        // Rejects dates with a day that doesn't exist in the given month/year
        // (e.g. February 30, or February 29 in a non-leap year).
        return fail();
    }
    return true;
}

DateOnly DateOnly::Parse(const std::string& s) {
    DateOnly result;
    if (!TryParse(s, result))
        throw FormatException("String was not recognized as a valid DateOnly: " + s);
    return result;
}

std::string DateOnly::ToString(const std::string& format) const {
    std::string result;
    result.reserve(format.size() + 4);
    auto pad = [](int n, int w) -> std::string {
        std::string s = std::to_string(n);
        while (static_cast<int>(s.size()) < w) s = "0" + s;
        return s;
    };
    size_t i = 0;
    while (i < format.size()) {
        char c = format[i];
        auto run = [&](char ch) {
            size_t k = i + 1;
            while (k < format.size() && format[k] == ch) ++k;
            return static_cast<int>(k - i);
        };
        if (c == 'y') {
            int n = run('y');
            result += (n >= 4) ? pad(year_, 4) : pad(year_ % 100, 2);
            i += n;
        } else if (c == 'M') {
            int n = run('M');
            result += (n >= 2) ? pad(month_, 2) : std::to_string(month_);
            i += n;
        } else if (c == 'd') {
            int n = run('d');
            result += (n >= 2) ? pad(day_, 2) : std::to_string(day_);
            i += n;
        } else if (c == '\'') {
            ++i;
            while (i < format.size() && format[i] != '\'') result += format[i++];
            if (i < format.size()) ++i;
        } else {
            result += c;
            ++i;
        }
    }
    return result;
}

} // namespace System

// ---------------------------------------------------------------------------
// ParseExact / TryParseExact -- ticket #1939 (#1929 row 4A)
// ---------------------------------------------------------------------------

namespace System {

    bool DateOnly::TryParseExact(const std::string& input, const std::string& format,
                                 DateOnly& result) {
        // #2412: the two-argument form is the null-provider, no-style case of the four-argument
        // one. Delegating rather than duplicating is what makes "no existing result changes" a
        // fact -- there is no second scanner path left to drift.
        return TryParseExact(input, format, nullptr,
                             System::Globalization::DateTimeStyles::None, result);
    }

    bool DateOnly::TryParseExact(const std::string& input, const std::string& format,
                                 const System::IFormatProvider* provider,
                                 System::Globalization::DateTimeStyles style, DateOnly& result) {
        // .NET raises here rather than returning false -- a Try* method that throws
        // (DateOnly.cs). Validation happens BEFORE the result is written, so a rejected style
        // leaves the caller's variable untouched.
        detail::ValidateDateTimeOnlyStyles(style);
        const detail::ExactParseOptions options = detail::ResolveExactParseOptions(provider, style);

        result = DateOnly::MinValue;

        std::string pattern = detail::ExpandStandardFormat(format, /*forDate=*/true);
        if (pattern.empty()) {
            // A ONE-CHARACTER format that is not standard is not silently custom either: .NET
            // requires "%d" for a single-specifier custom format, because a bare "d" is the
            // standard short-date pattern. Preserving that distinction is what stops "d" from
            // quietly meaning something this port does not implement.
            if (format.size() == 1) return false;
            pattern = format;
        }

        detail::ExactDateTimeFields fields;
        if (!detail::MatchExactFormat(input, pattern, /*forDate=*/true, fields, options)) return false;

        // The approval requires ONE COMPLETE year/month/day. There is no current-date default
        // here -- that is NoCurrentDateDefault, a style, and styles are 4C.
        if (fields.year < 0 || fields.month < 0 || fields.day < 0) return false;
        if (fields.year < 1 || fields.year > 9999) return false;
        if (fields.month < 1 || fields.month > 12) return false;
        if (fields.day < 1 || fields.day > DateTime::DaysInMonth(fields.year, fields.month))
            return false;

        DateOnly candidate(1, 1, 1);
        try {
            candidate = DateOnly(fields.year, fields.month, fields.day);
        } catch (...) {
            return false;
        }

        // Weekday AGREEMENT, which is what makes `R` a validating format rather than a shape.
        // "Mon, 15 Jun 2024" names a Saturday and must fail.
        if (fields.weekday >= 0 &&
            static_cast<int>(candidate.getDayOfWeekProperty()) != fields.weekday)
            return false;

        result = candidate;
        return true;
    }

    DateOnly DateOnly::ParseExact(const std::string& input, const std::string& format) {
        DateOnly result(1, 1, 1);
        if (!TryParseExact(input, format, result))
            throw FormatException("String was not recognized as a valid DateOnly: " + input);
        return result;
    }

    DateOnly DateOnly::ParseExact(const std::string& input, const std::string& format,
                                 const System::IFormatProvider* provider,
                                 System::Globalization::DateTimeStyles style) {
        DateOnly result = DateOnly::MinValue;
        // The style is validated by TryParseExact below, which raises for an illegal one exactly
        // as .NET's does; a PARSE failure is what becomes the FormatException here.
        if (!TryParseExact(input, format, provider, style, result))
            throw FormatException("String was not recognized as a valid DateOnly: " + input);
        return result;
    }

} // namespace System
