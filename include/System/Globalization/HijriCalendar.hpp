// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <stdexcept>
#include "System/Globalization/Calendar.hpp"

namespace System::Globalization {

/**
 * @brief Represents the Hijri (Islamic lunar) calendar.
 *
 * C++ counterpart of .NET System.Globalization.HijriCalendar.
 *
 * Rules:
 *  - Strictly lunar; 12 months of 30/29 alternating days.
 *  - Leap years: ((year * 11) + 14) % 30 < 11 — years 2,5,7,10,13,16,18,21,24,26,29
 *    of a 30-year cycle. Leap year has 355 days; common year 354 days.
 *  - Month 12 (Dhu al-Hijjah) has 30 days in a leap year, 29 otherwise.
 *  - Epoch: 1 Muharram 1 AH = absolute day 227013 (Julian 622-07-16).
 *
 * Supported range: Gregorian 0622-07-18 – 9999-12-31 (Hijri 0001 – 9666).
 */
class HijriCalendar : public Calendar {
public:
    static constexpr int HijriEra         = 1;    ///< The only era value for this calendar.
    static constexpr int MaxCalendarYear  = 9666; ///< Maximum supported Hijri year.
    static constexpr int MaxCalendarMonth = 4;    ///< Last valid month in MaxCalendarYear.

    /**
     * @brief Returns the era for the given DateTime.
     *
     * C++ counterpart of .NET HijriCalendar.GetEra(DateTime).
     * @return Always HijriEra (1).
     */
    [[nodiscard]] int GetEra(const System::DateTime& time) const override;

    /**
     * @brief Returns the number of eras in this calendar.
     *
     * @return Always 1; the Hijri calendar has a single era.
     */
    [[nodiscard]] int GetErasCount() const override { return 1; }

    /**
     * @brief Returns the Hijri year corresponding to the given DateTime.
     *
     * C++ counterpart of .NET HijriCalendar.GetYear(DateTime).
     * @param time The Gregorian DateTime to convert.
     * @return The Hijri year.
     */
    [[nodiscard]] int GetYear(const System::DateTime& time) const override;

    /**
     * @brief Returns the Hijri month corresponding to the given DateTime.
     *
     * C++ counterpart of .NET HijriCalendar.GetMonth(DateTime).
     * @param time The Gregorian DateTime to convert.
     * @return The Hijri month (1–12).
     */
    [[nodiscard]] int GetMonth(const System::DateTime& time) const override;

    /**
     * @brief Returns the day of the Hijri month corresponding to the given DateTime.
     *
     * C++ counterpart of .NET HijriCalendar.GetDayOfMonth(DateTime).
     * @param time The Gregorian DateTime to convert.
     * @return The day of the Hijri month (1–30).
     */
    [[nodiscard]] int GetDayOfMonth(const System::DateTime& time) const override;

    /**
     * @brief Returns the day of the Hijri year corresponding to the given DateTime.
     *
     * C++ counterpart of .NET HijriCalendar.GetDayOfYear(DateTime).
     * @param time The Gregorian DateTime to convert.
     * @return The day of the Hijri year (1–355).
     */
    [[nodiscard]] int GetDayOfYear(const System::DateTime& time) const override;

    /**
     * @brief Determines whether the specified Hijri year is a leap year.
     *
     * C++ counterpart of .NET HijriCalendar.IsLeapYear(int, int).
     * Leap years have 355 days; common years have 354 days.
     * @param year The Hijri year.
     * @param era  The era (default CurrentEra; ignored).
     * @return true if @p year is a Hijri leap year; otherwise false.
     */
    [[nodiscard]] bool IsLeapYear(int year, int era = CurrentEra) const override;

    /**
     * @brief Returns the number of days in the specified Hijri month.
     *
     * C++ counterpart of .NET HijriCalendar.GetDaysInMonth(int, int, int).
     * Odd months have 30 days, even months 29 days; month 12 has 30 in leap years.
     * @param year  The Hijri year.
     * @param month The Hijri month (1–12).
     * @param era   The era (default CurrentEra; ignored).
     * @return The number of days in the specified month (29 or 30).
     */
    [[nodiscard]] int GetDaysInMonth(int year, int month, int era = CurrentEra) const override;

    /**
     * @brief Returns the number of days in the specified Hijri year.
     *
     * C++ counterpart of .NET HijriCalendar.GetDaysInYear(int, int).
     * @param year The Hijri year.
     * @param era  The era (default CurrentEra; ignored).
     * @return 354 for common years, 355 for leap years.
     */
    [[nodiscard]] int GetDaysInYear(int year, int era = CurrentEra) const override;

    /**
     * @brief Returns a DateTime offset by the specified number of Hijri months.
     *
     * C++ counterpart of .NET HijriCalendar.AddMonths(DateTime, int).
     * @param time   The starting Gregorian DateTime.
     * @param months The number of Hijri months to add (may be negative).
     * @return A new DateTime offset by @p months Hijri months.
     */
    [[nodiscard]] System::DateTime AddMonths(const System::DateTime& time, int months) const override;

    /**
     * @brief Returns a DateTime offset by the specified number of Hijri years.
     *
     * C++ counterpart of .NET HijriCalendar.AddYears(DateTime, int).
     * @param time  The starting Gregorian DateTime.
     * @param years The number of Hijri years to add (may be negative).
     * @return A new DateTime offset by @p years Hijri years.
     */
    [[nodiscard]] System::DateTime AddYears(const System::DateTime& time, int years) const override;

private:
    // Absolute day 1 = 0001-01-01 Gregorian (= 0001-01-03 Julian proleptic).
    // Hijri epoch absolute day: 227013 (Friday 622-07-16 Julian = 622-07-18 Gregorian proleptic).
    static constexpr long long HijriEpoch = 227013LL;

    [[nodiscard]] long long daysUpToHijriYear(int year) const;
    [[nodiscard]] long long absoluteDateHijri(int y, int m, int d) const;
    [[nodiscard]] int getDatePart(long long ticks, int part) const;

    static constexpr int PartYear      = 0;
    static constexpr int PartDayOfYear = 1;
    static constexpr int PartMonth     = 2;
    static constexpr int PartDay       = 3;

    // Cumulative days before each month in a common year (30-29 pattern)
    static constexpr int s_monthDays[13] = {0,30,59,89,118,148,177,207,236,266,295,325,355};
};

} // namespace System::Globalization
