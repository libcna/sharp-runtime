// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <algorithm>
#include <stdexcept>
#include <vector>
#include "System/DateTime.hpp"
#include "System/DayOfWeek.hpp"
#include "System/Globalization/CalendarAlgorithmType.hpp"
#include "System/Globalization/CalendarWeekRule.hpp"

namespace System::Globalization {

/**
 * @brief Represents time in divisions such as weeks, months, and years.
 *
 * C++ counterpart of .NET System.Globalization.Calendar.
 * Abstract base class for all calendar implementations. Provides virtual default
 * implementations that delegate to the Gregorian calendar rules; subclasses override
 * as needed for other calendar systems.
 */
class Calendar {
public:
    /** @brief Constant representing the current era. */
    static constexpr int CurrentEra = 0;

    /** @brief Virtual destructor for safe polymorphic destruction. */
    virtual ~Calendar() = default;

    /**
     * @brief Gets the algorithm type for this calendar.
     *
     * C++ counterpart of .NET Calendar.AlgorithmType.
     * @return CalendarAlgorithmType::SolarCalendar by default.
     */
    [[nodiscard]] virtual CalendarAlgorithmType getAlgorithmTypeProperty() const {
        return CalendarAlgorithmType::SolarCalendar;
    }

    /**
     * @brief Gets a value indicating whether this calendar is read-only.
     *
     * C++ counterpart of .NET Calendar.IsReadOnly.
     * @return Always false in the base implementation.
     */
    [[nodiscard]] virtual bool getIsReadOnlyProperty() const { return false; }

    /**
     * @brief Gets the list of era identifiers supported by this calendar.
     *
     * C++ counterpart of .NET Calendar.Eras.
     * @return A vector containing {CurrentEra}.
     */
    [[nodiscard]] virtual std::vector<int> getErasProperty() const { return {CurrentEra}; }

    /**
     * @brief Gets the earliest date and time supported by this calendar.
     *
     * C++ counterpart of .NET Calendar.MinSupportedDateTime.
     * @return DateTime(1, 1, 1) by default.
     */
    [[nodiscard]] virtual System::DateTime getMinSupportedDateTimeProperty() const {
        return System::DateTime(1, 1, 1);
    }

    /**
     * @brief Gets the latest date and time supported by this calendar.
     *
     * C++ counterpart of .NET Calendar.MaxSupportedDateTime.
     * @return DateTime(9999, 12, 31) by default.
     */
    [[nodiscard]] virtual System::DateTime getMaxSupportedDateTimeProperty() const {
        return System::DateTime(9999, 12, 31);
    }

    /**
     * @brief Returns the year component of the given DateTime in this calendar.
     *
     * C++ counterpart of .NET Calendar.GetYear(DateTime).
     * @param time The DateTime to extract the year from.
     * @return The year component.
     */
    [[nodiscard]] virtual int GetYear(const System::DateTime& time) const {
        return time.getYearProperty();
    }

    /**
     * @brief Returns the month component of the given DateTime in this calendar.
     *
     * C++ counterpart of .NET Calendar.GetMonth(DateTime).
     * @param time The DateTime to extract the month from.
     * @return The month component (1–12).
     */
    [[nodiscard]] virtual int GetMonth(const System::DateTime& time) const {
        return time.getMonthProperty();
    }

    /**
     * @brief Returns the day-of-month for the given DateTime.
     *
     * C++ counterpart of .NET Calendar.GetDayOfMonth(DateTime).
     * @param time The DateTime to query.
     * @return The day-of-month (1–31).
     */
    [[nodiscard]] virtual int GetDayOfMonth(const System::DateTime& time) const {
        return time.getDayProperty();
    }

    /**
     * @brief Returns the day-of-week for the given DateTime.
     *
     * C++ counterpart of .NET Calendar.GetDayOfWeek(DateTime).
     * @param time The DateTime to query.
     * @return The DayOfWeek value.
     */
    [[nodiscard]] virtual System::DayOfWeek GetDayOfWeek(const System::DateTime& time) const {
        return time.getDayOfWeekProperty();
    }

    /**
     * @brief Returns the day-of-year for the given DateTime.
     *
     * C++ counterpart of .NET Calendar.GetDayOfYear(DateTime).
     * @param time The DateTime to query.
     * @return The day-of-year (1–366).
     */
    [[nodiscard]] virtual int GetDayOfYear(const System::DateTime& time) const {
        return time.getDayOfYearProperty();
    }

    /**
     * @brief Returns the hour component of the given DateTime.
     *
     * C++ counterpart of .NET Calendar.GetHour(DateTime).
     * @param time The DateTime to query.
     * @return The hour component (0–23).
     */
    [[nodiscard]] virtual int GetHour(const System::DateTime& time) const {
        return time.getHourProperty();
    }

    /**
     * @brief Returns the minute component of the given DateTime.
     *
     * C++ counterpart of .NET Calendar.GetMinute(DateTime).
     * @param time The DateTime to query.
     * @return The minute component (0–59).
     */
    [[nodiscard]] virtual int GetMinute(const System::DateTime& time) const {
        return time.getMinuteProperty();
    }

    /**
     * @brief Returns the second component of the given DateTime.
     *
     * C++ counterpart of .NET Calendar.GetSecond(DateTime).
     * @param time The DateTime to query.
     * @return The second component (0–59).
     */
    [[nodiscard]] virtual int GetSecond(const System::DateTime& time) const {
        return time.getSecondProperty();
    }

    /**
     * @brief Returns the millisecond component of the given DateTime.
     *
     * C++ counterpart of .NET Calendar.GetMilliseconds(DateTime).
     * @param time The DateTime to query.
     * @return The millisecond component (0–999) as a double.
     */
    [[nodiscard]] virtual double GetMilliseconds(const System::DateTime& time) const {
        return static_cast<double>(time.getMillisecondProperty());
    }

    /**
     * @brief Returns the era for the given DateTime.
     *
     * C++ counterpart of .NET Calendar.GetEra(DateTime).
     * @param time The DateTime to query.
     * @return Always CurrentEra (0) in the base implementation.
     */
    [[nodiscard]] virtual int GetEra(const System::DateTime& /*time*/) const { return CurrentEra; }

    /**
     * @brief Returns the number of eras in this calendar.
     *
     * Helper method not present in .NET API; use getErasProperty().size() for .NET compatibility.
     * @return Always 1 in the base implementation.
     */
    [[nodiscard]] virtual int GetErasCount() const { return 1; }

    /**
     * @brief Returns the number of months in the specified year and era.
     *
     * C++ counterpart of .NET Calendar.GetMonthsInYear(int, int).
     * @param year The year.
     * @param era  The era (default CurrentEra).
     * @return Always 12 in the base (Gregorian) implementation.
     */
    [[nodiscard]] virtual int GetMonthsInYear(int /*year*/, int /*era*/ = CurrentEra) const {
        return 12;
    }

    /**
     * @brief Returns the leap month for the specified year and era, or 0 if none.
     *
     * C++ counterpart of .NET Calendar.GetLeapMonth(int, int).
     * @param year The year.
     * @param era  The era (default CurrentEra).
     * @return Always 0 in the base implementation (no leap month in Gregorian).
     */
    [[nodiscard]] virtual int GetLeapMonth(int /*year*/, int /*era*/ = CurrentEra) const {
        return 0;
    }

    /**
     * @brief Determines whether the specified year is a leap year.
     *
     * C++ counterpart of .NET Calendar.IsLeapYear(int, int).
     * @param year The year to check.
     * @param era  The era (default CurrentEra).
     * @return true if @p year is a leap year; otherwise false.
     */
    [[nodiscard]] virtual bool IsLeapYear(int year, int /*era*/ = CurrentEra) const {
        return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    }

    /**
     * @brief Determines whether the specified month in the specified year is a leap month.
     *
     * C++ counterpart of .NET Calendar.IsLeapMonth(int, int, int).
     * @param year  The year.
     * @param month The month (1–12).
     * @param era   The era (default CurrentEra).
     * @return Always false in the base (Gregorian) implementation.
     */
    [[nodiscard]] virtual bool IsLeapMonth(int /*year*/, int /*month*/, int /*era*/ = CurrentEra) const {
        return false;
    }

    /**
     * @brief Determines whether the specified date is a leap day.
     *
     * C++ counterpart of .NET Calendar.IsLeapDay(int, int, int, int).
     * @param year  The year.
     * @param month The month (1–12).
     * @param day   The day (1–31).
     * @param era   The era (default CurrentEra).
     * @return true if the date is February 29 in a leap year; otherwise false.
     */
    [[nodiscard]] virtual bool IsLeapDay(int year, int month, int day, int era = CurrentEra) const {
        return month == 2 && day == 29 && IsLeapYear(year, era);
    }

    /**
     * @brief Returns the number of days in the specified month, year, and era.
     *
     * C++ counterpart of .NET Calendar.GetDaysInMonth(int, int, int).
     * @param year  The year.
     * @param month The month (1–12).
     * @param era   The era (default CurrentEra).
     * @return The number of days in the specified month.
     */
    [[nodiscard]] virtual int GetDaysInMonth(int year, int month, int /*era*/ = CurrentEra) const {
        static const int days[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        if (month == 2 && IsLeapYear(year)) return 29;
        return days[month];
    }

    /**
     * @brief Returns the number of days in the specified year and era.
     *
     * C++ counterpart of .NET Calendar.GetDaysInYear(int, int).
     * @param year The year.
     * @param era  The era (default CurrentEra).
     * @return 366 for leap years, 365 otherwise.
     */
    [[nodiscard]] virtual int GetDaysInYear(int year, int era = CurrentEra) const {
        return IsLeapYear(year, era) ? 366 : 365;
    }

    /**
     * @brief Returns the week-of-year for the given DateTime using the specified rule.
     *
     * C++ counterpart of .NET Calendar.GetWeekOfYear(DateTime, CalendarWeekRule, DayOfWeek).
     * @param time           The DateTime to query.
     * @param rule           The rule that defines the first week of the year.
     * @param firstDayOfWeek The first day of the week.
     * @return The week number (1-based).
     */
    [[nodiscard]] virtual int GetWeekOfYear(const System::DateTime& time,
                                             CalendarWeekRule /*rule*/,
                                             System::DayOfWeek /*firstDayOfWeek*/) const {
        return time.getDayOfYearProperty() / 7 + 1;
    }

    /**
     * @brief Returns a DateTime that is the specified number of years from the given DateTime.
     *
     * C++ counterpart of .NET Calendar.AddYears(DateTime, int).
     * @param time  The starting DateTime.
     * @param years The number of years to add.
     * @return A new DateTime offset by @p years.
     */
    virtual System::DateTime AddYears(const System::DateTime& time, int years) const {
        return System::DateTime(time.getYearProperty() + years,
                                time.getMonthProperty(),
                                time.getDayProperty());
    }

    /**
     * @brief Returns a DateTime that is the specified number of months from the given DateTime.
     *
     * C++ counterpart of .NET Calendar.AddMonths(DateTime, int).
     * @param time   The starting DateTime.
     * @param months The number of months to add.
     * @return A new DateTime offset by @p months.
     */
    virtual System::DateTime AddMonths(const System::DateTime& time, int months) const {
        int totalMonths = (time.getYearProperty() - 1) * 12 + (time.getMonthProperty() - 1) + months;
        int year  = totalMonths / 12 + 1;
        int month = totalMonths % 12 + 1;
        int day   = std::min(time.getDayProperty(), GetDaysInMonth(year, month));
        return System::DateTime(year, month, day,
                                time.getHourProperty(), time.getMinuteProperty(),
                                time.getSecondProperty());
    }

    /**
     * @brief Returns a DateTime that is the specified number of weeks from the given DateTime.
     *
     * C++ counterpart of .NET Calendar.AddWeeks(DateTime, int).
     * @param time  The starting DateTime.
     * @param weeks The number of weeks to add.
     * @return A new DateTime offset by @p weeks * 7 days.
     */
    virtual System::DateTime AddWeeks(const System::DateTime& time, int weeks) const {
        return time.AddDays(weeks * 7);
    }

    /**
     * @brief Returns a DateTime that is the specified number of days from the given DateTime.
     *
     * C++ counterpart of .NET Calendar.AddDays(DateTime, int).
     * @param time The starting DateTime.
     * @param days The number of days to add.
     * @return A new DateTime offset by @p days.
     */
    virtual System::DateTime AddDays(const System::DateTime& time, int days) const {
        return time.AddDays(days);
    }

    /**
     * @brief Returns a DateTime that is the specified number of hours from the given DateTime.
     *
     * C++ counterpart of .NET Calendar.AddHours(DateTime, int).
     * @param time  The starting DateTime.
     * @param hours The number of hours to add.
     * @return A new DateTime offset by @p hours.
     */
    virtual System::DateTime AddHours(const System::DateTime& time, int hours) const {
        return time.AddHours(hours);
    }

    /**
     * @brief Returns a DateTime that is the specified number of minutes from the given DateTime.
     *
     * C++ counterpart of .NET Calendar.AddMinutes(DateTime, int).
     * @param time    The starting DateTime.
     * @param minutes The number of minutes to add.
     * @return A new DateTime offset by @p minutes.
     */
    virtual System::DateTime AddMinutes(const System::DateTime& time, int minutes) const {
        return time.AddMinutes(minutes);
    }

    /**
     * @brief Returns a DateTime that is the specified number of seconds from the given DateTime.
     *
     * C++ counterpart of .NET Calendar.AddSeconds(DateTime, int).
     * @param time    The starting DateTime.
     * @param seconds The number of seconds to add.
     * @return A new DateTime offset by @p seconds.
     */
    virtual System::DateTime AddSeconds(const System::DateTime& time, int seconds) const {
        return time.AddSeconds(seconds);
    }

    /**
     * @brief Returns a DateTime that is the specified number of milliseconds from the given DateTime.
     *
     * C++ counterpart of .NET Calendar.AddMilliseconds(DateTime, double).
     * @param time         The starting DateTime.
     * @param milliseconds The number of milliseconds to add.
     * @return A new DateTime offset by @p milliseconds.
     */
    virtual System::DateTime AddMilliseconds(const System::DateTime& time, double milliseconds) const {
        return time.AddMilliseconds(static_cast<long long>(milliseconds));
    }

    /**
     * @brief Returns a DateTime from the individual date and time components.
     *
     * C++ counterpart of .NET Calendar.ToDateTime(int, int, int, int, int, int, int, int).
     * @param year        The year.
     * @param month       The month (1–12).
     * @param day         The day (1–31).
     * @param hour        The hour (0–23).
     * @param minute      The minute (0–59).
     * @param second      The second (0–59).
     * @param millisecond The millisecond (0–999).
     * @param era         The era (default CurrentEra; ignored in base implementation).
     * @return The DateTime corresponding to the given components.
     */
    virtual System::DateTime ToDateTime(int year, int month, int day,
                                        int hour, int minute, int second,
                                        int millisecond, int /*era*/ = CurrentEra) const {
        return System::DateTime(year, month, day, hour, minute, second, millisecond);
    }

    /**
     * @brief Converts a two-digit year to a four-digit year.
     *
     * C++ counterpart of .NET Calendar.ToFourDigitYear(int).
     * Years 0–99 are mapped to the current century; years >= 100 are returned as-is.
     * @param year The year to convert.
     * @return A four-digit year.
     */
    virtual int ToFourDigitYear(int year) const {
        if (year >= 100) return year;
        int currentYear = getMaxSupportedDateTimeProperty().getYearProperty();
        int century = (currentYear / 100) * 100;
        return century + year;
    }
};

} // namespace System::Globalization
