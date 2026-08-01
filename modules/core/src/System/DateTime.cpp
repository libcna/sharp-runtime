// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)

#include "System/DateTime.hpp"
#include "System/detail/DateTimeTextScanner.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/FormatException.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace System {

    // -------------------------------------------------------------------------
    // Private helpers
    // -------------------------------------------------------------------------

    // Converts ticks_ to a UTC std::tm via the C library.
    // Uses int64 time_t so pre-1970 dates (negative Unix timestamp) work on
    // 64-bit Linux/MSVC builds.
    std::tm DateTime::toTm() const {
        const longcs unixTicks = ticks_ - UnixEpochTicks;
        // Floor division toward -inf (C++ truncates toward 0, which is wrong
        // for negative values — e.g. pre-1970 dates lose 1 second).
        longcs q = unixTicks / TicksPerSecond;
        const longcs r = unixTicks % TicksPerSecond;
        if (r < 0) --q;
        const time_t unixSec = static_cast<time_t>(q);
        std::tm result{};
#ifdef _WIN32
        gmtime_s(&result, &unixSec);
#else
        gmtime_r(&unixSec, &result);
#endif
        return result;
    }

    // Direct Gregorian → ticks formula (mirrors .NET internals).
    // Days are counted from 0001-01-01 (day 0 in .NET).
    longcs DateTime::dateToTicks(int year, int month, int day,
                                  int hour, int minute, int second, int millisecond)
    {
        if (year < 1 || year > 9999 || month < 1 || month > 12 || day < 1)
            throw System::ArgumentOutOfRangeException("year", "DateTime: date component out of range");

        // Days-to-month tables for non-leap and leap years
        static const int d365[] = {0,31,59,90,120,151,181,212,243,273,304,334,365};
        static const int d366[] = {0,31,60,91,121,152,182,213,244,274,305,335,366};

        const bool leap = (year % 4 == 0) && (year % 100 != 0 || year % 400 == 0);
        const int* d = leap ? d366 : d365;

        if (day > d[month] - d[month - 1])
            throw System::ArgumentOutOfRangeException("day", "DateTime: day out of range for given month");

        // CCF-002 class A (SR-AUD-006, ticket #1877). Everything below this point is
        // arithmetic, so every remaining component must be range-checked FIRST. Before this,
        // the four time components were multiplied into the tick sum with no check at all,
        // which had three separate consequences, all measured (build-probe/
        // 1876_current_behaviour.log, 1876_ubsan_prefix.log):
        //
        //   1. Silent normalisation. DateTime(2024,1,1,24,0,0) returned 2024-01-02, and
        //      DateTime(2024,1,1,-1,0,0) returned 2023-12-31 23:00 -- a different *year*.
        //   2. A breach of this class's own documented [0, MaxTicks] invariant.
        //      DateTime(9999,12,31,24,0,0) stored MaxTicks + 1 and DateTime(1,1,1,-1,0,0)
        //      stored a negative tick count; the component constructors initialise ticks_
        //      directly, so DateTime(longcs)'s range check never saw them.
        //   3. Undefined behaviour. `hour * TicksPerHour` is `(long long)hour * 36000000000`,
        //      which overflows int64 for |hour| > 256204778 -- UBSan-confirmed at this file's
        //      multiplication below, both from a plain constructor call and from
        //      DateTime::TryParse("2024-06-15 2000000000:00:00"), which returned *true* with
        //      a negative tick count. TryParse's catch(...) cannot help: UB is not an
        //      exception. `hour` is the only operand that can overflow; INTCS_MAX times
        //      TicksPerMinute/Second/Millisecond all stay well inside int64.
        //
        // The rule, the unsigned single-compare idiom (which rejects a negative component by
        // the same test), the messages and the paramNames all mirror real .NET's
        // DateTime.TimeToTicks (DateTime.cs:1111-1133) plus ThrowHelper.cs:234-236 and
        // DateTime.cs:207 -- and they are byte-identical to TimeOnly::validateHms next door,
        // which already had this contract. Validating first is also what makes a post-hoc
        // MaxTicks check unnecessary: with day/hour/minute/second/millisecond all in range the
        // sum cannot exceed MaxTicks, exactly as .NET asserts at DateTime.cs:1129.
        if (static_cast<SharpRuntime::uintcs>(hour) >= 24 ||
            static_cast<SharpRuntime::uintcs>(minute) >= 60 ||
            static_cast<SharpRuntime::uintcs>(second) >= 60)
            throw System::ArgumentOutOfRangeException("",
                "Hour, Minute, and Second parameters describe an un-representable DateTime.");

        if (static_cast<SharpRuntime::uintcs>(millisecond) >= 1000)
            throw System::ArgumentOutOfRangeException("millisecond",
                "Valid values are between 0 and 999, inclusive.");

        const int y = year - 1;
        const longcs days = static_cast<longcs>(y) * 365
                          + y / 4 - y / 100 + y / 400
                          + d[month - 1] + day - 1;

        return days      * TicksPerDay
             + hour      * TicksPerHour
             + minute    * TicksPerMinute
             + second    * TicksPerSecond
             + millisecond * TicksPerMillisecond;
    }

    // -------------------------------------------------------------------------
    // Constructors
    // -------------------------------------------------------------------------

    DateTime::DateTime()
        : ticks_(0) {}

    DateTime::DateTime(longcs ticks)
        : ticks_(ticks) {
        if (ticks < 0 || ticks > MaxTicks)
            throw System::ArgumentOutOfRangeException("ticks", "Ticks must be between DateTime.MinValue.Ticks and DateTime.MaxValue.Ticks.");
    }

    DateTime::DateTime(int year, int month, int day)
        : ticks_(dateToTicks(year, month, day)) {}

    DateTime::DateTime(int year, int month, int day, int hour, int minute, int second)
        : ticks_(dateToTicks(year, month, day, hour, minute, second)) {}

    DateTime::DateTime(int year, int month, int day,
                       int hour, int minute, int second, int millisecond)
        : ticks_(dateToTicks(year, month, day, hour, minute, second, millisecond)) {}

    // -------------------------------------------------------------------------
    // Properties — tick-level decomposition
    // -------------------------------------------------------------------------

    longcs DateTime::getTicksProperty() const { return ticks_; }

    int DateTime::getYearProperty()        const { return toTm().tm_year + 1900; }
    int DateTime::getMonthProperty()       const { return toTm().tm_mon  + 1;    }
    int DateTime::getDayProperty()         const { return toTm().tm_mday;         }
    int DateTime::getHourProperty()        const { return toTm().tm_hour;         }
    int DateTime::getMinuteProperty()      const { return toTm().tm_min;          }
    int DateTime::getSecondProperty()      const { return toTm().tm_sec;          }
    int DateTime::getMillisecondProperty() const {
        return static_cast<int>((ticks_ % TicksPerSecond) / TicksPerMillisecond);
    }

    DayOfWeek DateTime::getDayOfWeekProperty() const {
        return static_cast<DayOfWeek>(toTm().tm_wday); // 0=Sunday … 6=Saturday
    }

    int DateTime::getDayOfYearProperty() const {
        return toTm().tm_yday + 1; // tm_yday is 0-based, .NET is 1-based
    }

    // -------------------------------------------------------------------------
    // Arithmetic
    // -------------------------------------------------------------------------

    DateTime DateTime::Add(const TimeSpan& value) const {
        return AddTicks(value.getTicksProperty());
    }

    DateTime DateTime::AddDays(int days) const {
        if (days < -MaxDays || days > MaxDays)
            throw System::ArgumentOutOfRangeException("value", "Value to add was out of range.");
        return AddTicks(static_cast<longcs>(days) * TicksPerDay);
    }

    DateTime DateTime::AddHours(int hours) const {
        if (hours < -MaxHours || hours > MaxHours)
            throw System::ArgumentOutOfRangeException("value", "Value to add was out of range.");
        return AddTicks(static_cast<longcs>(hours) * TicksPerHour);
    }

    DateTime DateTime::AddMinutes(int minutes) const {
        // No upfront Max* bound check needed here (unlike AddDays/AddHours): MaxTicks /
        // TicksPerMinute exceeds intcs's representable range, so no int argument can make
        // this multiplication overflow int64.
        return AddTicks(static_cast<longcs>(minutes) * TicksPerMinute);
    }

    DateTime DateTime::AddSeconds(int seconds) const {
        return AddTicks(static_cast<longcs>(seconds) * TicksPerSecond);
    }

    DateTime DateTime::AddMilliseconds(int milliseconds) const {
        return AddTicks(static_cast<longcs>(milliseconds) * TicksPerMillisecond);
    }

    DateTime DateTime::Subtract(const TimeSpan& value) const {
        // Matches real .NET's DateTime.Subtract(TimeSpan): `ulong ticks = (ulong)(Ticks -
        // value._ticks); if (ticks > MaxTicks) throw;`. Unsigned arithmetic wraps on
        // overflow/underflow by well-defined C++ semantics (unlike signed, which would be UB
        // here for a TimeSpan near TimeSpan::MinValue/MaxValue), and the single unsigned
        // comparison catches every invalid case -- wrapped-negative-to-huge or genuinely
        // out-of-range -- the same way the constructor's own range check does.
        const auto diff = static_cast<SharpRuntime::ulongcs>(ticks_) - static_cast<SharpRuntime::ulongcs>(value.getTicksProperty());
        if (diff > static_cast<SharpRuntime::ulongcs>(MaxTicks))
            throw System::ArgumentOutOfRangeException("value", "Value to add was out of range.");
        return DateTime(static_cast<longcs>(diff));
    }

    TimeSpan DateTime::Subtract(const DateTime& value) const {
        return TimeSpan(ticks_ - value.ticks_);
    }

    DateTime DateTime::AddTicks(longcs value) const {
        // Matches real .NET's DateTime.AddTicks: `ulong ticks = (ulong)(Ticks + value); if
        // (ticks > MaxTicks) throw;`. Computing `ticks_ + value` directly in signed int64
        // arithmetic (the previous version of this method) is undefined behavior in C++ when
        // it overflows -- confirmed via UBSan for e.g. ticks_ == MaxTicks, value ==
        // Int64::MaxValue. Unsigned addition wraps by well-defined C++ semantics instead, and
        // the single unsigned comparison against MaxTicks catches every invalid case.
        const auto newTicks = static_cast<SharpRuntime::ulongcs>(ticks_) + static_cast<SharpRuntime::ulongcs>(value);
        if (newTicks > static_cast<SharpRuntime::ulongcs>(MaxTicks))
            throw System::ArgumentOutOfRangeException("value", "Value to add was out of range.");
        return DateTime(static_cast<longcs>(newTicks));
    }

    DateTime DateTime::AddMonths(int months) const {
        if (months < -120000 || months > 120000)
            throw System::ArgumentOutOfRangeException("months", "DateTime: months out of range");

        const int year  = getYearProperty();
        const int month = getMonthProperty();
        const int day   = getDayProperty();

        int m = month + months;
        const int q = (m > 0) ? (m - 1) / 12 : m / 12 - 1;
        const int y = year + q;
        m -= q * 12;

        if (y < 1 || y > 9999)
            throw System::ArgumentOutOfRangeException("months", "DateTime: resulting year out of range");

        const int d = std::min(day, DaysInMonth(y, m));
        const longcs timeOfDay = ticks_ % TicksPerDay;
        return DateTime(dateToTicks(y, m, d) + timeOfDay);
    }

    DateTime DateTime::AddYears(int value) const {
        if (value < -10000 || value > 10000)
            throw System::ArgumentOutOfRangeException("value", "DateTime: years out of range");
        return AddMonths(value * 12);
    }

    bool DateTime::IsLeapYear(int year) {
        if (year < 1 || year > 9999)
            throw System::ArgumentOutOfRangeException("year", "DateTime: year out of range");
        return (year % 4 == 0) && (year % 100 != 0 || year % 400 == 0);
    }

    int DateTime::DaysInMonth(int year, int month) {
        if (month < 1 || month > 12)
            throw System::ArgumentOutOfRangeException("month", "DateTime: month out of range");
        static const int dpm365[] = {31,28,31,30,31,30,31,31,30,31,30,31};
        static const int dpm366[] = {31,29,31,30,31,30,31,31,30,31,30,31};
        return (IsLeapYear(year) ? dpm366 : dpm365)[month - 1];
    }

    // -------------------------------------------------------------------------
    // Static factories
    // -------------------------------------------------------------------------

    const DateTime DateTime::MinValue{0LL};
    const DateTime DateTime::MaxValue{DateTime::MaxTicks};
    const DateTime DateTime::UnixEpoch{DateTime::UnixEpochTicks};

    DateTime DateTime::getNowProperty() {
        using namespace std::chrono;
        const auto now      = system_clock::now();
        const auto duration = now.time_since_epoch();
        const auto secs     = duration_cast<seconds>(duration);
        const auto sub      = duration - secs;
        const longcs ticks  = UnixEpochTicks
            + static_cast<longcs>(secs.count()) * TicksPerSecond
            + static_cast<longcs>(duration_cast<nanoseconds>(sub).count() / 100);
        return DateTime(ticks);
    }

    DateTime DateTime::getTodayProperty() {
        const DateTime now = getNowProperty();
        // Truncate to midnight
        return DateTime(now.ticks_ - (now.ticks_ % TicksPerDay));
    }

    TimeSpan DateTime::getTimeOfDayProperty() const {
        return TimeSpan(ticks_ % TicksPerDay);
    }

    // -------------------------------------------------------------------------
    // String
    // -------------------------------------------------------------------------

    std::string DateTime::ToString() const {
        const std::tm t = toTm();
        char buf[32];
        std::snprintf(buf, sizeof(buf),
            "%04d-%02d-%02d %02d:%02d:%02d",
            t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
            t.tm_hour, t.tm_min, t.tm_sec);
        return buf;
    }

    std::string DateTime::ToString(const std::string& format) const
    {
        std::string result;
        result.reserve(format.size() + 8);
        int yr  = getYearProperty(),   mo  = getMonthProperty(),  dy  = getDayProperty();
        int hr  = getHourProperty(),   mn  = getMinuteProperty(), sc  = getSecondProperty();
        const int fractionTicks = static_cast<int>(ticks_ % TicksPerSecond);

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
                result += (n >= 4) ? pad(yr, 4) : pad(yr % 100, 2);
                i += n;
            } else if (c == 'M') {
                int n = run('M');
                static constexpr const char* abbrevMonths[13] = {
                    "", "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
                static constexpr const char* fullMonths[13] = {
                    "", "January", "February", "March", "April", "May", "June",
                    "July", "August", "September", "October", "November", "December"};
                if (n >= 4) result += fullMonths[mo];
                else if (n == 3) result += abbrevMonths[mo];
                else result += (n >= 2) ? pad(mo, 2) : std::to_string(mo);
                i += n;
            } else if (c == 'd') {
                int n = run('d');
                static constexpr const char* abbrevDays[7] = {
                    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
                static constexpr const char* fullDays[7] = {
                    "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
                if (n >= 4) result += fullDays[static_cast<int>(getDayOfWeekProperty())];
                else if (n == 3) result += abbrevDays[static_cast<int>(getDayOfWeekProperty())];
                else result += (n >= 2) ? pad(dy, 2) : std::to_string(dy);
                i += n;
            } else if (c == 'H') {
                int n = run('H');
                result += (n >= 2) ? pad(hr, 2) : std::to_string(hr);
                i += n;
            } else if (c == 'h') {
                int n = run('h');
                int h12 = hr % 12; if (h12 == 0) h12 = 12;
                result += (n >= 2) ? pad(h12, 2) : std::to_string(h12);
                i += n;
            } else if (c == 'm') {
                int n = run('m');
                result += (n >= 2) ? pad(mn, 2) : std::to_string(mn);
                i += n;
            } else if (c == 's') {
                int n = run('s');
                result += (n >= 2) ? pad(sc, 2) : std::to_string(sc);
                i += n;
            } else if (c == 'f') {
                int n = run('f');
                std::string fraction = pad(fractionTicks, 7);
                // Approval covers f through fffffff. Preserve the old three-digit
                // fallback for a longer unsupported run rather than widening it too.
                const int width = (n <= 7) ? n : 3;
                result += fraction.substr(0, static_cast<size_t>(width));
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

    bool DateTime::TryParse(const std::string& s, DateTime& result)
    {
        // Ticket #1880: C# out parameters are definitely assigned. Keep the
        // successful path commit-only, but give every ordinary failure the
        // reference contract's DateTime.MinValue instead of a caller sentinel.
        const auto fail = [&result]() {
            result = DateTime::MinValue;
            return false;
        };
        // CCF-002 class D (SR-AUD-007b, ticket #1879, approved 2026-07-31).
        //
        // This used to check that s[4] and s[7] were dashes and then run one
        // std::sscanf PREFIX conversion, never asking whether the conversion
        // consumed the whole string -- so "2024-06-15junk" was a valid date. Worse,
        // when the "%d:%d:%d" time conversion yielded fewer than three fields it
        // substituted zeros, so "2024-06-15 10:xx:00" and "2024-06-15 trailing"
        // both parsed successfully as MIDNIGHT: a wrong answer with no diagnostic.
        //
        // The grammar below is the port's documented one and nothing more
        // (docs/DateTimeValidationBoundaryPlan.md §16.4 keeps widening it out of
        // scope), now required to match the WHOLE string:
        //
        //     W* yyyy '-' MM '-' dd
        //        [ (' '|'T') H{1,2} ':' m{1,2} ':' s{1,2} [ '.' f{1,7} ] ]
        //        [ 'Z'|'z' ] W*
        //
        // Every currently-valid input still produces its exact previous value.
        // Correction/remediation (#1929 rows 5-6, approved 2026-08-01): the preceding
        // historical grammar accurately described the repaired #1879 subset, but it was
        // narrower than both the related port doors and current .NET. Outer invariant
        // whitespace, one-or-two digit clock fields, and one through seven fractional
        // digits are now the approved shared contract. Date fields, offsets and internal
        // whitespace remain deliberately fixed/rejected.
        detail::DateTimeTextScanner scanner(detail::trimDateTimeText(s));
        int yr = 0, mo = 0, dy = 0, hr = 0, mn = 0, sc = 0, fractionTicks = 0;
        if (!scanner.takeDigits(4, 4, yr) || !scanner.take('-') ||
            !scanner.takeDigits(2, 2, mo) || !scanner.take('-') ||
            !scanner.takeDigits(2, 2, dy))
            return fail();

        if (scanner.take(' ') || scanner.take('T')) {
            if (!scanner.takeDigits(1, 2, hr) || !scanner.take(':') ||
                !scanner.takeDigits(1, 2, mn) || !scanner.take(':') ||
                !scanner.takeDigits(1, 2, sc))
                return fail();
            if (scanner.take('.')) {
                // A fraction must have 1-7 digits. A bare ".", a non-numeric
                // ".abc", and an eighth digit are rejected rather than read as a
                // prefix. DateTime is tick-based, so every accepted digit is retained.
                int digits = 0;
                if (!scanner.takeDigits(1, 7, fractionTicks, &digits)) return fail();
                while (digits < 7) { fractionTicks *= 10; ++digits; }
            }
        }
        // A trailing time-zone designator stays accepted, and stays ignored: this
        // port has no DateTimeKind (§16.4), so "…T10:20:30Z" and
        // "…T10:20:30.123+02:00" keep the exact values they have always had.
        // §20.1's "unchanged in every option" list names both spellings, and
        // three DateTimeTests pin the offset one. The offset fields are two
        // digits each, the same rule DateTimeOffset applies -- and the same rule
        // the date fields have always had, since "2024-6-15" was never accepted
        // either.
        if (!scanner.take('Z') && !scanner.take('z')) {
            if (scanner.take('+') || scanner.take('-')) {
                int offsetHours = 0, offsetMinutes = 0;
                if (!scanner.takeDigits(2, 2, offsetHours) || !scanner.take(':') ||
                    !scanner.takeDigits(2, 2, offsetMinutes))
                    return fail();
            }
        }
        if (!scanner.atEnd()) return fail();

        try {
            result = DateTime(dateToTicks(yr, mo, dy, hr, mn, sc) + fractionTicks);
            return true;
        } catch (...) { return fail(); }
    }

    DateTime DateTime::Parse(const std::string& s)
    {
        DateTime result;
        if (!TryParse(s, result))
            throw FormatException("String was not recognized as a valid DateTime: " + s);
        return result;
    }

    // -------------------------------------------------------------------------
    // Comparison operators
    // -------------------------------------------------------------------------

    bool DateTime::operator==(const DateTime& o) const { return ticks_ == o.ticks_; }
    bool DateTime::operator!=(const DateTime& o) const { return ticks_ != o.ticks_; }
    bool DateTime::operator< (const DateTime& o) const { return ticks_ <  o.ticks_; }
    bool DateTime::operator<=(const DateTime& o) const { return ticks_ <= o.ticks_; }
    bool DateTime::operator> (const DateTime& o) const { return ticks_ >  o.ticks_; }
    bool DateTime::operator>=(const DateTime& o) const { return ticks_ >= o.ticks_; }

    GetTypeNameCPP(DateTime, "System::DateTime")

} // namespace System
