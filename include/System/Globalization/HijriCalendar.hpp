// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <stdexcept>
#include "System/Globalization/Calendar.hpp"

namespace System::Globalization {

/**
 * <summary>
 * Represents the Hijri (Islamic lunar) calendar.
 * 
 * Rules:
 *   - Strictly lunar; 12 months of 30/29 alternating days.
 *   - Leap years: ((year * 11) + 14) % 30 &lt; 11 — years 2,5,7,10,13,16,18,21,24,26,29
 *     of a 30-year cycle. Leap year has 355 days; common year 354 days.
 *   - Month 12 (Dhu al-Hijjah) has 30 days in a leap year, 29 otherwise.
 *   - Epoch: 1 Muharram 1 AH = absolute day 227013 (Julian 622-07-16).
 * 
 * Support range: Gregorian 0622-07-18 – 9999-12-31 (Hijri 0001 – 9666).
 * </summary>
 */
class HijriCalendar : public Calendar {
public:
    static constexpr int HijriEra         = 1;   ///< The only era value for this calendar.
    static constexpr int MaxCalendarYear  = 9666; ///< Maximum supported Hijri year.
    static constexpr int MaxCalendarMonth = 4;    ///< Last valid month in MaxCalendarYear.

    /** @return Always HijriEra (1). */
    [[nodiscard]] int GetEra(const System::DateTime& time) const override;
    /** @return Always 1 — the Hijri calendar has a single era. */
    [[nodiscard]] int GetErasCount() const override { return 1; }

    /** @return The Hijri year corresponding to @p time. */
    [[nodiscard]] int  GetYear      (const System::DateTime& time) const override;
    /** @return The Hijri month (1–12) corresponding to @p time. */
    [[nodiscard]] int  GetMonth     (const System::DateTime& time) const override;
    /** @return The day of the Hijri month corresponding to @p time. */
    [[nodiscard]] int  GetDayOfMonth(const System::DateTime& time) const override;
    /** @return The day of the Hijri year (1–355/354) corresponding to @p time. */
    [[nodiscard]] int  GetDayOfYear (const System::DateTime& time) const override;

    /** @return True if @p year is a Hijri leap year (355 days). */
    [[nodiscard]] bool IsLeapYear(int year, int era = CurrentEra) const override;
    /** @return Number of days in the given Hijri month (29 or 30). */
    [[nodiscard]] int  GetDaysInMonth(int year, int month, int era = CurrentEra) const override;
    /** @return Number of days in the given Hijri year (354 or 355). */
    [[nodiscard]] int  GetDaysInYear (int year, int era = CurrentEra) const override;

    /** @return A new DateTime offset by @p months Hijri months. */
    [[nodiscard]] System::DateTime AddMonths(const System::DateTime& time, int months) const override;
    /** @return A new DateTime offset by @p years Hijri years. */
    [[nodiscard]] System::DateTime AddYears (const System::DateTime& time, int years)  const override;

private:
    // Absolute day 1 = 0001-01-01 Gregorian (= 0001-01-03 Julian proleptic).
    // Hijri epoch absolute day: 227013 (Friday 622-07-16 Julian = 622-07-18 Gregorian proleptic).
    static constexpr long long HijriEpoch = 227013LL;

    [[nodiscard]] long long daysUpToHijriYear(int year) const;
    [[nodiscard]] long long absoluteDateHijri(int y, int m, int d) const;
    [[nodiscard]] int  getDatePart(long long ticks, int part) const;

    static constexpr int PartYear      = 0;
    static constexpr int PartDayOfYear = 1;
    static constexpr int PartMonth     = 2;
    static constexpr int PartDay       = 3;

    // Cumulative days before each month in a common year (30-29 pattern)
    static constexpr int s_monthDays[13] = {0,30,59,89,118,148,177,207,236,266,295,325,355};
};

} // namespace System::Globalization
