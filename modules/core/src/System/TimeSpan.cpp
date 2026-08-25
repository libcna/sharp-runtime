// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Created by robertvokac on 5/30/25.
//

#include "System/TimeSpan.hpp"
#include "System/detail/ExactTimeSpanParser.hpp"
#include "System/detail/InvariantExactDateTimeParser.hpp"
#include "System/ArgumentException.hpp"

#include <iomanip>

#include "System/FormatException.hpp"
#include "System/Int64.hpp"
#include "System/OverflowException.hpp"
#include "SharpRuntime/PortableScan.hpp"
#include "System/detail/DateTimeTextScanner.hpp"

namespace System {

    std::atomic<int> TimeSpan::copy_count{0};
    std::atomic<int> TimeSpan::move_count{0};

    int TimeSpan::getCopyCount() { return copy_count.load(std::memory_order_relaxed); }
    int TimeSpan::getMoveCount() { return move_count.load(std::memory_order_relaxed); }

    void TimeSpan::resetCopyCount() {
        copy_count.store(0, std::memory_order_relaxed);
    }

    void TimeSpan::resetMoveCount() {
        move_count.store(0, std::memory_order_relaxed);
    }

    const TimeSpan TimeSpan::Zero = TimeSpan(0);
    const TimeSpan TimeSpan::MaxValue = TimeSpan(SharpRuntime::LONGCS_MAX);

    const TimeSpan TimeSpan::MinValue = TimeSpan(SharpRuntime::LONGCS_MIN);


    TimeSpan::TimeSpan() : ticks_internal(0) {
    }

    TimeSpan::TimeSpan(longcs ticks): ticks_internal(ticks) {
    }

    TimeSpan::TimeSpan(intcs hours, intcs minutes, intcs seconds): ticks_internal(
        TimeToTicks(hours, minutes, seconds)) {
    }

    TimeSpan::TimeSpan(intcs days, intcs hours, intcs minutes, intcs seconds, intcs milliseconds,
                       intcs microseconds): ticks_internal(
        TimeToTicks(days, hours, minutes, seconds, milliseconds, microseconds) * TicksPerMicrosecond) {
    }

    TimeSpan::TimeSpan(const TimeSpan& other) : ticks_internal(other.ticks_internal) {
        copy_count.fetch_add(1, std::memory_order_relaxed);
    }

    TimeSpan::TimeSpan(TimeSpan&& other) noexcept : ticks_internal(other.ticks_internal) {
        move_count.fetch_add(1, std::memory_order_relaxed);
    }

    longcs TimeSpan::TimeToTicks(intcs days, intcs hours, intcs minutes, intcs seconds, intcs milliseconds,
                                 intcs microseconds) {
        // Computed in uint64_t (defined wraparound on overflow), not signed int64_t like the
        // rest of this file: verified with a standalone UBSan repro that the previous
        // signed-arithmetic version invoked real signed-integer-overflow UB for extreme
        // component values (e.g. days == INT32_MAX) -- the microsecond scale factor here is
        // 1000x finer than TimeToTicks(hour,minute,second)'s seconds granularity below, which
        // .NET's own source comments confirm can't overflow int64; this one can. Real .NET's
        // `long` arithmetic here is unchecked and wraps silently (defined in C#), so computing
        // in unsigned and converting back to signed reproduces that exact behavior instead of
        // C++ signed overflow's undefined one -- static_cast from uint64_t to a same-width
        // signed type for an out-of-range value is well-defined (2's-complement wraparound) as
        // of C++20, which this project already requires.
        std::uint64_t totalMicroseconds =
            ((static_cast<std::uint64_t>(days) * 86400ull
            + static_cast<std::uint64_t>(hours) * 3600ull
            + static_cast<std::uint64_t>(minutes) * 60ull
            + static_cast<std::uint64_t>(seconds))
            * 1000ull + static_cast<std::uint64_t>(milliseconds)) * 1000ull
            + static_cast<std::uint64_t>(microseconds);
        longcs result = static_cast<longcs>(totalMicroseconds);
        if (result > MaxMicroSeconds || result < MinMicroSeconds)
            throw ArgumentOutOfRangeException("", "TimeSpan overflowed because the duration is too long.");
        return result;
    }

    TimeSpan &TimeSpan::operator=(const TimeSpan &other) {
        if (this != &other) {  // Prevent self-assignment
            ticks_internal = other.ticks_internal;  // Copy internal data
        }
        return *this;
    }

    TimeSpan& TimeSpan::operator=(TimeSpan&& other) noexcept {
        if (this != &other) {  // Prevent self-assignment
            ticks_internal = other.ticks_internal;  // Transfer ownership
            move_count.fetch_add(1, std::memory_order_relaxed);
        }
        return *this;
    }

    longcs TimeSpan::getTicksProperty() const {
        return ticks_internal;
    }


    [[nodiscard]] intcs TimeSpan::getDaysProperty() const { return (intcs) (ticks_internal / TicksPerDay); }


    [[nodiscard]] intcs TimeSpan::getHoursProperty() const { return (intcs) ((ticks_internal / TicksPerHour) % 24); }


    [[nodiscard]] intcs TimeSpan::getMillisecondsProperty() const {
        return (intcs) ((ticks_internal / TicksPerMillisecond) % 1000);
    }


    [[nodiscard]] intcs TimeSpan::getMicrosecondsProperty() const {
        return (intcs) ((ticks_internal / TicksPerMicrosecond) % 1000);
    }

    /**
     * @brief Retrieves the nanosecond portion of the time span.
     *
     * Provides access to the nanosecond component of the current TimeSpan instance.
     *
     * @return The number of whole nanoseconds in the time span.
     *
     * @details The `Nanoseconds` property returns only full nanoseconds, while
     * `TotalNanoseconds` provides both complete and fractional values.
     */


    [[nodiscard]] intcs TimeSpan::getNanosecondsProperty() const {
        return (intcs) ((ticks_internal % TicksPerMicrosecond) * 100);
    }


    [[nodiscard]] intcs TimeSpan::getMinutesProperty() const {
        return (intcs) ((ticks_internal / TicksPerMinute) % 60);
    }


    [[nodiscard]] intcs TimeSpan::getSecondsProperty() const {
        return (intcs) ((ticks_internal / TicksPerSecond) % 60);
    }


    [[nodiscard]] double TimeSpan::getTotalDaysProperty() const { return ((double) ticks_internal) / TicksPerDay; }


    [[nodiscard]] double TimeSpan::getTotalHoursProperty() const { return (double) ticks_internal / TicksPerHour; }


    [[nodiscard]] double TimeSpan::getTotalMillisecondsProperty() const {
        double temp = (double) ticks_internal / TicksPerMillisecond;
        if (temp > MaxMilliSeconds)
            return (double) MaxMilliSeconds;

        if (temp < MinMilliSeconds)
            return (double) MinMilliSeconds;

        return temp;
    }

    [[nodiscard]] double TimeSpan::getTotalMicrosecondsProperty() const {
        return (double) ticks_internal / TicksPerMicrosecond;
    }


    [[nodiscard]] double TimeSpan::getTotalNanosecondsProperty() const {
        return (double) ticks_internal * NanosecondsPerTick;
    }

    [[nodiscard]] double TimeSpan::getTotalMinutesProperty() const { return (double) ticks_internal / TicksPerMinute; }

    [[nodiscard]] double TimeSpan::getTotalSecondsProperty() const { return (double) ticks_internal / TicksPerSecond; }

    TimeSpan TimeSpan::Add(const TimeSpan &ts) const {
        if ((ts.ticks_internal > 0 && ticks_internal > std::numeric_limits<int64_t>::max() - ts.ticks_internal) ||
            (ts.ticks_internal < 0 && ticks_internal < std::numeric_limits<int64_t>::min() - ts.ticks_internal)) {
            throw OverflowException("TimeSpan overflowed because the duration is too long.");
        }

        return {ticks_internal + ts.ticks_internal};
    }

    intcs TimeSpan::Compare(const TimeSpan &t1, const TimeSpan &t2) {
        if (t1.ticks_internal > t2.ticks_internal) return 1;
        if (t1.ticks_internal < t2.ticks_internal) return -1;
        return 0;
    }

    intcs TimeSpan::CompareTo(const TimeSpan &value) const {
        return Compare(*this, value);
    }

    TimeSpan TimeSpan::FromDays(double value) {
        return Interval(value, TicksPerDay);
    }

    TimeSpan TimeSpan::Duration() const {
        if (getTicksProperty() == MinValue.getTicksProperty())
            throw OverflowException("The duration cannot be returned for TimeSpan.MinValue because the absolute value of TimeSpan.MinValue exceeds the value of TimeSpan.MaxValue.");
        return {ticks_internal >= 0 ? ticks_internal : -ticks_internal};
    }

    bool TimeSpan::Equals(const TimeSpan &obj) const {
        return Equals(*this, obj);
    }

    bool TimeSpan::Equals(const TimeSpan &t1, const TimeSpan &t2) {
        return t1.ticks_internal == t2.ticks_internal;
    }

    intcs TimeSpan::GetHashCode() const noexcept {
        return static_cast<intcs>(ticks_internal) ^ static_cast<intcs>(static_cast<uint64_t>(ticks_internal) >> 32);
    }

    TimeSpan TimeSpan::FromHours(double value) {
        return Interval(value, TicksPerHour);
    }

    TimeSpan TimeSpan::Interval(double value, double scale) {
        if (std::isnan(value)) {
            throw ArgumentException("TimeSpan does not accept floating point Not-a-Number values.");
        }
        return IntervalFromDoubleTicks(value * scale);
    }

    TimeSpan TimeSpan::IntervalFromDoubleTicks(double ticks) {
        if (std::isnan(ticks) || (ticks > static_cast<double>(SharpRuntime::LONGCS_MAX)) || (ticks < static_cast<double>(SharpRuntime::LONGCS_MIN)))
            throw OverflowException("TimeSpan overflowed because the duration is too long.");
        if (ticks == static_cast<double>(Int64::MaxValue))
            return MaxValue;
        return {(longcs) ticks};
    }

    TimeSpan TimeSpan::FromMilliseconds(double value) {
        return Interval(value, TicksPerMillisecond);
    }

    TimeSpan TimeSpan::FromMicroseconds(double value) {
        return Interval(value, TicksPerMicrosecond);
    }

    TimeSpan TimeSpan::FromMinutes(double value) {
        return Interval(value, TicksPerMinute);
    }

    TimeSpan TimeSpan::Negate() const {
        if (getTicksProperty() == MinValue.getTicksProperty())
            throw OverflowException("Negating the minimum value of a twos complement number is invalid.");
        return TimeSpan(-ticks_internal);
    }

    TimeSpan TimeSpan::FromSeconds(double value) {
        return Interval(value, TicksPerSecond);
    }

    TimeSpan TimeSpan::Subtract(const TimeSpan &ts) const {
        // CCF-004 class A (SR-AUD-008, ticket #1836): defined wrap, no observable change.
        // Real .NET's `TimeSpan.operator -(TimeSpan, TimeSpan)` (TimeSpan.cs:877-879) computes
        // `long result = t1._ticks - t2._ticks;` in an unchecked context and *then* tests the
        // sign bits -- the wrap is intended and the sign-bit test below is the real guard. The
        // same expression in signed C++ is undefined behaviour, measured at three public doors
        // (Subtract, operator-, and Subtract via operator-) in build-probe/1836_prefix.log
        // cases 1-3. Doing the subtraction in the unsigned counterpart type and converting
        // back reproduces C#'s two's-complement wrap with defined semantics; every returned
        // value, and the OverflowException's type and message, are unchanged.
        const longcs result = static_cast<longcs>(
            static_cast<SharpRuntime::ulongcs>(ticks_internal) -
            static_cast<SharpRuntime::ulongcs>(ts.ticks_internal));

        constexpr int signShift = sizeof(longcs) * 8 - 1;

        const bool overflow = ((ticks_internal >> signShift) != (ts.ticks_internal >> signShift)) &&
                              ((ticks_internal >> signShift) != (result >> signShift));

        if (overflow) {
            throw OverflowException("TimeSpan overflowed because the duration is too long.");
        }

        return TimeSpan(result);
    }

    TimeSpan TimeSpan::Multiply(double factor) const { return (*this) * factor; }

    TimeSpan TimeSpan::Divide(double divisor) const { return (*this) / divisor; }

    double TimeSpan::Divide(const TimeSpan &ts) const { return (*this) / ts; }

    TimeSpan TimeSpan::FromTicks(longcs value) {
        return TimeSpan(value);
    }

    longcs TimeSpan::TimeToTicks(intcs hour, intcs minute, intcs second) {
        longcs totalSeconds = (longcs) hour * 3600 + (longcs) minute * 60 + (longcs) second;
        if (totalSeconds > MaxSeconds || totalSeconds < MinSeconds)
            throw ArgumentOutOfRangeException("", "TimeSpan overflowed because the duration is too long.");
        return totalSeconds * TicksPerSecond;
    }

    std::string TimeSpan::ToString() const {
        longcs ticks = ticks_internal;
        bool negative = ticks < 0;

        std::uint64_t magnitude;
        if (negative) {
            magnitude = static_cast<std::uint64_t>(-(ticks + 1)) + 1;
        } else {
            magnitude = static_cast<std::uint64_t>(ticks);
        }

        const std::uint64_t days = magnitude / static_cast<std::uint64_t>(TicksPerDay);
        magnitude %= static_cast<std::uint64_t>(TicksPerDay);

        const std::uint64_t hours = magnitude / static_cast<std::uint64_t>(TicksPerHour);
        magnitude %= static_cast<std::uint64_t>(TicksPerHour);

        const std::uint64_t minutes = magnitude / static_cast<std::uint64_t>(TicksPerMinute);
        magnitude %= static_cast<std::uint64_t>(TicksPerMinute);

        const std::uint64_t seconds = magnitude / static_cast<std::uint64_t>(TicksPerSecond);
        magnitude %= static_cast<std::uint64_t>(TicksPerSecond);

        std::ostringstream oss;
        oss.imbue(std::locale::classic());
        if (negative) {
            oss << '-';
        }
        if (days != 0) {
            oss << days << '.'
                << std::setw(2) << std::setfill('0') << hours;
        } else {
            oss << hours;
        }
        oss << ":"
            << std::setw(2) << std::setfill('0') << minutes << ":"
            << std::setw(2) << std::setfill('0') << seconds << "."
            << std::setw(7) << std::setfill('0') << magnitude;

        return oss.str();
    }


    std::string TimeSpan::ToString(const std::string& format) const {
        longcs ticks = ticks_internal;
        bool negative = ticks < 0;
        std::uint64_t magnitude;
        if (negative) magnitude = static_cast<std::uint64_t>(-(ticks + 1)) + 1;
        else           magnitude = static_cast<std::uint64_t>(ticks);

        const auto days    = static_cast<long long>(magnitude / static_cast<std::uint64_t>(TicksPerDay));
        magnitude %= static_cast<std::uint64_t>(TicksPerDay);
        const int hours   = static_cast<int>(magnitude / static_cast<std::uint64_t>(TicksPerHour));
        magnitude %= static_cast<std::uint64_t>(TicksPerHour);
        const int minutes = static_cast<int>(magnitude / static_cast<std::uint64_t>(TicksPerMinute));
        magnitude %= static_cast<std::uint64_t>(TicksPerMinute);
        const int seconds = static_cast<int>(magnitude / static_cast<std::uint64_t>(TicksPerSecond));
        magnitude %= static_cast<std::uint64_t>(TicksPerSecond);
        // magnitude is now sub-second ticks (0–9 999 999)

        auto pad = [](long long n, int w) -> std::string {
            std::string s = std::to_string(n < 0 ? -n : n);
            while (static_cast<int>(s.size()) < w) s = "0" + s;
            return s;
        };

        std::string result;
        if (negative) result += '-';
        size_t i = 0;
        while (i < format.size()) {
            char c = format[i];
            auto run = [&](char ch) {
                size_t k = i + 1;
                while (k < format.size() && format[k] == ch) ++k;
                return static_cast<int>(k - i);
            };
            if (c == 'd') {
                int n = run('d');
                result += (n >= 2) ? pad(days, n) : std::to_string(days < 0 ? -days : days);
                i += n;
            } else if (c == 'h') {
                int n = run('h');
                result += (n >= 2) ? pad(hours, 2) : std::to_string(hours);
                i += n;
            } else if (c == 'm') {
                int n = run('m');
                result += (n >= 2) ? pad(minutes, 2) : std::to_string(minutes);
                i += n;
            } else if (c == 's') {
                int n = run('s');
                result += (n >= 2) ? pad(seconds, 2) : std::to_string(seconds);
                i += n;
            } else if (c == 'f') {
                int n = run('f');
                std::string frac = pad(static_cast<long long>(magnitude), 7);
                result += frac.substr(0, static_cast<size_t>(std::min(n, 7)));
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

    namespace {

        /**
         * @brief Outcome of the shared TimeSpan parse core.
         *
         * CCF-004 class C (SR-AUD-008, ticket #1836). Real .NET distinguishes a malformed
         * TimeSpan string (`FormatException`) from one whose numeric components cannot be
         * represented (`OverflowException`, raised by `TimeSpanParse.SetOverflowFailure`,
         * TimeSpanParse.cs:547-554). TryParse maps both to `false`; Parse needs to tell them
         * apart, so the parse body reports which it was. A file-local helper is used rather
         * than a new private member so that no declaration in TimeSpan.hpp changes.
         */
        enum class TimeSpanParseOutcome { Ok, BadFormat, Overflow };

        // Field limit copied from .NET's System/Globalization/TimeSpanParse.cs:59
        // (`private const int MaxDays = 10675199;`). .NET range-checks every component
        // *before* multiplying it up into ticks; this port did not, which is SR-AUD-008.
        constexpr long long kParseMaxDays = 10675199;

        // A run of at most 18 decimal digits always fits in `long long` (10^18 - 1 is below
        // LONGCS_MAX), so rejecting longer runs up front makes every %lld conversion below
        // in-range and therefore defined. std::sscanf's behaviour when a converted value is
        // not representable in the target object is undefined (C17 7.21.6.2p10), and it was
        // measured wrapping silently: "2147483648.00:00:00" reached the tick arithmetic as
        // -2147483648 and "99999999999999999999.00:00:00" as -1
        // (build-probe/1836_prefix.log cases 18 and 19). .NET reports the same inputs as
        // overflow ("contains too many digits").
        constexpr int kParseMaxDigitRun = 18;

        bool hasOverlongDigitRun(std::string_view s) {
            int run = 0;
            for (const char ch : s) {
                if (ch >= '0' && ch <= '9') {
                    if (++run > kParseMaxDigitRun) return true;
                } else {
                    run = 0;
                }
            }
            return false;
        }

    } // namespace

    // Internal linkage: TimeSpan.hpp is unchanged by this repair.
    static TimeSpanParseOutcome parseTimeSpanCore(const std::string& s, TimeSpan& result) {
        if (s.empty()) return TimeSpanParseOutcome::BadFormat;

        // Keep the ordinary no-whitespace path byte-for-byte direct. Only row-5 inputs
        // whose first or last byte is whitespace run the trim and allocate a temporary
        // NUL-terminated copy for the legacy sscanf-based core below.
        std::string trimmedStorage;
        std::string_view input(s);
        const char* p = s.c_str();
        if (detail::isDateTimeTextWhitespace(s.front()) ||
            detail::isDateTimeTextWhitespace(s.back())) {
            const std::string_view trimmedView = detail::trimDateTimeText(s);
            if (trimmedView.empty()) return TimeSpanParseOutcome::BadFormat;
            trimmedStorage.assign(trimmedView);
            input = trimmedStorage;
            p = trimmedStorage.c_str();
        }
        if (hasOverlongDigitRun(input)) return TimeSpanParseOutcome::Overflow;
        bool negative = (*p == '-');
        if (negative) ++p;

        // Determine if there's a days component: look for '.' before the first ':'
        const char* pScan = p;
        bool hasDot = false;
        while (*pScan && *pScan != ':') {
            if (*pScan == '.') { hasDot = true; break; }
            ++pScan;
        }

        // `long long` rather than `int`, so that a component wider than intcs is a value this
        // function can range-check instead of an undefined sscanf conversion. The digit-run
        // pre-check above guarantees every conversion is representable.
        long long days = 0, hours = 0, minutes = 0, seconds = 0;
        int matched = 0;
        if (hasDot) {
            matched = SHARP_RUNTIME_SSCANF(p, "%lld.%lld:%lld:%lld", &days, &hours, &minutes, &seconds);
            if (matched != 4) return TimeSpanParseOutcome::BadFormat;
        } else {
            matched = SHARP_RUNTIME_SSCANF(p, "%lld:%lld:%lld", &hours, &minutes, &seconds);
            if (matched != 3) return TimeSpanParseOutcome::BadFormat;
        }
        if (hours < 0 || hours > 23 || minutes < 0 || minutes > 59 || seconds < 0 || seconds > 59)
            return TimeSpanParseOutcome::BadFormat;
        // A negative day count can only come from a second sign character, as in
        // "--5.00:00:00": the leading '-' is consumed above and sscanf then reads "-5". That
        // input used to be accepted and to yield the *positive* five-day duration
        // (build-probe/1836_prefix.log case 17). Real .NET rejects it as a malformed string,
        // not as an overflow, because its tokenizer never produces a signed component.
        if (days < 0) return TimeSpanParseOutcome::BadFormat;
        // .NET's TimeSpanParse.TryTimeToTicks (TimeSpanParse.cs:595-601) rejects
        // `days._num > MaxDays` before any multiplication. Without this check
        // `days * TicksPerDay` overflows int64 for every day count above 10,675,199 --
        // the audited defect, which returned a *negative* duration for a positive input.
        if (days > kParseMaxDays) return TimeSpanParseOutcome::Overflow;

        // Find position after the seconds digits to look for fractional part
        const char* afterSec = p;
        int colons = 0;
        while (*afterSec && colons < 2) { if (*afterSec == ':') ++colons; ++afterSec; }
        while (*afterSec >= '0' && *afterSec <= '9') ++afterSec;

        long long subsecTicks = 0;
        if (*afterSec == '.') {
            ++afterSec;
            int fracDigits = 0;
            long long fracVal = 0;
            while (*afterSec >= '0' && *afterSec <= '9' && fracDigits < 7) {
                fracVal = fracVal * 10 + (*afterSec - '0');
                ++fracDigits; ++afterSec;
            }
            while (fracDigits < 7) { fracVal *= 10; ++fracDigits; }
            subsecTicks = fracVal;
        }

        // sscanf() above only checks that a matching prefix exists, not that the whole input
        // was consumed -- e.g. "12:34:56garbage" previously parsed successfully as 12:34:56,
        // silently discarding "garbage" instead of being rejected like real .NET's
        // TimeSpan.Parse (a FormatException) would. Reject any unconsumed trailing content.
        if (*afterSec != '\0') return TimeSpanParseOutcome::BadFormat;

        // CCF-004 class C. The magnitude is accumulated in the unsigned counterpart type, as
        // .NET does the equivalent unchecked (TimeSpanParse.cs:606-618), so no step here is
        // undefined. With the field ranges enforced above the largest possible magnitude is
        //   10675199*864000000000 + 23*36000000000 + 59*600000000 + 59*10000000 + 9999999
        //   == 9,223,372,799,999,999,999
        // which is far below 2^64, so the unsigned accumulation cannot wrap either. Before
        // this check the four terms overflowed int64 at four distinct columns --
        // TimeSpan.cpp:454:53 (the day product), :454:16 and :457:22 (two of the sums) and
        // :459:29 (the negation of the int64 minimum) -- all measured in
        // build-probe/1836_prefix.log.
        using SharpRuntime::ulongcs;
        const ulongcs magnitude = static_cast<ulongcs>(days)    * static_cast<ulongcs>(TimeSpan::TicksPerDay)
                                + static_cast<ulongcs>(hours)   * static_cast<ulongcs>(TimeSpan::TicksPerHour)
                                + static_cast<ulongcs>(minutes) * static_cast<ulongcs>(TimeSpan::TicksPerMinute)
                                + static_cast<ulongcs>(seconds) * static_cast<ulongcs>(TimeSpan::TicksPerSecond)
                                + static_cast<ulongcs>(subsecTicks);

        // .NET accepts one more magnitude in the negative direction than in the positive one,
        // and that asymmetry is deliberate rather than incidental: TryTimeToTicks rejects a
        // wrapped-negative result only `if (positive)`, and ProcessTerminal_* then rejects the
        // negated value only `if (ticks > 0)` (TimeSpanParse.cs:612-618, :816-822). The single
        // magnitude that survives both tests is 2^63, i.e. TimeSpan::MinValue -- which is why
        // "-10675199.02:48:05.4775808" is the canonical MinValue string and must keep parsing.
        constexpr ulongcs kMaxPositiveMagnitude = static_cast<ulongcs>(SharpRuntime::LONGCS_MAX);
        if (negative) {
            if (magnitude > kMaxPositiveMagnitude + 1u) return TimeSpanParseOutcome::Overflow;
            // Two's-complement negation in the unsigned domain; the conversion back is
            // well-defined since C++20 for every value this can produce.
            result = TimeSpan(static_cast<longcs>(static_cast<ulongcs>(0) - magnitude));
        } else {
            if (magnitude > kMaxPositiveMagnitude) return TimeSpanParseOutcome::Overflow;
            result = TimeSpan(static_cast<longcs>(magnitude));
        }
        return TimeSpanParseOutcome::Ok;
    }

    bool TimeSpan::TryParse(const std::string& s, TimeSpan& result) {
        return parseTimeSpanCore(s, result) == TimeSpanParseOutcome::Ok;
    }

    TimeSpan TimeSpan::Parse(const std::string& s) {
        TimeSpan result;
        switch (parseTimeSpanCore(s, result)) {
            case TimeSpanParseOutcome::Ok:
                return result;
            case TimeSpanParseOutcome::Overflow:
                // Real .NET's TimeSpanParse.SetOverflowFailure (TimeSpanParse.cs:547-554)
                // raises OverflowException with SR.Overflow_TimeSpanElementTooLarge, whose
                // text is reproduced here verbatim with the input substituted.
                throw OverflowException("The TimeSpan string '" + s + "' could not be parsed "
                                        "because at least one of the numeric components is out "
                                        "of range or contains too many digits.");
            case TimeSpanParseOutcome::BadFormat:
                break;
        }
        throw FormatException("String was not recognized as a valid TimeSpan: " + s);
    }

    TimeSpan TimeSpan::operator-() const {
        if (ticks_internal == MinValue.ticks_internal)
            throw OverflowException("Negating the minimum value of a twos complement number is invalid.");
        return TimeSpan(-ticks_internal);
    }

    TimeSpan TimeSpan::operator-(const TimeSpan &t2) const { return Subtract(t2); }

    TimeSpan TimeSpan::operator+() const { return *this; }

    TimeSpan TimeSpan::operator+(const TimeSpan &t2) const { return Add(t2); }

    TimeSpan &TimeSpan::operator+=(const TimeSpan &t2)
    {
        *this = Add(t2);
        return *this;
    }

    TimeSpan &TimeSpan::operator-=(const TimeSpan &t2)
    {
        *this = Subtract(t2);
        return *this;
    }


    TimeSpan TimeSpan::operator*(double factor) const {
        if (std::isnan(factor)) {
            throw ArgumentException("TimeSpan does not accept floating point Not-a-Number values.", "factor");
        }
        const double ticks = std::round(getTicksProperty() * factor);
        return IntervalFromDoubleTicks(ticks);
    }

    TimeSpan TimeSpan::operator/(double divisor) const {
        if (std::isnan(divisor)) {
            throw ArgumentException("TimeSpan does not accept floating point Not-a-Number values.", "divisor");
        }
        const double ticks = std::round(getTicksProperty() / divisor);
        return IntervalFromDoubleTicks(ticks);
    }

    // Performing division using floating-point arithmetic allows the result to be infinity,
    // which makes sense when dividing a non-zero TimeSpan by TimeSpan.Zero.
    // For example, TimeSpan.FromHours(1) / TimeSpan.Zero asks how many zero-length intervals fit into one hour,
    // and mathematically, the answer is infinity.
    // Dividing TimeSpan.Zero by TimeSpan.Zero returns NaN, which may be less practical,
    // but it is no less valid than throwing an exception.


    double TimeSpan::operator/(const TimeSpan &t2) const { return getTicksProperty() / (double) t2.getTicksProperty(); }


    bool TimeSpan::operator==(const TimeSpan &t2) const { return ticks_internal == t2.ticks_internal; }


    bool TimeSpan::operator!=(const TimeSpan &t2) const { return ticks_internal != t2.ticks_internal; }


    bool TimeSpan::operator<(const TimeSpan &t2) const { return ticks_internal < t2.ticks_internal; }


    bool TimeSpan::operator<=(const TimeSpan &t2) const { return ticks_internal <= t2.ticks_internal; }


    bool TimeSpan::operator>(const TimeSpan &t2) const { return ticks_internal > t2.ticks_internal; }


    bool TimeSpan::operator>=(const TimeSpan &t2) const { return ticks_internal >= t2.ticks_internal; }


    TimeSpan operator*(double factor, const TimeSpan &timeSpan) { return timeSpan * factor; }

// ---------------------------------------------------------------------------
// ParseExact / TryParseExact -- ticket #1943
// ---------------------------------------------------------------------------

    namespace {

        /**
         * @brief .NET's `TimeSpanStyles` validation, transcribed.
         *
         * `AssumeNegative` is the only flag, so anything outside {0, 1} is undefined.
         */
        void validateTimeSpanStyles(System::Globalization::TimeSpanStyles styles) {
            const int raw = static_cast<int>(styles);
            if (raw != 0 && raw != 1) {
                throw System::ArgumentException(
                    "An undefined TimeSpanStyles value is being used.", "styles");
            }
        }

    } // namespace

    bool TimeSpan::TryParseExact(const std::string& input, const std::string& format,
                                 TimeSpan& result) {
        return TryParseExact(input, format, nullptr,
                             System::Globalization::TimeSpanStyles::None, result);
    }

    bool TimeSpan::TryParseExact(const std::string& input, const std::string& format,
                                 const System::IFormatProvider* provider,
                                 System::Globalization::TimeSpanStyles styles, TimeSpan& result) {
        // .NET raises here rather than returning false -- a Try* method that throws. Validation
        // runs BEFORE the result is written, so a rejected style leaves the caller's variable
        // untouched; that is two claims and both are asserted.
        validateTimeSpanStyles(styles);
        (void)provider;   // the grammar reads no culture-driven token; see the header on g/G

        result = TimeSpan(static_cast<longcs>(0));

        // An EMPTY format is a bad format specifier, not merely a mismatch -- .NET's
        // `SetBadFormatSpecifierFailure` (TimeSpanParse.cs:1230-1233).
        if (format.empty()) return false;

        if (format.size() == 1) {
            switch (format[0]) {
                // `c`, `t` and `T` are the INVARIANT constant format, which is exactly what this
                // port's general `TryParse` already accepts -- so they delegate rather than grow
                // a second grammar for the same text. They also ignore `styles`, as .NET does:
                // the constant format carries its own sign.
                case 'c': case 't': case 'T':
                    return TryParse(input, result);
                // `g` and `G` are the LOCALIZED standard formats and are deliberately absent;
                // the header records why. A one-character format that is not one of the five is
                // a bad format specifier in either case.
                default:
                    return false;
            }
        }

        detail::ExactTimeSpanFields fields;
        if (!detail::MatchExactTimeSpanFormat(input, format, fields)) return false;

        // The sign comes ENTIRELY from the style, because the custom grammar has no sign token.
        const longcs sign =
            (styles == System::Globalization::TimeSpanStyles::AssumeNegative) ? -1 : 1;

        const longcs ticks =
            sign * (static_cast<longcs>(fields.days)    * TicksPerDay +
                    static_cast<longcs>(fields.hours)   * TicksPerHour +
                    static_cast<longcs>(fields.minutes) * TicksPerMinute +
                    static_cast<longcs>(fields.seconds) * TicksPerSecond +
                    static_cast<longcs>(fields.fractionTicks));

        result = TimeSpan(ticks);
        return true;
    }


    bool TimeSpan::TryParseExact(const std::string& input,
                                    const std::vector<std::string>& formats,
                                    const System::IFormatProvider* provider,
                                    System::Globalization::TimeSpanStyles styles, TimeSpan& result) {
        // The style is validated ONCE, before the loop, so an illegal style raises whatever the
        // formats are -- including an empty collection, where no single-format call would run.
        // Validating inside the loop would make the exception depend on the format list.
        TimeSpan candidate = TimeSpan(static_cast<longcs>(0));
        result = TimeSpan(static_cast<longcs>(0));
        const auto outcome = detail::MatchFirstOfManyFormats(
            input, formats, [&](const std::string& format) {
                return TryParseExact(input, format, provider, styles, candidate);
            });
        // A Try* method returns false for BOTH failure kinds -- .NET does too -- so the kind is
        // observable only through ParseExact's message, which is where the caller can act on it.
        if (outcome == detail::MultiFormatOutcome::Matched) result = candidate;
        return outcome == detail::MultiFormatOutcome::Matched;
    }

    TimeSpan TimeSpan::ParseExact(const std::string& input,
                                   const std::vector<std::string>& formats,
                                   const System::IFormatProvider* provider,
                                   System::Globalization::TimeSpanStyles styles) {
        TimeSpan result = TimeSpan(static_cast<longcs>(0));
        TimeSpan candidate = TimeSpan(static_cast<longcs>(0));
        const auto outcome = detail::MatchFirstOfManyFormats(
            input, formats, [&](const std::string& format) {
                return TryParseExact(input, format, provider, styles, candidate);
            });
        // THE TWO FAILURE KINDS GET .NET'S TWO MESSAGES. Telling a caller who supplied no formats
        // that their INPUT was not recognized is the wrong diagnosis, and it is the only thing
        // that distinguishes them -- both are FormatException and both make Try* return false.
        if (outcome == detail::MultiFormatOutcome::NoFormatSpecifier)
            throw FormatException("No format specifiers were provided.");
        if (outcome != detail::MultiFormatOutcome::Matched)
            throw FormatException("String was not recognized as a valid TimeSpan: " + input);
        result = candidate;
        return result;
    }

    TimeSpan TimeSpan::ParseExact(const std::string& input, const std::string& format,
                                  const System::IFormatProvider* provider,
                                  System::Globalization::TimeSpanStyles styles) {
        TimeSpan result;
        // The style is validated by TryParseExact below, which raises for an illegal one exactly
        // as .NET's does; a PARSE failure is what becomes the FormatException here.
        if (!TryParseExact(input, format, provider, styles, result))
            throw FormatException("String was not recognized as a valid TimeSpan: " + input);
        return result;
    }

}
