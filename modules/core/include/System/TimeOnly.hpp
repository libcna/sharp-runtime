// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include <sstream>
#include <iomanip>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/TimeSpan.hpp"
#include "System/ArgumentOutOfRangeException.hpp"

namespace System {

    class DateTime; // forward declaration for FromDateTime

    using SharpRuntime::intcs;
    using SharpRuntime::longcs;

    /**
     * @brief Represents a time of day, independent of any date.
     *
     * C++ counterpart of .NET System.TimeOnly.
     * Stores the time as hours (0–23), minutes (0–59), seconds (0–59),
     * and the 100-nanosecond ticks within the second. The four-int representation
     * preserves the pre-existing size, alignment, and field layout while retaining
     * the full precision promised by the ticks constructor and conversions.
     *
     * @note Missing surface versus real .NET's TimeOnly (all additive, none affect the behavior
     * of what's already implemented, which was verified against the reference including the
     * Add/AddTicks overflow-safe modulo formula and IsBetween's unsigned-wraparound arithmetic):
     * `Deconstruct` (5 tuple-deconstruction overloads), the `(TimeSpan/double, out int
     * wrappedDays)` overloads of Add/AddHours/AddMinutes, the 5-arg microsecond constructor
     * overload, and the whole `ParseExact`/`TryParseExact` family plus culture/`DateTimeStyles`-
     * aware `Parse`/`TryParse` overloads (this port's Parse/TryParse only accept the fixed
     * "H:M:S"/"H:M:S.fffffff" numeric subset, with one or two digits per clock field and
     * one through seven fraction digits). Add on demand rather than porting the full surface
     * speculatively.
     */
    class TimeOnly {
        intcs hour_   = 0;
        intcs minute_ = 0;
        intcs second_ = 0;
        intcs subsecondTicks_ = 0;

        static constexpr longcs TicksPerMs      = 10000LL;
        static constexpr longcs TicksPerSecond  = 10000000LL;
        static constexpr longcs TicksPerMinute  = 600000000LL;
        static constexpr longcs TicksPerHour    = 36000000000LL;
        static constexpr longcs TicksPerDay     = 864000000000LL;

    public:
        /**
         * @brief Constructs a TimeOnly representing midnight (00:00:00.000).
         *
         * C++ counterpart of .NET TimeOnly default constructor.
         */
        TimeOnly() = default;

        /**
         * @brief Constructs a TimeOnly from hours and minutes.
         *
         * C++ counterpart of .NET TimeOnly(int, int).
         * @param hour   Hour of the day (0–23).
         * @param minute Minute of the hour (0–59).
         */
        TimeOnly(intcs hour, intcs minute)
            : hour_(hour), minute_(minute) {
            validateHms(hour, minute, 0);
        }

        /**
         * @brief Constructs a TimeOnly from hours, minutes, and seconds.
         *
         * C++ counterpart of .NET TimeOnly(int, int, int).
         * @param hour   Hour of the day (0–23).
         * @param minute Minute of the hour (0–59).
         * @param second Second of the minute (0–59).
         */
        TimeOnly(intcs hour, intcs minute, intcs second)
            : hour_(hour), minute_(minute), second_(second) {
            validateHms(hour, minute, second);
        }

        /**
         * @brief Constructs a TimeOnly from hours, minutes, seconds, and milliseconds.
         *
         * C++ counterpart of .NET TimeOnly(int, int, int, int).
         * @param hour        Hour of the day (0–23).
         * @param minute      Minute of the hour (0–59).
         * @param second      Second of the minute (0–59).
         * @param millisecond Millisecond of the second (0–999).
         */
        TimeOnly(intcs hour, intcs minute, intcs second, intcs millisecond)
            : hour_(hour), minute_(minute), second_(second) {
            validateHms(hour, minute, second);
            if (static_cast<SharpRuntime::uintcs>(millisecond) >= 1000)
                throw ArgumentOutOfRangeException("millisecond", "Valid values are between 0 and 999, inclusive.");
            subsecondTicks_ = millisecond * static_cast<intcs>(TicksPerMs);
        }

        /**
         * @brief Constructs a TimeOnly from a ticks value (100-nanosecond intervals since midnight).
         *
         * C++ counterpart of .NET TimeOnly(long ticks).
         * @param ticks Number of 100-nanosecond ticks since midnight (must be in [0, TicksPerDay)).
         */
        explicit TimeOnly(longcs ticks) {
            if (static_cast<SharpRuntime::ulongcs>(ticks) >= static_cast<SharpRuntime::ulongcs>(TicksPerDay))
                throw ArgumentOutOfRangeException("ticks", "Ticks must be between 0 and and TimeOnly.MaxValue.Ticks.");
            subsecondTicks_ = static_cast<intcs>(ticks % TicksPerSecond);
            second_ = static_cast<intcs>((ticks / TicksPerSecond) % 60);
            minute_ = static_cast<intcs>((ticks / TicksPerMinute) % 60);
            hour_   = static_cast<intcs>(ticks / TicksPerHour);
        }

        // -----------------------------------------------------------------------
        // Static min/max
        // -----------------------------------------------------------------------

        /**
         * @brief Gets the earliest possible TimeOnly value (00:00:00.000).
         *
         * C++ counterpart of .NET TimeOnly.MinValue.
         */
        [[nodiscard]] static TimeOnly getMinValueProperty() { return TimeOnly(0, 0, 0, 0); }

        /**
         * @brief Gets the latest possible TimeOnly value (23:59:59.9999999).
         *
         * C++ counterpart of .NET TimeOnly.MaxValue.
         */
        [[nodiscard]] static TimeOnly getMaxValueProperty() { return TimeOnly(TicksPerDay - 1); }

        // -----------------------------------------------------------------------
        // Properties
        // -----------------------------------------------------------------------

        /**
         * @brief Gets the hour component (0–23).
         *
         * C++ counterpart of .NET TimeOnly.Hour.
         */
        [[nodiscard]] intcs getHourProperty()        const noexcept { return hour_; }

        /**
         * @brief Gets the minute component (0–59).
         *
         * C++ counterpart of .NET TimeOnly.Minute.
         */
        [[nodiscard]] intcs getMinuteProperty()      const noexcept { return minute_; }

        /**
         * @brief Gets the second component (0–59).
         *
         * C++ counterpart of .NET TimeOnly.Second.
         */
        [[nodiscard]] intcs getSecondProperty()      const noexcept { return second_; }

        /**
         * @brief Gets the millisecond component (0–999).
         *
         * C++ counterpart of .NET TimeOnly.Millisecond.
         */
        [[nodiscard]] intcs getMillisecondProperty() const noexcept {
            return subsecondTicks_ / static_cast<intcs>(TicksPerMs);
        }

        /**
         * @brief Gets the microsecond component (0–999 after the millisecond component).
         *
         * C++ counterpart of .NET TimeOnly.Microsecond.
         */
        [[nodiscard]] intcs getMicrosecondProperty() const noexcept {
            return (subsecondTicks_ / 10) % 1000;
        }

        /**
         * @brief Gets the nanosecond component (0–900 in 100ns increments).
         *
         * C++ counterpart of .NET TimeOnly.Nanosecond.
         */
        [[nodiscard]] intcs getNanosecondProperty() const noexcept {
            return (subsecondTicks_ % 10) * 100;
        }

        /**
         * @brief Gets the number of 100-nanosecond ticks since midnight.
         *
         * C++ counterpart of .NET TimeOnly.Ticks.
         */
        [[nodiscard]] longcs getTicksProperty() const noexcept {
            return static_cast<longcs>(hour_)   * TicksPerHour
                 + static_cast<longcs>(minute_) * TicksPerMinute
                 + static_cast<longcs>(second_) * TicksPerSecond
                 + static_cast<longcs>(subsecondTicks_);
        }

        // -----------------------------------------------------------------------
        // Arithmetic
        // -----------------------------------------------------------------------

        /**
         * @brief Returns a new TimeOnly that adds the given TimeSpan (wraps midnight).
         *
         * C++ counterpart of .NET TimeOnly.Add(TimeSpan).
         * Real .NET's private AddTicks(long) reduces the delta modulo TicksPerDay *before*
         * adding it to this instance's (already-bounded) ticks -- `_ticks + TicksPerDay +
         * (ticks % TicksPerDay)`. Adding the raw, unreduced TimeSpan ticks first (as this
         * method previously did) can overflow int64 for a TimeSpan near
         * TimeSpan::MaxValue/MinValue (confirmed via UBSan), since TimeSpan's tick range is
         * ~Int64 range while this instance's own ticks are bounded to under one day.
         * @param value The TimeSpan to add.
         */
        [[nodiscard]] TimeOnly Add(const TimeSpan& value) const {
            longcs deltaModDay = value.getTicksProperty() % TicksPerDay;
            longcs ticks = (getTicksProperty() + TicksPerDay + deltaModDay) % TicksPerDay;
            return TimeOnly(ticks);
        }

        /**
         * @brief Returns a new TimeOnly with @p value hours added (wraps around midnight).
         *
         * C++ counterpart of .NET TimeOnly.AddHours(double). Supports fractional hours.
         * @param value Number of hours to add (may be fractional or negative).
         */
        [[nodiscard]] TimeOnly AddHours(double value) const;

        /**
         * @brief Returns a new TimeOnly with @p value minutes added (wraps around midnight).
         *
         * C++ counterpart of .NET TimeOnly.AddMinutes(double). Supports fractional minutes.
         * @param value Number of minutes to add (may be fractional or negative).
         */
        [[nodiscard]] TimeOnly AddMinutes(double value) const;

        // -----------------------------------------------------------------------
        // Comparison / equality
        // -----------------------------------------------------------------------

        /**
         * @brief Determines whether this TimeOnly is equal to @p value.
         *
         * C++ counterpart of .NET TimeOnly.Equals(TimeOnly).
         */
        [[nodiscard]] bool Equals(const TimeOnly& value) const noexcept {
            return getTicksProperty() == value.getTicksProperty();
        }

        /**
         * @brief Compares this TimeOnly to @p value.
         *
         * C++ counterpart of .NET TimeOnly.CompareTo(TimeOnly).
         * @return Negative if earlier, zero if equal, positive if later.
         */
        [[nodiscard]] intcs CompareTo(const TimeOnly& value) const noexcept {
            const longcs a = getTicksProperty(), b = value.getTicksProperty();
            return (a < b) ? -1 : (a > b) ? 1 : 0;
        }

        /**
         * @brief Returns a hash code for this TimeOnly.
         *
         * C++ counterpart of .NET TimeOnly.GetHashCode().
         */
        [[nodiscard]] intcs GetHashCode() const noexcept {
            const auto ticks = static_cast<SharpRuntime::ulongcs>(getTicksProperty());
            return static_cast<intcs>(ticks) ^ static_cast<intcs>(ticks >> 32);
        }

        /**
         * @brief Determines whether this time falls within the range [@p start, @p end)
         * (handling midnight wrap).
         *
         * C++ counterpart of .NET TimeOnly.IsBetween(TimeOnly, TimeOnly).
         * The start is inclusive and the end is exclusive. If @p start equals
         * @p end, the elapsed time in the range is zero and this always returns
         * false. When @p start > @p end the range wraps midnight.
         *
         * @param start Inclusive start of the range.
         * @param end   Exclusive end of the range.
         */
        [[nodiscard]] bool IsBetween(const TimeOnly& start, const TimeOnly& end) const noexcept {
            // Uses unsigned wraparound arithmetic (matching .NET's ulong-tick implementation)
            // so that start==end correctly yields false regardless of `this`, and midnight-
            // wrapping ranges (start > end) work without a separate branch's edge cases.
            auto t = static_cast<SharpRuntime::ulongcs>(getTicksProperty());
            auto s = static_cast<SharpRuntime::ulongcs>(start.getTicksProperty());
            auto e = static_cast<SharpRuntime::ulongcs>(end.getTicksProperty());
            if (s <= e)
                return (t - s) < (e - s);
            return (t - e) >= (s - e);
        }

        // -----------------------------------------------------------------------
        // Conversion
        // -----------------------------------------------------------------------

        /**
         * @brief Returns a TimeOnly representing the time-of-day component of @p ts.
         *
         * C++ counterpart of .NET TimeOnly.FromTimeSpan(TimeSpan).
         */
        [[nodiscard]] static TimeOnly FromTimeSpan(const TimeSpan& ts);

        /**
         * @brief Returns a TimeSpan representing the elapsed time since midnight.
         *
         * C++ counterpart of .NET TimeOnly.ToTimeSpan().
         */
        [[nodiscard]] TimeSpan ToTimeSpan() const {
            return TimeSpan(getTicksProperty());
        }

        /**
         * @brief Extracts the time-of-day part from the specified DateTime.
         *
         * C++ counterpart of .NET TimeOnly.FromDateTime(DateTime).
         */
        [[nodiscard]] static TimeOnly FromDateTime(const DateTime& dt);

        // -----------------------------------------------------------------------
        // Formatting
        // -----------------------------------------------------------------------

        /**
         * @brief Returns the time formatted as "HH:MM:SS".
         *
         * C++ counterpart of .NET TimeOnly.ToShortTimeString() / ToString().
         */
        [[nodiscard]] std::string ToString() const {
            std::ostringstream oss;
            oss.imbue(std::locale::classic());
            oss << std::setw(2) << std::setfill('0') << hour_ << ':'
                << std::setw(2) << std::setfill('0') << minute_ << ':'
                << std::setw(2) << std::setfill('0') << second_;
            return oss.str();
        }

        /**
         * @brief Returns the time formatted as "HH:MM:SS".
         *
         * C++ counterpart of .NET TimeOnly.ToShortTimeString().
         */
        [[nodiscard]] std::string ToShortTimeString() const { return ToString(); }

        /**
         * @brief Returns the time formatted as "HH:MM:SS.fff".
         *
         * C++ counterpart of .NET TimeOnly.ToLongTimeString().
         */
        [[nodiscard]] std::string ToLongTimeString() const {
            std::ostringstream oss;
            oss.imbue(std::locale::classic());
            oss << std::setw(2) << std::setfill('0') << hour_   << ':'
                << std::setw(2) << std::setfill('0') << minute_ << ':'
                << std::setw(2) << std::setfill('0') << second_ << '.'
                << std::setw(3) << std::setfill('0') << getMillisecondProperty();
            return oss.str();
        }

        /**
         * @brief Returns the time formatted according to @p format.
         *
         * C++ counterpart of .NET TimeOnly.ToString(string).
         * Supported tokens: HH/H (24h hour), hh/h (12h hour),
         * mm/m (minute), ss/s (second), f through fffffff (100ns precision),
         * single-quoted literals.
         * @param format A format string.
         */
        [[nodiscard]] std::string ToString(const std::string& format) const;

        // -----------------------------------------------------------------------
        // Parsing
        // -----------------------------------------------------------------------

        /**
         * @brief Parses a time string with one/two-digit fields and up to seven fraction digits.
         *
         * C++ counterpart of .NET TimeOnly.Parse(string).
         * @throws System::FormatException on failure.
         */
        [[nodiscard]] static TimeOnly Parse(const std::string& s);

        /**
         * @brief Tries to parse a time string; returns false on failure.
         *
         * C++ counterpart of .NET TimeOnly.TryParse(string, out TimeOnly).
         * Accepted numeric subset: "H:M:S" through "HH:MM:SS.fffffff", with
         * optional invariant whitespace surrounding the whole input.
         * @param s      Input string.
         * @param result Receives the parsed value on success.
         * @return true on success; false on parse failure.
         */
        static bool TryParse(const std::string& s, TimeOnly& result);

        /**
         * @brief Parses @p input against exactly @p format, with invariant AM/PM and no provider.
         *
         * Ticket #1939 (#1929 row 4A). Supports the invariant standard formats `O`/`o`
         * (`HH:mm:ss.fffffff`) and `R`/`r` (`HH:mm:ss`), and a custom format naming one hour and
         * minute with `H`/`HH` or `h`/`hh` plus `m`/`mm`. Seconds `s`/`ss` are optional and
         * default to zero. A 12-hour form REQUIRES `t`/`tt`. `f`..`fffffff` needs the exact digit
         * count; `F`..`FFFFFFF` permits omitted low digits.
         *
         * Rejected: date, era and zone tokens, more than seven fractional specifiers or digits,
         * mixing 12- and 24-hour fields, duplicate fields, a 12-hour form without `t`, and any
         * leading, trailing or extra inner whitespace. The whole input must be consumed.
         *
         * @throws System::FormatException for an input mismatch or a malformed/unsupported format.
         */
        [[nodiscard]] static TimeOnly ParseExact(const std::string& input,
                                                 const std::string& format);

        /**
         * @brief Non-throwing `ParseExact`; writes `MinValue` on every failure.
         * @return true on success.
         */
        static bool TryParseExact(const std::string& input, const std::string& format,
                                  TimeOnly& result);

        // -----------------------------------------------------------------------
        // Operators
        // -----------------------------------------------------------------------

        /**
         * @brief Returns the elapsed time from @p t2 to @p t1, wrapped to always be
         * non-negative ("circular clock" semantics — e.g. 01:00 - 23:00 = 02:00, not -22:00).
         *
         * C++ counterpart of .NET TimeOnly.operator-(TimeOnly, TimeOnly).
         */
        friend TimeSpan operator-(const TimeOnly& t1, const TimeOnly& t2) {
            longcs diff = t1.getTicksProperty() - t2.getTicksProperty();
            if (diff < 0) diff += TicksPerDay;
            return TimeSpan(diff);
        }

        /** @brief Returns true if @p a and @p b represent the same time. */
        bool operator==(const TimeOnly& o) const noexcept {
            return hour_ == o.hour_ && minute_ == o.minute_ && second_ == o.second_ &&
                   subsecondTicks_ == o.subsecondTicks_;
        }
        /** @brief Returns true if @p a and @p b represent different times. */
        bool operator!=(const TimeOnly& o) const noexcept { return !(*this==o); }
        /** @brief Returns true if this is earlier than @p o. */
        bool operator< (const TimeOnly& o) const noexcept { return getTicksProperty() <  o.getTicksProperty(); }
        /** @brief Returns true if this is earlier than or equal to @p o. */
        bool operator<=(const TimeOnly& o) const noexcept { return getTicksProperty() <= o.getTicksProperty(); }
        /** @brief Returns true if this is later than @p o. */
        bool operator> (const TimeOnly& o) const noexcept { return getTicksProperty() >  o.getTicksProperty(); }
        /** @brief Returns true if this is later than or equal to @p o. */
        bool operator>=(const TimeOnly& o) const noexcept { return getTicksProperty() >= o.getTicksProperty(); }

    private:
        static void validateHms(intcs hour, intcs minute, intcs second) {
            if (static_cast<SharpRuntime::uintcs>(hour) >= 24 ||
                static_cast<SharpRuntime::uintcs>(minute) >= 60 ||
                static_cast<SharpRuntime::uintcs>(second) >= 60)
                throw ArgumentOutOfRangeException("", "Hour, Minute, and Second parameters describe an un-representable DateTime.");
        }
    };

} // namespace System
