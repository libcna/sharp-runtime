// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Created by robertvokac on 6/7/25.
//

#pragma once

#include <string>

#include "System/Object.hpp"
#include "System/DateTime.hpp"
#include "System/DayOfWeek.hpp"
#include "System/TimeSpan.hpp"
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System {

    using SharpRuntime::intcs;
    using SharpRuntime::longcs;

    /**
     * @brief Represents a point in time, typically expressed as a date and time of day,
     * relative to Coordinated Universal Time (UTC).
     *
     * This is a partial C++ counterpart of the .NET System::DateTimeOffset structure.
     * It stores a "clock" DateTime value together with its UTC offset; the UTC instant
     * (used for identity, sorting, and subtraction) is derived as clock ticks minus offset.
     *
     * @note Status: Partial. Deviations from .NET, mirroring the deviations already
     *   documented on System::DateTime:
     *   - DateTimeKind is not tracked. The single-argument DateTime constructor and
     *     ToOffset-free implicit conversion always attach a zero offset instead of
     *     inferring it from Kind.
     *   - ToLocalTime()/LocalDateTime approximate "local" using the current system
     *     UTC offset for every instant (no historical/future DST or timezone-rule
     *     lookup), consistent with System::TimeZoneInfo's own documented limitations.
     *   - Calendar-based constructors, leap-second handling, culture-aware
     *     ToString/Parse (IFormatProvider), ParseExact/TryParseExact, span-based
     *     TryFormat, Deconstruct, and FromFileTime/ToFileTime (OLE/FILETIME
     *     conversions) are out of scope.
     */
    class DateTimeOffset : public Object {
    private:
        DateTime dateTime_;
        TimeSpan offset_;

    public:
        DateTimeOffset();
        DateTimeOffset(const DateTime& dateTime, const TimeSpan& offset);

        /**
         * @brief Initializes a new instance from a DateTime, attaching a zero UTC offset.
         *
         * C++ counterpart of .NET's implicit operator DateTimeOffset(DateTime).
         * @note Since DateTimeKind is not tracked, this always attaches TimeSpan::Zero
         *       rather than inferring the offset from Kind.
         */
        DateTimeOffset(const DateTime& dateTime);

        /**
         * @brief Initializes a new instance from a tick count and a UTC offset.
         *
         * C++ counterpart of .NET DateTimeOffset(long, TimeSpan).
         * @param ticks  Clock time expressed in 100-nanosecond ticks since the .NET epoch.
         * @param offset The time's offset from UTC.
         * @throws System::ArgumentException if @p offset is not a whole number of minutes.
         * @throws System::ArgumentOutOfRangeException if @p offset is outside ±14 hours,
         *         or if the resulting UTC time is outside DateTime's representable range.
         */
        DateTimeOffset(longcs ticks, const TimeSpan& offset);

        /**
         * @brief Initializes a new instance from date, time, and offset components.
         *
         * C++ counterpart of .NET DateTimeOffset(int, int, int, int, int, int, TimeSpan).
         * @throws System::ArgumentException if @p offset is not a whole number of minutes.
         * @throws System::ArgumentOutOfRangeException if @p offset is outside ±14 hours,
         *         or if the resulting UTC time is outside DateTime's representable range.
         */
        DateTimeOffset(intcs year, intcs month, intcs day,
                       intcs hour, intcs minute, intcs second,
                       const TimeSpan& offset);

        /**
         * @brief Initializes a new instance from date, time, millisecond, and offset components.
         *
         * C++ counterpart of .NET DateTimeOffset(int, int, int, int, int, int, int, TimeSpan).
         * @throws System::ArgumentException if @p offset is not a whole number of minutes.
         * @throws System::ArgumentOutOfRangeException if @p offset is outside ±14 hours,
         *         or if the resulting UTC time is outside DateTime's representable range.
         */
        DateTimeOffset(intcs year, intcs month, intcs day,
                       intcs hour, intcs minute, intcs second, intcs millisecond,
                       const TimeSpan& offset);

        /** @brief The smallest possible value of DateTimeOffset (0001-01-01 00:00:00 +00:00). */
        static const DateTimeOffset MinValue;
        /** @brief The largest possible value of DateTimeOffset (9999-12-31 23:59:59.9999999 +00:00). */
        static const DateTimeOffset MaxValue;
        /** @brief Represents the point in time when Unix time begins (1970-01-01 00:00:00 +00:00). */
        static const DateTimeOffset UnixEpoch;

        // Static factory
        [[nodiscard]] static DateTimeOffset getNowProperty();
        [[nodiscard]] static DateTimeOffset getUtcNowProperty();

        // Component accessors
        [[nodiscard]] const DateTime& getDateTimeProperty() const;
        [[nodiscard]] const TimeSpan& getOffsetProperty() const;
        [[nodiscard]] intcs getTotalOffsetMinutesProperty() const;
        [[nodiscard]] longcs getTicksProperty() const;
        [[nodiscard]] longcs getUtcTicksProperty() const;
        [[nodiscard]] intcs getYearProperty()        const;
        [[nodiscard]] intcs getMonthProperty()       const;
        [[nodiscard]] intcs getDayProperty()         const;
        [[nodiscard]] DayOfWeek getDayOfWeekProperty() const;
        [[nodiscard]] intcs getDayOfYearProperty()   const;
        [[nodiscard]] intcs getHourProperty()        const;
        [[nodiscard]] intcs getMinuteProperty()      const;
        [[nodiscard]] intcs getSecondProperty()      const;
        [[nodiscard]] intcs getMillisecondProperty() const;
        [[nodiscard]] TimeSpan getTimeOfDayProperty() const;
        [[nodiscard]] DateTime getDateProperty()          const;
        [[nodiscard]] DateTime getUtcDateTimeProperty()   const;
        [[nodiscard]] DateTime getLocalDateTimeProperty() const;

        /**
         * @brief Returns a DateTimeOffset that represents the same point in time as this
         * instance, but with a different UTC offset.
         *
         * C++ counterpart of .NET DateTimeOffset.ToOffset(TimeSpan).
         * @throws System::ArgumentOutOfRangeException if @p offset is outside ±14 hours,
         *         or if the resulting UTC time is outside DateTime's representable range.
         */
        [[nodiscard]] DateTimeOffset ToOffset(const TimeSpan& offset) const;

        // Arithmetic
        [[nodiscard]] DateTimeOffset Add(const TimeSpan& ts) const;
        [[nodiscard]] DateTimeOffset AddDays(double days) const;
        [[nodiscard]] DateTimeOffset AddHours(double hours) const;
        [[nodiscard]] DateTimeOffset AddMinutes(double minutes) const;
        [[nodiscard]] DateTimeOffset AddSeconds(double seconds) const;
        [[nodiscard]] DateTimeOffset AddMilliseconds(double ms) const;
        [[nodiscard]] DateTimeOffset AddTicks(longcs ticks) const;
        [[nodiscard]] DateTimeOffset AddMonths(intcs months) const;
        [[nodiscard]] DateTimeOffset AddYears(intcs years) const;
        [[nodiscard]] TimeSpan Subtract(const DateTimeOffset& other) const;
        [[nodiscard]] DateTimeOffset Subtract(const TimeSpan& ts) const;
        [[nodiscard]] DateTimeOffset ToUniversalTime() const;

        /**
         * @brief Converts this instance to local time.
         *
         * C++ counterpart of .NET DateTimeOffset.ToLocalTime().
         * @note Approximates "local" using the current system UTC offset (see class note).
         */
        [[nodiscard]] DateTimeOffset ToLocalTime() const;

        /**
         * @brief Converts a Unix time (seconds since 1970-01-01 00:00:00 UTC) to a
         * DateTimeOffset with a zero offset.
         *
         * C++ counterpart of .NET DateTimeOffset.FromUnixTimeSeconds(long).
         * @throws System::ArgumentOutOfRangeException if @p seconds is outside the
         *         representable range.
         */
        [[nodiscard]] static DateTimeOffset FromUnixTimeSeconds(longcs seconds);

        /**
         * @brief Converts a Unix time (milliseconds since 1970-01-01 00:00:00 UTC) to a
         * DateTimeOffset with a zero offset.
         *
         * C++ counterpart of .NET DateTimeOffset.FromUnixTimeMilliseconds(long).
         * @throws System::ArgumentOutOfRangeException if @p milliseconds is outside the
         *         representable range.
         */
        [[nodiscard]] static DateTimeOffset FromUnixTimeMilliseconds(longcs milliseconds);

        /**
         * @brief Returns the number of whole seconds since 1970-01-01 00:00:00 UTC
         * represented by this instance.
         *
         * C++ counterpart of .NET DateTimeOffset.ToUnixTimeSeconds().
         */
        [[nodiscard]] longcs ToUnixTimeSeconds() const;

        /**
         * @brief Returns the number of whole milliseconds since 1970-01-01 00:00:00 UTC
         * represented by this instance.
         *
         * C++ counterpart of .NET DateTimeOffset.ToUnixTimeMilliseconds().
         */
        [[nodiscard]] longcs ToUnixTimeMilliseconds() const;

        // Parsing / formatting
        [[nodiscard]] static DateTimeOffset Parse(const std::string& s);
        static bool TryParse(const std::string& s, DateTimeOffset& result);
        [[nodiscard]] std::string ToString() const override;
        [[nodiscard]] std::string ToString(const std::string& format) const;

        using Object::Equals;

        // Comparison
        [[nodiscard]] static intcs Compare(const DateTimeOffset& first, const DateTimeOffset& second);
        [[nodiscard]] static bool Equals(const DateTimeOffset& first, const DateTimeOffset& second);
        [[nodiscard]] intcs CompareTo(const DateTimeOffset& other) const;
        [[nodiscard]] bool Equals(const DateTimeOffset& other) const;

        /**
         * @brief Determines whether this instance is equal to @p other, comparing both
         * the UTC instant and the offset.
         *
         * C++ counterpart of .NET DateTimeOffset.EqualsExact(DateTimeOffset).
         */
        [[nodiscard]] bool EqualsExact(const DateTimeOffset& other) const;

        /**
         * @brief Returns a hash code for this DateTimeOffset, based on its UTC instant.
         *
         * C++ counterpart of .NET DateTimeOffset.GetHashCode().
         */
        [[nodiscard]] intcs GetHashCode() const;

        bool operator==(const DateTimeOffset& other) const;
        bool operator!=(const DateTimeOffset& other) const;
        bool operator< (const DateTimeOffset& other) const;
        bool operator<=(const DateTimeOffset& other) const;
        bool operator> (const DateTimeOffset& other) const;
        bool operator>=(const DateTimeOffset& other) const;

        // Operators
        [[nodiscard]] DateTimeOffset operator+(const TimeSpan& ts) const;
        [[nodiscard]] DateTimeOffset operator-(const TimeSpan& ts) const;
        [[nodiscard]] TimeSpan       operator-(const DateTimeOffset& other) const;

        GetTypeNameHPP()
    };

} // namespace System