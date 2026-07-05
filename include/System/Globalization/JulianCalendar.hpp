// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Globalization/Calendar.hpp"

namespace System::Globalization {

/**
 * @brief Represents the Julian calendar.
 *
 * C++ counterpart of .NET System.Globalization.JulianCalendar.
 * In the Julian calendar every year divisible by 4 is a leap year;
 * there is no century exception (unlike Gregorian). This produces a drift
 * of approximately one day every 128 years relative to the solar year.
 */
class JulianCalendar : public Calendar {
public:
    static constexpr int JulianEra = 1; ///< The only era value for this calendar.

    /**
     * @brief Gets the algorithm type for this calendar.
     *
     * C++ counterpart of .NET JulianCalendar.AlgorithmType.
     * @return Always CalendarAlgorithmType::SolarCalendar.
     */
    [[nodiscard]] CalendarAlgorithmType getAlgorithmTypeProperty() const override {
        return CalendarAlgorithmType::SolarCalendar;
    }

    /**
     * @brief Gets the earliest date supported by JulianCalendar.
     *
     * C++ counterpart of .NET JulianCalendar.MinSupportedDateTime.
     * @return DateTime(1, 1, 1).
     */
    [[nodiscard]] System::DateTime getMinSupportedDateTimeProperty() const override {
        return System::DateTime(1, 1, 1);
    }

    /**
     * @brief Gets the latest date supported by JulianCalendar.
     *
     * C++ counterpart of .NET JulianCalendar.MaxSupportedDateTime.
     * @return DateTime(9999, 12, 31).
     */
    [[nodiscard]] System::DateTime getMaxSupportedDateTimeProperty() const override {
        return System::DateTime(9999, 12, 31);
    }

    /**
     * @brief Gets the last two-digit year that maps into the range of this calendar.
     *
     * C++ counterpart of .NET JulianCalendar.TwoDigitYearMax.
     * @return The maximum two-digit year (default 2029).
     */
    [[nodiscard]] int getTwoDigitYearMaxProperty() const { return twoDigitYearMax_; }

    /**
     * @brief Sets the last two-digit year that maps into the range of this calendar.
     *
     * C++ counterpart of .NET JulianCalendar.TwoDigitYearMax setter.
     * @param value The new maximum two-digit year.
     */
    void setTwoDigitYearMaxProperty(int value) { twoDigitYearMax_ = value; }

    /**
     * @brief Returns the era for the given DateTime.
     *
     * C++ counterpart of .NET JulianCalendar.GetEra(DateTime).
     * @return Always JulianEra (1).
     */
    [[nodiscard]] int GetEra(const System::DateTime& /*time*/) const override { return JulianEra; }

    /**
     * @brief Returns the number of eras in this calendar.
     *
     * @return Always 1; the Julian calendar has a single era.
     */
    [[nodiscard]] int GetErasCount() const override { return 1; }

    /**
     * @brief Determines whether the specified year is a Julian leap year.
     *
     * C++ counterpart of .NET JulianCalendar.IsLeapYear(int, int).
     * Every year divisible by 4 is a leap year (no century exception).
     * @param year The year to check.
     * @param era  The era (ignored; always Julian).
     * @return true if @p year is divisible by 4; otherwise false.
     */
    [[nodiscard]] bool IsLeapYear(int year, int /*era*/ = CurrentEra) const override {
        return year % 4 == 0;
    }

    /**
     * @brief Returns the number of days in the specified Julian month.
     *
     * C++ counterpart of .NET JulianCalendar.GetDaysInMonth(int, int, int).
     * Uses the Julian leap year rule: every year divisible by 4 gives February 29 days.
     * @param year  The year.
     * @param month The month (1–12).
     * @param era   The era (ignored).
     * @return The number of days in the specified month.
     */
    [[nodiscard]] int GetDaysInMonth(int year, int month, int /*era*/ = CurrentEra) const override {
        static const int days[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        if (month == 2 && IsLeapYear(year)) return 29;
        return days[month];
    }

private:
    int twoDigitYearMax_{2029};
};

} // namespace System::Globalization
