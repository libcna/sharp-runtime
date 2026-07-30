// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)

#include "System/DateOnly.hpp"
#include "System/DateTime.hpp"
#include "System/TimeOnly.hpp"
#include "System/FormatException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
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
    return DateTime(year_, month_, day_,
                     time.getHourProperty(), time.getMinuteProperty(),
                     time.getSecondProperty(), time.getMillisecondProperty());
}

bool DateOnly::TryParse(const std::string& s, DateOnly& result) {
    int y, m, d;
    if (s.size() < 10 || s[4] != '-' || s[7] != '-') return false;
    if (std::sscanf(s.c_str(), "%d-%d-%d", &y, &m, &d) != 3) return false;
    if (y < 1 || y > 9999 || m < 1 || m > 12 || d < 1 || d > 31) return false;
    try {
        result = DateOnly(y, m, d);
    } catch (...) {
        // Rejects dates with a day that doesn't exist in the given month/year
        // (e.g. February 30, or February 29 in a non-leap year).
        return false;
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
