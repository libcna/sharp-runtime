// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <stdexcept>
#include "System/Globalization/Calendar.hpp"

namespace System::Globalization {

/**
 * @brief Represents the Hebrew lunisolar calendar.
 *
 * C++ counterpart of .NET System.Globalization.HebrewCalendar.
 *
 * Leap year rule: ((7 * year + 1) % 19) < 7 — years 3,6,8,11,14,17,19 of the 19-year cycle.
 * Common years have 12 months (353/354/355 days); leap years have 13 months (383/384/385 days).
 * Month 7 (Adar II) exists only in leap years.
 * Supported range: Gregorian 1583-01-01 – 2239-09-29 (Hebrew years 5343–5999).
 */
class HebrewCalendar : public Calendar {
public:
    static constexpr int HebrewEra     = 1;    ///< The only era value for this calendar.
    static constexpr int MinHebrewYear = 5343; ///< Minimum supported Hebrew year.
    static constexpr int MaxHebrewYear = 5999; ///< Maximum supported Hebrew year.

    /**
     * @brief Returns the era for the given DateTime.
     *
     * C++ counterpart of .NET HebrewCalendar.GetEra(DateTime).
     * @return Always HebrewEra (1).
     */
    [[nodiscard]] int GetEra(const System::DateTime& /*time*/) const override { return HebrewEra; }

    /**
     * @brief Returns the number of eras in this calendar.
     *
     * @return Always 1; the Hebrew calendar has a single era.
     */
    [[nodiscard]] int GetErasCount() const override { return 1; }

    /**
     * @brief Returns the Hebrew year corresponding to the given DateTime.
     *
     * C++ counterpart of .NET HebrewCalendar.GetYear(DateTime).
     * @param time The Gregorian DateTime to convert.
     * @return The Hebrew year.
     */
    [[nodiscard]] int GetYear(const System::DateTime& time) const override;

    /**
     * @brief Returns the Hebrew month corresponding to the given DateTime.
     *
     * C++ counterpart of .NET HebrewCalendar.GetMonth(DateTime).
     * @param time The Gregorian DateTime to convert.
     * @return The Hebrew month (1-based).
     */
    [[nodiscard]] int GetMonth(const System::DateTime& time) const override;

    /**
     * @brief Returns the day of the Hebrew month corresponding to the given DateTime.
     *
     * C++ counterpart of .NET HebrewCalendar.GetDayOfMonth(DateTime).
     * @param time The Gregorian DateTime to convert.
     * @return The day of the Hebrew month (1-based).
     */
    [[nodiscard]] int GetDayOfMonth(const System::DateTime& time) const override;

    /**
     * @brief Returns the day of the Hebrew year corresponding to the given DateTime.
     *
     * C++ counterpart of .NET HebrewCalendar.GetDayOfYear(DateTime).
     * @param time The Gregorian DateTime to convert.
     * @return The day of the Hebrew year (1-based).
     */
    [[nodiscard]] int GetDayOfYear(const System::DateTime& time) const override;

    /**
     * @brief Determines whether the specified Hebrew year is a leap year.
     *
     * C++ counterpart of .NET HebrewCalendar.IsLeapYear(int, int).
     * A leap year has 13 months instead of 12.
     * @param year The Hebrew year.
     * @param era  The era (default CurrentEra; ignored).
     * @return true if @p year is a Hebrew leap year; otherwise false.
     */
    [[nodiscard]] bool IsLeapYear(int year, int era = CurrentEra) const override;

    /**
     * @brief Returns the number of months in the specified Hebrew year.
     *
     * C++ counterpart of .NET HebrewCalendar.GetMonthsInYear(int, int).
     * @param year The Hebrew year.
     * @param era  The era (default CurrentEra; ignored).
     * @return 12 for common years, 13 for leap years.
     */
    [[nodiscard]] int GetMonthsInYear(int year, int era = CurrentEra) const override;

    /**
     * @brief Returns the number of days in the specified Hebrew month.
     *
     * C++ counterpart of .NET HebrewCalendar.GetDaysInMonth(int, int, int).
     * @param year  The Hebrew year.
     * @param month The Hebrew month (1-based).
     * @param era   The era (default CurrentEra; ignored).
     * @return The number of days in the specified month.
     */
    [[nodiscard]] int GetDaysInMonth(int year, int month, int era = CurrentEra) const override;

    /**
     * @brief Returns the number of days in the specified Hebrew year.
     *
     * C++ counterpart of .NET HebrewCalendar.GetDaysInYear(int, int).
     * @param year The Hebrew year.
     * @param era  The era (default CurrentEra; ignored).
     * @return The number of days (353–385) in the specified year.
     */
    [[nodiscard]] int GetDaysInYear(int year, int era = CurrentEra) const override;

    /**
     * @brief Returns a DateTime offset by the specified number of Hebrew months.
     *
     * C++ counterpart of .NET HebrewCalendar.AddMonths(DateTime, int).
     * @param time   The starting Gregorian DateTime.
     * @param months The number of Hebrew months to add (may be negative).
     * @return A new DateTime offset by @p months Hebrew months.
     */
    [[nodiscard]] System::DateTime AddMonths(const System::DateTime& time, int months) const override;

    /**
     * @brief Returns a DateTime offset by the specified number of Hebrew years.
     *
     * C++ counterpart of .NET HebrewCalendar.AddYears(DateTime, int).
     * @param time  The starting Gregorian DateTime.
     * @param years The number of Hebrew years to add (may be negative).
     * @return A new DateTime offset by @p years Hebrew years.
     */
    [[nodiscard]] System::DateTime AddYears(const System::DateTime& time, int years) const override;

private:
    static constexpr int HebrewYearOf1AD        = 3760;
    static constexpr int FirstGregorianTableYear = 1583;
    static constexpr int LastGregorianTableYear  = 2239;
    static constexpr int MaxMonthPlusOne         = 14;

    struct DateBuffer { int year = 0, month = 0, day = 0; };

    static int      getLunarMonthDay(int gregorianYear, DateBuffer& lunarDate);
    static int      getHebrewYearType(int year, int era);
    static int      getDayDifference(int lunarYearType, int m1, int d1, int m2, int d2);
    static long long getAbsoluteDate(int year, int month, int day);
    static System::DateTime hebrewToGregorian(int hy, int hm, int hd,
                                               int hour, int min, int sec, int ms);
    static int      getDatePart(long long ticks, int part);

    static constexpr int PartYear  = 0;
    static constexpr int PartMonth = 2;
    static constexpr int PartDay   = 3;
};

} // namespace System::Globalization
