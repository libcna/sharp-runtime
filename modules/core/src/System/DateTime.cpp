// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)

#include "System/DateTime.hpp"
#include "System/ILocalTimeZone.hpp"
#include "System/Globalization/DateTimeFormatInfo.hpp"
#include <array>
#include "System/detail/DateTimeTextScanner.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/FormatException.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string_view>
#include "System/detail/InvariantExactDateTimeParser.hpp"
#include "System/Globalization/DateTimeStyles.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/detail/ProcessTimeZoneState.hpp"

namespace System {

    // -------------------------------------------------------------------------
    // Private helpers
    // -------------------------------------------------------------------------

    // Decomposes the full DateTime range arithmetically. MSVC's `_gmtime64_s` accepts only
    // 1970-01-01 through 3000-12-31, while DateTime promises years 1 through 9999; feeding a
    // failed CRT result into the public component properties also broke the Windows timezone
    // adapter for historical/future values. The Gregorian cycle below is .NET's GetDatePart
    // decomposition and has no platform timestamp range.
    std::tm DateTime::toTm() const {
        static constexpr int daysPerYear = 365;
        static constexpr int daysPer4Years = daysPerYear * 4 + 1;
        static constexpr int daysPer100Years = daysPer4Years * 25 - 1;
        static constexpr int daysPer400Years = daysPer100Years * 4 + 1;
        static constexpr int daysToMonth365[] =
            {0,31,59,90,120,151,181,212,243,273,304,334,365};
        static constexpr int daysToMonth366[] =
            {0,31,60,91,121,152,182,213,244,274,305,335,366};

        const longcs rawTicks = ticks();
        int remainingDays = static_cast<int>(rawTicks / TicksPerDay);
        const int absoluteDays = remainingDays;

        const int years400 = remainingDays / daysPer400Years;
        remainingDays -= years400 * daysPer400Years;
        int years100 = remainingDays / daysPer100Years;
        if (years100 == 4) years100 = 3;
        remainingDays -= years100 * daysPer100Years;
        const int years4 = remainingDays / daysPer4Years;
        remainingDays -= years4 * daysPer4Years;
        int years1 = remainingDays / daysPerYear;
        if (years1 == 4) years1 = 3;

        const int year = years400 * 400 + years100 * 100 + years4 * 4 + years1 + 1;
        remainingDays -= years1 * daysPerYear;
        const bool leap = years1 == 3 && (years4 != 24 || years100 == 3);
        const int* daysToMonth = leap ? daysToMonth366 : daysToMonth365;
        int month = (remainingDays >> 5) + 1;
        while (remainingDays >= daysToMonth[month]) ++month;

        longcs timeTicks = rawTicks % TicksPerDay;
        std::tm result{};
        result.tm_year = year - 1900;
        result.tm_mon = month - 1;
        result.tm_mday = remainingDays - daysToMonth[month - 1] + 1;
        result.tm_yday = remainingDays;
        result.tm_wday = (absoluteDays + 1) % 7; // 0001-01-01 was Monday; tm Sunday is zero.
        result.tm_hour = static_cast<int>(timeTicks / TicksPerHour);
        timeTicks %= TicksPerHour;
        result.tm_min = static_cast<int>(timeTicks / TicksPerMinute);
        timeTicks %= TicksPerMinute;
        result.tm_sec = static_cast<int>(timeTicks / TicksPerSecond);
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
        //      stored a negative tick count; the component constructors initialise ticks()
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
        : dateData_(0) {}

    namespace {
        // #1941. .NET's packing constants, DateTime.cs:118-123.
        constexpr int kKindShift = 62;   // FlagsMask is 0xC000000000000000; see DateTime.hpp

        /// .NET's `(uint)kind > (uint)DateTimeKind.Local` (DateTime.cs:1309).
        void throwIfInvalidKind(DateTimeKind kind) {
            const auto raw = static_cast<unsigned int>(kind);
            if (raw > static_cast<unsigned int>(DateTimeKind::Local))
                throw System::ArgumentException("Invalid DateTimeKind value.", "kind");
        }

        // DateTimeOffset's Core-local current-time path established this platform policy:
        // reentrant C-library conversion on hosted platforms, and UTC-as-local on Emscripten.
        // Keep DateTime in Core.Base rather than reaching up into the TimeZone component (which
        // depends on Core.Base), while still avoiding std::localtime's shared result buffer.
        bool tryGetCurrentLocalTime(std::time_t unixSeconds, std::tm& result) {
#if defined(_WIN32)
            return ::localtime_s(&result, &unixSeconds) == 0;
#elif defined(__EMSCRIPTEN__)
            return ::gmtime_r(&unixSeconds, &result) != nullptr;
#else
            std::lock_guard<std::mutex> lock(System::detail::processTimeZoneMutex());
            return ::localtime_r(&unixSeconds, &result) != nullptr;
#endif
        }
    }

    DateTime::DateTime(longcs ticks)
        : dateData_(static_cast<unsigned long long>(ticks)) {
        if (ticks < 0 || ticks > MaxTicks)
            throw System::ArgumentOutOfRangeException("ticks", "Ticks must be between DateTime.MinValue.Ticks and DateTime.MaxValue.Ticks.");
    }

    DateTime::DateTime(longcs ticks, DateTimeKind kind)
        : dateData_(static_cast<unsigned long long>(ticks)) {
        // The range check runs BEFORE packing, which the approval requires and which is not
        // decoration: an out-of-range tick count would otherwise collide with the flag bits and
        // silently report a kind the caller never asked for.
        if (ticks < 0 || ticks > MaxTicks)
            throw System::ArgumentOutOfRangeException("ticks", "Ticks must be between DateTime.MinValue.Ticks and DateTime.MaxValue.Ticks.");
        throwIfInvalidKind(kind);
        dateData_ |= static_cast<unsigned long long>(static_cast<unsigned int>(kind)) << kKindShift;
    }

    DateTimeKind DateTime::getKindProperty() const {
        // .NET's bit trick, transcribed with its own comment: "values 0-2 map directly to
        // DateTimeKind, 3 (LocalAmbiguousDst) needs to be mapped to 2 (Local) using bit0 NAND
        // bit1" (DateTime.cs:1463-1465). Current conversion paths still do not set the fourth
        // encoding; keeping the fold lets a future ambiguity-capable path do so without changing
        // this accessor or the arithmetic that now preserves the raw InternalKind bits.
        const auto kind = static_cast<unsigned int>(dateData_ >> kKindShift);
        return static_cast<DateTimeKind>(kind & ~(kind >> 1));
    }

    DateTime DateTime::SpecifyKind(const DateTime& value, DateTimeKind kind) {
        throwIfInvalidKind(kind);
        return DateTime(value.ticks(), kind);
    }

    DateTime::DateTime(int year, int month, int day)
        : dateData_(dateToTicks(year, month, day)) {}

    DateTime::DateTime(int year, int month, int day, int hour, int minute, int second)
        : dateData_(dateToTicks(year, month, day, hour, minute, second)) {}

    DateTime::DateTime(int year, int month, int day,
                       int hour, int minute, int second, int millisecond)
        : dateData_(dateToTicks(year, month, day, hour, minute, second, millisecond)) {}

    // -------------------------------------------------------------------------
    // Properties — tick-level decomposition
    // -------------------------------------------------------------------------

    longcs DateTime::getTicksProperty() const { return ticks(); }

    DateTime DateTime::withTicksPreservingKind(longcs replacementTicks) const {
        DateTime result(replacementTicks);
        // Preserve InternalKind, not merely the public Kind. The fourth packed state reports as
        // Local through getKindProperty(), so reconstructing via that accessor would silently
        // erase the ambiguous-DST marker. .NET's AddTicks/AddMonths/Subtract do the same raw OR.
        result.dateData_ |= dateData_ & FlagsMask;
        return result;
    }

    int DateTime::getYearProperty()        const { return toTm().tm_year + 1900; }
    int DateTime::getMonthProperty()       const { return toTm().tm_mon  + 1;    }
    int DateTime::getDayProperty()         const { return toTm().tm_mday;         }
    int DateTime::getHourProperty()        const { return toTm().tm_hour;         }
    int DateTime::getMinuteProperty()      const { return toTm().tm_min;          }
    int DateTime::getSecondProperty()      const { return toTm().tm_sec;          }
    int DateTime::getMillisecondProperty() const {
        return static_cast<int>((ticks() % TicksPerSecond) / TicksPerMillisecond);
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
        const auto diff = static_cast<SharpRuntime::ulongcs>(ticks()) - static_cast<SharpRuntime::ulongcs>(value.getTicksProperty());
        if (diff > static_cast<SharpRuntime::ulongcs>(MaxTicks))
            throw System::ArgumentOutOfRangeException("value", "Value to add was out of range.");
        return withTicksPreservingKind(static_cast<longcs>(diff));
    }

    TimeSpan DateTime::Subtract(const DateTime& value) const {
        return TimeSpan(ticks() - value.ticks());
    }

    DateTime DateTime::AddTicks(longcs value) const {
        // Matches real .NET's DateTime.AddTicks: `ulong ticks = (ulong)(Ticks + value); if
        // (ticks > MaxTicks) throw;`. Computing `ticks() + value` directly in signed int64
        // arithmetic (the previous version of this method) is undefined behavior in C++ when
        // it overflows -- confirmed via UBSan for e.g. ticks() == MaxTicks, value ==
        // Int64::MaxValue. Unsigned addition wraps by well-defined C++ semantics instead, and
        // the single unsigned comparison against MaxTicks catches every invalid case.
        const auto newTicks = static_cast<SharpRuntime::ulongcs>(ticks()) + static_cast<SharpRuntime::ulongcs>(value);
        if (newTicks > static_cast<SharpRuntime::ulongcs>(MaxTicks))
            throw System::ArgumentOutOfRangeException("value", "Value to add was out of range.");
        return withTicksPreservingKind(static_cast<longcs>(newTicks));
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
        const longcs timeOfDay = ticks() % TicksPerDay;
        return withTicksPreservingKind(dateToTicks(y, m, d) + timeOfDay);
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
    const DateTime DateTime::UnixEpoch{DateTime::UnixEpochTicks, DateTimeKind::Utc};

    DateTime DateTime::getNowProperty() {
        using namespace std::chrono;
        const auto now = system_clock::now();
        const auto sinceUnixEpoch = now.time_since_epoch();
        // floor() rather than duration_cast() keeps the sub-second remainder non-negative even
        // for a clock before 1970. The current clock is ordinarily positive, but this costs
        // nothing and makes the UTC fallback's arithmetic correct over the whole time_t domain.
        const auto wholeSeconds = floor<seconds>(sinceUnixEpoch);
        const auto subSecond = sinceUnixEpoch - wholeSeconds;
        const longcs subSecondTicks = static_cast<longcs>(
            duration_cast<nanoseconds>(subSecond).count() / 100);
        const std::time_t unixSeconds = static_cast<std::time_t>(wholeSeconds.count());

        std::tm local{};
        if (!tryGetCurrentLocalTime(unixSeconds, local)) {
            // There is no useful exception contract for a failed platform clock conversion.
            // Preserve the instant and the required Local kind with a zero-offset fallback,
            // matching Core's Emscripten policy rather than returning an invalid DateTime.
            const longcs utcTicks = UnixEpochTicks
                + static_cast<longcs>(wholeSeconds.count()) * TicksPerSecond + subSecondTicks;
            return DateTime(utcTicks, DateTimeKind::Local);
        }

        // The DateTime tick payload of a Local value is the local WALL CLOCK, not the UTC
        // instant. Building from local calendar fields handles fractional-hour offsets and DST
        // without asking Core.Base to name a TimeZone type; retain the original sub-second ticks.
        const longcs localTicks = dateToTicks(
            local.tm_year + 1900, local.tm_mon + 1, local.tm_mday,
            local.tm_hour, local.tm_min, local.tm_sec) + subSecondTicks;
        return DateTime(localTicks, DateTimeKind::Local);
    }

    DateTime DateTime::getTodayProperty() {
        const DateTime now = getNowProperty();
        // Today is Now.Date in .NET. Going through the same kind-preserving arithmetic primitive
        // pins both parts of that contract: local wall-clock midnight and Kind Local.
        return now.AddTicks(-(now.ticks() % TicksPerDay));
    }

    TimeSpan DateTime::getTimeOfDayProperty() const {
        return TimeSpan(ticks() % TicksPerDay);
    }

    // -------------------------------------------------------------------------
    // String
    // -------------------------------------------------------------------------

    // #1941 PHASE 2. Both bodies are DateTime.cs's, and the two Unspecified rules are deliberately
    // NOT symmetric: ToLocalTime tests only the Local bit (:1707) so Unspecified converts as UTC,
    // while ToUniversalTime returns early only for Utc (:1772) so Unspecified converts as local.
    DateTime DateTime::ToLocalTime(const ILocalTimeZone& zone) const {
        if (getKindProperty() == DateTimeKind::Local) return *this;
        const longcs offset = zone.GetUtcOffsetFromUniversalTime(*this).getTicksProperty();
        // :1718-1721 -- clamp rather than throw. A conversion at the very edge of the range is a
        // representable answer in .NET and must not become an exception here. Test the bounds
        // BEFORE adding: ILocalTimeZone is public and may return TimeSpan::MinValue/MaxValue, so
        // computing `ticks() + offset` first would itself be signed-overflow UB.
        const longcs currentTicks = ticks();
        if (offset > MaxTicks - currentTicks)
            return DateTime(MaxTicks, DateTimeKind::Local);
        if (offset < -currentTicks)
            return DateTime(0LL, DateTimeKind::Local);
        return DateTime(currentTicks + offset, DateTimeKind::Local);
    }

    DateTime DateTime::ToUniversalTime(const ILocalTimeZone& zone) const {
        if (getKindProperty() == DateTimeKind::Utc) return *this;
        const longcs offset = zone.GetUtcOffset(*this).getTicksProperty();
        const longcs currentTicks = ticks();
        // Likewise, never negate offset: `-TimeSpan::MinValue` is not representable. The two
        // rearranged comparisons use only values already inside DateTime's [0, MaxTicks] range.
        if (offset > currentTicks)
            return DateTime(0LL, DateTimeKind::Utc);
        if (offset < currentTicks - MaxTicks)
            return DateTime(MaxTicks, DateTimeKind::Utc);
        return DateTime(currentTicks - offset, DateTimeKind::Utc);
    }

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
        // #1940: one body, and the one-argument overload is now the null-provider case of it.
        // That delegation is what makes "no existing result changes" a fact rather than a claim --
        // there is no second formatter left to drift.
        return ToString(format, nullptr);
    }

    std::string DateTime::ToString(const std::string& format,
                                    const System::IFormatProvider* provider) const
    {
        // DateTimeFormatInfo.cs:307-323 -- null means current, a DateTimeFormatInfo is itself,
        // anything else is asked through GetFormat.
        const System::Globalization::DateTimeFormatInfo& info =
            System::Globalization::DateTimeFormatInfo::GetInstance(provider);

        // ---------------------------------------------------------------------------------
        // #2416: THE STANDARD-FORMAT TABLE, WHICH THIS MEMBER NEVER CONSULTED.
        // ---------------------------------------------------------------------------------
        // Measured before the repair: `ToString("o")` emitted the LITERAL "o", `ToString("s")`
        // returned "0" -- reading `s` as SECONDS -- and `"%d"` rendered "%15". So a one-character
        // format was silently treated as a CUSTOM specifier, where .NET treats it as a STANDARD
        // one and requires `%d` for the custom reading.
        //
        // THE TABLE ALREADY EXISTED AND WAS SIMPLY NOT CALLED: `DateTimeFormatInfo::
        // GetAllDateTimePatterns(char)` carries all nineteen specifiers, culture-aware. So this is
        // a wiring repair rather than a new table, and it is what makes the two halves of this
        // type agree -- #2414 and #1942 gave the PARSE side its table, and until now
        // `ParseExact(x, "o")` read .NET's roundtrip pattern while `ToString("o")` emitted the
        // letter.
        std::string expanded = format;
        if (format.size() == 1) {
            // `U` requires conversion through the process-local zone before formatting. This
            // signature cannot carry one, and Core.Base cannot reach one implicitly, so reject
            // it rather than emit the unconverted wall clock that the pattern alone produces.
            if (format[0] == 'U') {
                throw FormatException(
                    "The universal full-date format requires a local time zone and is not "
                    "supported by this overload.");
            }
            static constexpr std::string_view kStandard = "dDfFgGmMoOrRstTuyY";
            if (kStandard.find(format[0]) == std::string_view::npos) {
                // .NET raises FormatException here, NOT the ArgumentException
                // `GetAllDateTimePatterns` raises -- two members, two contracts, and emitting the
                // character as a literal (which is what this did) is neither.
                throw FormatException("Format specifier was invalid.");
            }
            expanded = info.GetAllDateTimePatterns(format[0]).front();
        }
        return formatCustom(expanded, info, nullptr);
    }

    std::string DateTime::formatCustom(const std::string& format,
                                       const System::Globalization::DateTimeFormatInfo& info,
                                       const TimeSpan* offset) const
    {
        const std::array<std::string, 13> abbreviatedMonths = info.getAbbreviatedMonthNamesProperty();
        const std::array<std::string, 13> fullMonths = info.getMonthNamesProperty();
        const std::array<std::string, 7> abbreviatedDays = info.getAbbreviatedDayNamesProperty();
        const std::array<std::string, 7> fullDays = info.getDayNamesProperty();

        // `%d` is .NET's spelling for a SINGLE custom specifier, and it may occur anywhere in a
        // custom pattern. DateTimeOffset reaches this same helper, so `%z` and `%K` select its
        // one-character custom offset tokens without duplicating the rule.
        const std::string& fmt = format;

        std::string result;
        result.reserve(fmt.size() + 8);
        int yr  = getYearProperty(),   mo  = getMonthProperty(),  dy  = getDayProperty();
        int hr  = getHourProperty(),   mn  = getMinuteProperty(), sc  = getSecondProperty();
        const int fractionTicks = static_cast<int>(ticks() % TicksPerSecond);

        auto pad = [](int n, int w) -> std::string {
            std::string s = std::to_string(n);
            while (static_cast<int>(s.size()) < w) s = "0" + s;
            return s;
        };

        auto appendOffset = [&](int width) {
            // DateTimeOffset validates offsets as whole minutes within +/-14 hours before this
            // private helper can see them. Work in minutes so a negative sub-hour value keeps its
            // sign (`-00:30`), then apply the three distinct .NET custom-token widths.
            longcs totalMinutes = offset->getTicksProperty() / TimeSpan::TicksPerMinute;
            const char sign = totalMinutes < 0 ? '-' : '+';
            if (totalMinutes < 0) totalMinutes = -totalMinutes;
            const int hours = static_cast<int>(totalMinutes / 60);
            const int minutes = static_cast<int>(totalMinutes % 60);

            result += sign;
            if (width == 1) {
                result += std::to_string(hours);
            } else if (width == 2) {
                result += pad(hours, 2);
            } else if (width == 3) {
                result += pad(hours, 2);
                result += ':';
                result += pad(minutes, 2);
            } else {
                throw FormatException("Format specifier was invalid.");
            }
        };

        size_t i = 0;
        while (i < fmt.size()) {
            char c = fmt[i];
            bool forceSingleToken = false;
            if (c == '%') {
                if (i + 1 >= fmt.size() || fmt[i + 1] == '%')
                    throw FormatException("Format specifier was invalid.");
                ++i;
                c = fmt[i];
                forceSingleToken = true;
            }
            auto run = [&](char ch) {
                if (forceSingleToken) return 1;
                size_t k = i + 1;
                while (k < fmt.size() && fmt[k] == ch) ++k;
                return static_cast<int>(k - i);
            };
            if (c == 'y') {
                int n = run('y');
                result += (n >= 4) ? pad(yr, 4) : pad(yr % 100, 2);
                i += n;
            } else if (c == 'M') {
                int n = run('M');
                // THE INDEX BASE DIFFERS AND IT IS .NET'S, NOT AN OVERSIGHT. The tables this
                // replaced were 1-based with an empty slot 0; DateTimeFormatInfo's arrays are
                // 0-BASED with an empty THIRTEENTH slot, which is .NET's MonthNames convention.
                // `mo` is 1..12, so it indexes at `mo - 1`.
                if (n >= 4) result += fullMonths[static_cast<size_t>(mo) - 1];
                else if (n == 3) result += abbreviatedMonths[static_cast<size_t>(mo) - 1];
                else result += (n >= 2) ? pad(mo, 2) : std::to_string(mo);
                i += n;
            } else if (c == 'd') {
                int n = run('d');
                // Day names ARE 0-based on both sides, Sunday first, so this index is unchanged.
                const auto dayIndex = static_cast<size_t>(getDayOfWeekProperty());
                if (n >= 4) result += fullDays[dayIndex];
                else if (n == 3) result += abbreviatedDays[dayIndex];
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
                if (n > 7) throw FormatException("Format specifier was invalid.");
                std::string fraction = pad(fractionTicks, 7);
                result += fraction.substr(0, static_cast<size_t>(n));
                i += n;
            } else if (c == 'F') {
                const int n = run('F');
                if (n > 7) throw FormatException("Format specifier was invalid.");
                std::string fraction = pad(fractionTicks, 7).substr(0, static_cast<size_t>(n));
                while (!fraction.empty() && fraction.back() == '0') fraction.pop_back();
                if (fraction.empty() && !result.empty() && result.back() == '.') {
                    // .NET removes the decimal point immediately preceding an all-optional
                    // fraction. This is what makes `ss.FFFFFFF` valid XSD output at whole seconds
                    // instead of leaving a dangling dot.
                    result.pop_back();
                } else {
                    result += fraction;
                }
                i += n;
            } else if (c == 't') {
                // AM/PM, which the formatter did not have -- so `hh:mm tt` emitted a literal `tt`
                // while the PARSE side has read `t`/`tt` since #1939. Another row where the two
                // halves of one type disagreed.
                const std::size_t n = static_cast<std::size_t>(run(c));
                const std::string& designator = (hr < 12) ? info.getAMDesignatorProperty()
                                                          : info.getPMDesignatorProperty();
                if (n == 1) result += designator.empty() ? std::string() : designator.substr(0, 1);
                else result += designator;
                i += n;
            } else if (c == 'z') {
                if (offset == nullptr) {
                    // DateTime's `z` token needs the process-local offset for this value. The
                    // Core.Base signature carries no zone; emitting literal `zzz` was a
                    // plausible wrong result, so this named subset boundary fails visibly.
                    throw FormatException(
                        "The custom time-zone offset format requires a local time zone and is "
                        "not supported by DateTime.ToString in Core.Base.");
                }
                const int n = run(c);
                appendOffset(n >= 3 ? 3 : n);
                i += static_cast<std::size_t>(n);
            } else if (c == 'K') {
                // The kind marker, needed for `o` to be .NET's roundtrip pattern rather than a
                // shape. `Unspecified` emits NOTHING, which is why `K` can also match the empty
                // string on the parse side (#1942) -- the two rules are one rule.
                if (offset != nullptr) {
                    appendOffset(3);
                } else {
                    if (getKindProperty() == DateTimeKind::Utc) result += 'Z';
                    // A LOCAL value would need this process's zone, which `Core.Base` cannot name
                    // -- the same boundary #1941 phase 2 and SA-16.1 recorded, here with no
                    // parameter to carry one. It is emitted as empty rather than guessed, and
                    // that is stated in the header rather than left to be discovered.
                }
                ++i;
            } else if (c == '\'' || c == '"') {
                const char quote = c;
                ++i;
                while (i < fmt.size() && fmt[i] != quote) result += fmt[i++];
                if (i >= fmt.size()) throw FormatException("Format specifier was invalid.");
                ++i;
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
        //     W* yyyy '-' M{1,2} '-' d{1,2}
        //        [ (' '|'T') H{1,2} ':' m{1,2} ':' s{1,2} [ '.' f{1,7} ] ]
        //        [ 'Z'|'z' | ('+'|'-') offset ] W*
        //
        // Every currently-valid input still produces its exact previous value.
        // Correction/remediation (#1929 rows 5-6, approved 2026-08-01): the preceding
        // historical grammar accurately described the repaired #1879 subset, but it was
        // narrower than both the related port doors and current .NET. Outer invariant
        // whitespace, one-or-two digit clock fields, and one through seven fractional
        // digits became the approved shared contract then; internal whitespace is still
        // grammar, and still rejected.
        //
        // #1929 rows 1 and 3 (decided 2026-08-18) finish the job: the MONTH and DAY
        // admit one or two digits, and the offset is .NET's ParseTimeZone grammar. Both
        // are pure widenings -- no string that parsed yesterday parses differently or
        // fails today -- and both now live in System/detail/DateTimeTextScanner.hpp so
        // that the three doors sharing them cannot drift apart again.
        detail::DateTimeTextScanner scanner(detail::trimDateTimeText(s));
        detail::DateTimeParts        parts;
        if (!detail::takeDateTimeParts(scanner, parts)) return fail();

        // A trailing time-zone designator stays accepted, and general Parse deliberately leaves
        // the result Unspecified. DateTime now carries Kind, but Core.Base cannot obtain an
        // implicit current-zone service without reversing the TimeZone -> Core.Base dependency.
        // The explicit-zone ParseExact surface is the kind-aware conversion door; this older
        // grammar therefore keeps "…T10:20:30Z" and "…T10:20:30.123+02:00" at their historical
        // wall-clock values rather than pretending to perform the missing conversion.
        // §20.1's "unchanged in every option" list names both spellings, and
        // three DateTimeTests pin the offset one.
        //
        // #1929 row 3 (decided 2026-08-18) widens WHICH offsets are accepted, not
        // what any of them mean here: "+8", "+2:5", "+800" and "+0800" join
        // "+02:05". The grammar lives in one place now, shared with
        // DateTimeOffset, which is the door that actually uses the value.
        if (!scanner.take('Z') && !scanner.take('z')) {
            if (!scanner.atEnd()) {
                int ignoredOffsetMinutes = 0;
                if (!detail::takeUtcOffsetMinutes(scanner, ignoredOffsetMinutes))
                    return fail();
            }
        }
        if (!scanner.atEnd()) return fail();

        try {
            result = DateTime(dateToTicks(parts.year, parts.month, parts.day,
                                          parts.hour, parts.minute, parts.second) +
                              parts.fractionTicks);
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

    bool DateTime::operator==(const DateTime& o) const { return ticks() == o.ticks(); }
    bool DateTime::operator!=(const DateTime& o) const { return ticks() != o.ticks(); }
    bool DateTime::operator< (const DateTime& o) const { return ticks() <  o.ticks(); }
    bool DateTime::operator<=(const DateTime& o) const { return ticks() <= o.ticks(); }
    bool DateTime::operator> (const DateTime& o) const { return ticks() >  o.ticks(); }
    bool DateTime::operator>=(const DateTime& o) const { return ticks() >= o.ticks(); }


// ---------------------------------------------------------------------------
// ParseExact / TryParseExact -- ticket #2414
// ---------------------------------------------------------------------------

    bool DateTime::TryParseExact(const std::string& input, const std::string& format,
                                 DateTime& result) {
        return TryParseExact(input, format, nullptr, result);
    }

    bool DateTime::TryParseExact(const std::string& input, const std::string& format,
                                 const System::IFormatProvider* provider, DateTime& result) {
        return TryParseExact(input, format, provider,
                             System::Globalization::DateTimeStyles::None, result, nullptr);
    }

    bool DateTime::TryParseExact(const std::string& input, const std::string& format,
                                 const System::IFormatProvider* provider,
                                 System::Globalization::DateTimeStyles styles, DateTime& result,
                                 const System::ILocalTimeZone* zone) {
        using System::Globalization::DateTimeStyles;
        // .NET raises here rather than returning false -- a Try* method that throws. Validation
        // runs BEFORE the result is written, so a rejected style leaves the caller's variable
        // untouched; that is two claims and each has its own assertion.
        detail::ValidateDateTimeStyles(styles, "style");

        const auto has = [styles](DateTimeStyles f) {
            return (static_cast<int>(styles) & static_cast<int>(f)) != 0;
        };

        detail::ExactParseOptions options = detail::ResolveExactParseOptions(provider, styles);
        // The zone token is admitted only here. `DateOnly` and `TimeOnly` have no kind and no
        // offset to carry, so their doors leave it off -- see ExactParseOptions.
        options.allowZoneToken = true;

        result = DateTime::MinValue;

        std::string pattern = detail::ExpandStandardDateTimeFormat(format);
        if (pattern.empty()) {
            // A ONE-CHARACTER format that is not standard is not silently custom either: .NET
            // requires "%H" for a single-specifier custom format, because a bare "H" would
            // otherwise be read as a standard pattern this port does not implement. The rule is
            // #1939's and is repeated here rather than shared, because the two tables differ.
            if (format.size() == 1) return false;
            pattern = format;
        }

        detail::ExactDateTimeFields fields;
        if (!detail::MatchExactFormat(input, pattern, detail::ExactTokenSet::DateAndTime, fields,
                                      options))
            return false;

        // A DateTime format must bind a COMPLETE date. Time components default to midnight, which
        // is .NET's rule and not a convenience: `DateTime.ParseExact("2024-06-15", "yyyy-MM-dd")`
        // is a valid call returning midnight, whereas a format binding only a time has no date to
        // attach it to and NoCurrentDateDefault -- the style that decides what happens then -- is
        // #1942's, so a date-less format is refused here rather than given an invented default.
        if (fields.year < 0 || fields.month < 0 || fields.day < 0) return false;
        if (fields.year < 1 || fields.year > 9999) return false;
        if (fields.month < 1 || fields.month > 12) return false;
        if (fields.day < 1 || fields.day > DateTime::DaysInMonth(fields.year, fields.month))
            return false;

        int hour = fields.hour < 0 ? 0 : fields.hour;
        const int minute = fields.minute < 0 ? 0 : fields.minute;
        const int second = fields.second < 0 ? 0 : fields.second;

        // The 12-hour rule is the scanner's, applied here because the fields are the scanner's
        // output rather than its state: 12 AM is hour 0 and 12 PM is hour 12, so neither is
        // `+ 12` and neither is a no-op.
        if (fields.twelveHour) {
            if (hour < 1 || hour > 12) return false;
            if (fields.amPm == 1) hour = (hour == 12) ? 12 : hour + 12;
            else if (fields.amPm == 0) hour = (hour == 12) ? 0 : hour;
        }
        if (hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59)
            return false;

        DateTime candidate = DateTime::MinValue;
        try {
            candidate = DateTime(fields.year, fields.month, fields.day, hour, minute, second);
        } catch (...) {
            return false;
        }
        if (fields.fractionTicks != 0) candidate = candidate.AddTicks(fields.fractionTicks);

        // Weekday AGREEMENT, which is what makes `R` a validating format rather than a shape.
        if (fields.weekday >= 0 &&
            static_cast<int>(candidate.getDayOfWeekProperty()) != fields.weekday)
            return false;

        // -------------------------------------------------------------------------------
        // .NET's DetermineTimeZoneAdjustments (DateTimeParse.cs:2764-2839), transcribed.
        // -------------------------------------------------------------------------------
        // TWO CASES, AND THE SECOND IS NOT THE FIRST WITH A DEFAULT. When the input carried a
        // zone the Assume* styles do NOT apply at all -- .NET says so in its own comment -- and
        // when it did not, four of the five outcomes RETURN EARLY without converting anything.

        const auto requireZone = [&zone](const char* why) -> const System::ILocalTimeZone& {
            if (zone == nullptr) {
                throw System::ArgumentNullException(
                    "zone", std::string("A local time zone is required because ") + why +
                    ". Pass System::TimeZone::CurrentTimeZone(), or use a style that does not "
                    "convert.");
            }
            return *zone;
        };

        // An offset outside +-14:00 is a FAILURE rather than a clamp (DateTimeParse.cs:2776-2781).
        if (fields.hasOffset && (fields.offsetMinutes < -14 * 60 || fields.offsetMinutes > 14 * 60))
            return false;

        if (!fields.hasOffset) {
            if (has(DateTimeStyles::AssumeLocal)) {
                if (has(DateTimeStyles::AdjustToUniversal)) {
                    // Read as local, then converted -- the ONE Assume* path that needs a zone.
                    candidate = SpecifyKind(candidate, DateTimeKind::Local)
                                    .ToUniversalTime(requireZone(
                                        "AssumeLocal with AdjustToUniversal converts a local "
                                        "value to UTC"));
                } else {
                    // STAMP and return: the ticks do not move.
                    candidate = SpecifyKind(candidate, DateTimeKind::Local);
                }
            } else if (has(DateTimeStyles::AssumeUniversal)) {
                if (has(DateTimeStyles::AdjustToUniversal)) {
                    candidate = SpecifyKind(candidate, DateTimeKind::Utc);
                } else {
                    // .NET sets the offset to ZERO and falls through to the local adjustment, so
                    // an assumed-universal value WITHOUT AdjustToUniversal comes back as LOCAL --
                    // the row a reader most expects to be Utc, and it is .NET's.
                    candidate = SpecifyKind(candidate, DateTimeKind::Utc)
                                    .ToLocalTime(requireZone(
                                        "AssumeUniversal without AdjustToUniversal converts a UTC "
                                        "value to local"));
                }
            }
            // Neither Assume* style: Unspecified, and the ticks are untouched.
        } else {
            // RoundtripKind fires ONLY for a literal `Z`, .NET's ParseFlags.TimeZoneUtc -- a
            // numeric offset is not a kind, so `+00:00` does not roundtrip as Utc even though it
            // names the same instant. That distinction is invisible to any value-based test.
            if (has(DateTimeStyles::RoundtripKind) && fields.zoneIsUtc) {
                candidate = SpecifyKind(candidate, DateTimeKind::Utc);
            } else {
                // A Try* parse must report an offset that moves the value outside DateTime's
                // range as `false`, not leak AddMinutes' ArgumentOutOfRangeException. Unsigned
                // subtraction makes both the below-Min and above-Max cases well-defined and
                // lets one comparison reject them before construction.
                const longcs offsetTicks =
                    static_cast<longcs>(fields.offsetMinutes) * TimeSpan::TicksPerMinute;
                const auto utcTicks = static_cast<SharpRuntime::ulongcs>(
                                          candidate.getTicksProperty()) -
                                      static_cast<SharpRuntime::ulongcs>(offsetTicks);
                if (utcTicks > static_cast<SharpRuntime::ulongcs>(MaxTicks)) return false;
                const DateTime asUtc(static_cast<longcs>(utcTicks), DateTimeKind::Utc);
                candidate = has(DateTimeStyles::AdjustToUniversal)
                                ? asUtc
                                : asUtc.ToLocalTime(requireZone(
                                      "a zone-qualified input is converted to local time unless "
                                      "AdjustToUniversal or RoundtripKind is used"));
            }
        }

        result = candidate;
        return true;
    }

    DateTime DateTime::ParseExact(const std::string& input, const std::string& format) {
        return ParseExact(input, format, nullptr);
    }


    bool DateTime::TryParseExact(const std::string& input,
                                        const std::vector<std::string>& formats,
                                        const System::IFormatProvider* provider,
                                        System::Globalization::DateTimeStyles styles,
                                        DateTime& result,
                                        const System::ILocalTimeZone* zone) {
        // The style is validated ONCE, before the loop, so an illegal style raises whatever the
        // formats are -- including an empty collection, where no single-format call would ever
        // run. Validating inside the loop would make the exception depend on the format list.
        detail::ValidateDateTimeStyles(styles, "styles");
        result = DateTime::MinValue;
        DateTime candidate = DateTime::MinValue;
        const auto outcome = detail::MatchFirstOfManyFormats(
            input, formats, [&](const std::string& format) {
                return TryParseExact(input, format, provider, styles, candidate, zone);
            });
        // A Try* method returns false for BOTH failure kinds -- .NET does too -- so the kind is
        // observable only through ParseExact's message, which is where the caller can act on it.
        if (outcome == detail::MultiFormatOutcome::Matched) result = candidate;
        return outcome == detail::MultiFormatOutcome::Matched;
    }

    DateTime DateTime::ParseExact(const std::string& input,
                                        const std::vector<std::string>& formats,
                                        const System::IFormatProvider* provider,
                                        System::Globalization::DateTimeStyles styles,
                                        const System::ILocalTimeZone* zone) {
        detail::ValidateDateTimeStyles(styles, "styles");
        DateTime result = DateTime::MinValue;
        DateTime candidate = DateTime::MinValue;
        const auto outcome = detail::MatchFirstOfManyFormats(
            input, formats, [&](const std::string& format) {
                return TryParseExact(input, format, provider, styles, candidate, zone);
            });
        // THE TWO FAILURE KINDS GET .NET'S TWO MESSAGES. Telling a caller who supplied no formats
        // that their INPUT was not recognized is the wrong diagnosis, and it is the only thing
        // that distinguishes them -- both are FormatException and both make Try* return false.
        if (outcome == detail::MultiFormatOutcome::NoFormatSpecifier)
            throw FormatException("No format specifiers were provided.");
        if (outcome != detail::MultiFormatOutcome::Matched)
            throw FormatException("String was not recognized as a valid DateTime: " + input);
        result = candidate;
        return result;
    }

    DateTime DateTime::ParseExact(const std::string& input, const std::string& format,
                                  const System::IFormatProvider* provider,
                                  System::Globalization::DateTimeStyles styles,
                                  const System::ILocalTimeZone* zone) {
        DateTime result = DateTime::MinValue;
        // The style is validated by TryParseExact below, which raises for an illegal one exactly
        // as .NET's does; a PARSE failure is what becomes the FormatException here.
        if (!TryParseExact(input, format, provider, styles, result, zone))
            throw FormatException("String was not recognized as a valid DateTime: " + input);
        return result;
    }

    DateTime DateTime::ParseExact(const std::string& input, const std::string& format,
                                  const System::IFormatProvider* provider) {
        DateTime result = DateTime::MinValue;
        if (!TryParseExact(input, format, provider, result))
            throw FormatException("String was not recognized as a valid DateTime: " + input);
        return result;
    }

    GetTypeNameCPP(DateTime, "System::DateTime")

} // namespace System
