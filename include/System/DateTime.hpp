// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Created by robertvokac on 5/25/25.
//

#pragma once

#include <string>

#include "System/Object.hpp"
#include "System/TimeSpan.hpp"
#include "System/DayOfWeek.hpp"
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System {

    using SharpRuntime::longcs;
    using SharpRuntime::intcs;

    /**
     * @brief Represents an instant in time, expressed as the number of 100-nanosecond
     * ticks since the .NET epoch (0001-01-01 00:00:00).
     *
     * Partial C++ counterpart of .NET System.DateTime.
     *
     * @note Status: Partial — calendar component properties (Year, Month, Day, …) and
     *   three-argument constructors were added in session 33. DateTimeKind is not stored.
     */
    class DateTime : public Object {
    public:
        static constexpr longcs TicksPerMillisecond = 10000LL;
        static constexpr longcs TicksPerSecond      = 10000000LL;
        static constexpr longcs TicksPerMinute      = 600000000LL;
        static constexpr longcs TicksPerHour        = 36000000000LL;
        static constexpr longcs TicksPerDay         = 864000000000LL;
        /// Ticks from the .NET epoch (0001-01-01) to the Unix epoch (1970-01-01).
        static constexpr longcs UnixEpochTicks      = 621355968000000000LL;

    private:
        longcs ticks_;

        /**
         * @brief Decomposes ticks_ into a UTC std::tm using the C standard library.
         *
         * Uses floor division so that pre-1970 (negative Unix-timestamp) dates are
         * decomposed correctly.
         */
        [[nodiscard]] std::tm toTm() const;

        /**
         * @brief Converts a calendar date and time to a tick count measured from the
         * .NET epoch (0001-01-01 00:00:00).
         *
         * @param year         Year (1–9999).
         * @param month        Month (1–12).
         * @param day          Day of month (1–31, validated against the given month).
         * @param hour         Hour (0–23). Default 0.
         * @param minute       Minute (0–59). Default 0.
         * @param second       Second (0–59). Default 0.
         * @param millisecond  Millisecond (0–999). Default 0.
         * @throws std::out_of_range if any component is out of the valid range.
         */
        static longcs dateToTicks(int year, int month, int day,
                                   int hour = 0, int minute = 0,
                                   int second = 0, int millisecond = 0);

    public:
        /**
         * @brief Initializes a new instance with zero ticks (0001-01-01 00:00:00).
         */
        DateTime();

        /**
         * @brief Initializes a new instance with the specified number of ticks.
         *
         * @param ticks A date and time expressed in 100-nanosecond ticks since
         *              the .NET epoch (0001-01-01 00:00:00).
         */
        explicit DateTime(longcs ticks);

        /**
         * @brief Initializes a new instance with the specified year, month, and day.
         *
         * @param year   Year (1–9999).
         * @param month  Month (1–12).
         * @param day    Day of month (1–max for the given month/year).
         * @throws std::out_of_range if any component is out of range.
         */
        DateTime(int year, int month, int day);

        /**
         * @brief Initializes a new instance with date and time components.
         *
         * @param year    Year (1–9999).
         * @param month   Month (1–12).
         * @param day     Day of month.
         * @param hour    Hour (0–23).
         * @param minute  Minute (0–59).
         * @param second  Second (0–59).
         * @throws std::out_of_range if any component is out of range.
         */
        DateTime(int year, int month, int day, int hour, int minute, int second);

        /**
         * @brief Initializes a new instance with date, time, and millisecond components.
         *
         * @param year         Year (1–9999).
         * @param month        Month (1–12).
         * @param day          Day of month.
         * @param hour         Hour (0–23).
         * @param minute       Minute (0–59).
         * @param second       Second (0–59).
         * @param millisecond  Millisecond (0–999).
         * @throws std::out_of_range if any component is out of range.
         */
        DateTime(int year, int month, int day,
                 int hour, int minute, int second, int millisecond);

        /**
         * @brief Gets the number of 100-nanosecond ticks since the .NET epoch.
         *
         * @return Tick count (0 = 0001-01-01 00:00:00).
         */
        [[nodiscard]] longcs getTicksProperty() const;

        /**
         * @brief Gets the year component of this instance (1–9999).
         */
        [[nodiscard]] int getYearProperty() const;

        /**
         * @brief Gets the month component of this instance (1–12).
         */
        [[nodiscard]] int getMonthProperty() const;

        /**
         * @brief Gets the day-of-month component of this instance (1–31).
         */
        [[nodiscard]] int getDayProperty() const;

        /**
         * @brief Gets the hour component of this instance (0–23).
         */
        [[nodiscard]] int getHourProperty() const;

        /**
         * @brief Gets the minute component of this instance (0–59).
         */
        [[nodiscard]] int getMinuteProperty() const;

        /**
         * @brief Gets the second component of this instance (0–59).
         */
        [[nodiscard]] int getSecondProperty() const;

        /**
         * @brief Gets the millisecond component of this instance (0–999).
         */
        [[nodiscard]] int getMillisecondProperty() const;

        /**
         * @brief Gets the day of the week represented by this instance.
         *
         * @return DayOfWeek enumeration value (Sunday = 0, …, Saturday = 6).
         */
        [[nodiscard]] DayOfWeek getDayOfWeekProperty() const;

        /**
         * @brief Gets the day of the year represented by this instance (1–366).
         */
        [[nodiscard]] int getDayOfYearProperty() const;

        /**
         * @brief Adds the specified time span to this instance.
         *
         * @param value Time span to add.
         * @return A new DateTime that is the sum of this instance and @p value.
         */
        [[nodiscard]] DateTime Add(const TimeSpan& value) const;

        /**
         * @brief Returns a new DateTime with the specified number of whole days added.
         *
         * @param days Number of days to add (may be negative).
         * @return A new DateTime.
         */
        [[nodiscard]] DateTime AddDays(int days) const;

        /**
         * @brief Returns a new DateTime with the specified number of hours added.
         *
         * @param hours Number of hours to add (may be negative).
         * @return A new DateTime.
         */
        [[nodiscard]] DateTime AddHours(int hours) const;

        /**
         * @brief Returns a new DateTime with the specified number of minutes added.
         *
         * @param minutes Number of minutes to add (may be negative).
         * @return A new DateTime.
         */
        [[nodiscard]] DateTime AddMinutes(int minutes) const;

        /**
         * @brief Returns a new DateTime with the specified number of seconds added.
         *
         * @param seconds Number of seconds to add (may be negative).
         * @return A new DateTime.
         */
        [[nodiscard]] DateTime AddSeconds(int seconds) const;

        /**
         * @brief Returns a new DateTime with the specified number of milliseconds added.
         *
         * @param milliseconds Number of milliseconds to add (may be negative).
         * @return A new DateTime.
         */
        [[nodiscard]] DateTime AddMilliseconds(int milliseconds) const;

        /**
         * @brief Subtracts the specified time span from this instance.
         *
         * @param value Time span to subtract.
         * @return A new DateTime that is this instance minus @p value.
         */
        [[nodiscard]] DateTime Subtract(const TimeSpan& value) const;

        /**
         * @brief Subtracts another DateTime from this instance.
         *
         * @param value The DateTime to subtract.
         * @return A TimeSpan representing the interval between the two values.
         */
        [[nodiscard]] TimeSpan Subtract(const DateTime& value) const;

        /**
         * @brief Gets the current local date and time.
         *
         * @return Current local DateTime expressed in .NET-compatible ticks.
         * @note DateTimeKind is not stored; the value reflects UTC-based system time.
         */
        [[nodiscard]] static DateTime getNowProperty();

        /**
         * @brief Gets the current date with the time component set to midnight (00:00:00).
         *
         * @return Today's date at 00:00:00.
         */
        [[nodiscard]] static DateTime getTodayProperty();

        /**
         * @brief Gets the time-of-day component of this instance as a TimeSpan.
         *
         * @return A TimeSpan representing the time elapsed since midnight.
         */
        [[nodiscard]] TimeSpan getTimeOfDayProperty() const;

        /**
         * @brief Returns an ISO-8601-style string representation: "YYYY-MM-DD HH:MM:SS".
         *
         * @return Formatted date/time string.
         */
        [[nodiscard]] std::string ToString() const override;

        /// @brief Returns the date/time formatted according to @p format.
        /// Tokens: yyyy, yy, MM, M, dd, d, HH, H, hh, h, mm, m, ss, s, fff, ff, f.
        /// Literal text can be enclosed in single quotes.
        [[nodiscard]] std::string ToString(const std::string& format) const;

        /// @brief Parses a date/time string in ISO-8601 style ("yyyy-MM-dd" or
        /// "yyyy-MM-dd HH:mm:ss" or "yyyy-MM-ddTHH:mm:ss" or with .fff suffix).
        /// @throws System::FormatException if the string cannot be parsed.
        [[nodiscard]] static DateTime Parse(const std::string& s);

        /// @brief Tries to parse a date/time string; returns false without throwing on failure.
        static bool TryParse(const std::string& s, DateTime& result);

        [[nodiscard]] bool operator==(const DateTime& other) const;
        [[nodiscard]] bool operator!=(const DateTime& other) const;
        [[nodiscard]] bool operator<(const DateTime& other)  const;
        [[nodiscard]] bool operator<=(const DateTime& other) const;
        [[nodiscard]] bool operator>(const DateTime& other)  const;
        [[nodiscard]] bool operator>=(const DateTime& other) const;
        GetTypeNameHPP()
    };

} // namespace System
