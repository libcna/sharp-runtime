// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <stdexcept>
#include "System/Globalization/Calendar.hpp"

namespace System::Globalization {

/// <summary>
/// Represents the Hebrew lunisolar calendar.
///
/// Rules:
///   - Leap years: ((7 * year + 1) % 19) &lt; 7  — years 3,6,8,11,14,17,19 of the 19-year cycle.
///   - Common years: 12 months, 353/354/355 days.
///   - Leap years: 13 months, 383/384/385 days.
///   - Month 7 (Adar II) exists only in leap years.
///
/// Support range: Gregorian 1583-01-01 – 2239-09-29 (Hebrew 5343–5999).
/// </summary>
class HebrewCalendar : public Calendar {
public:
    static constexpr int HebrewEra         = 1;
    static constexpr int MinHebrewYear     = 5343;
    static constexpr int MaxHebrewYear     = 5999;

    [[nodiscard]] int GetEra(const System::DateTime& /*time*/) const override { return HebrewEra; }
    [[nodiscard]] int GetErasCount() const override { return 1; }

    [[nodiscard]] int  GetYear      (const System::DateTime& time) const override;
    [[nodiscard]] int  GetMonth     (const System::DateTime& time) const override;
    [[nodiscard]] int  GetDayOfMonth(const System::DateTime& time) const override;
    [[nodiscard]] int  GetDayOfYear (const System::DateTime& time) const override;

    [[nodiscard]] bool IsLeapYear   (int year, int era = CurrentEra) const override;
    [[nodiscard]] int  GetMonthsInYear(int year, int era = CurrentEra) const;
    [[nodiscard]] int  GetDaysInMonth (int year, int month, int era = CurrentEra) const override;
    [[nodiscard]] int  GetDaysInYear  (int year, int era = CurrentEra) const override;

    [[nodiscard]] System::DateTime AddMonths(const System::DateTime& time, int months) const override;
    [[nodiscard]] System::DateTime AddYears (const System::DateTime& time, int years)  const override;

private:
    static constexpr int HebrewYearOf1AD          = 3760;
    static constexpr int FirstGregorianTableYear   = 1583;
    static constexpr int LastGregorianTableYear    = 2239;
    static constexpr int MaxMonthPlusOne           = 14;

    struct DateBuffer { int year = 0, month = 0, day = 0; };

    static int  getLunarMonthDay(int gregorianYear, DateBuffer& lunarDate);
    static int  getHebrewYearType(int year, int era);
    static int  getDayDifference(int lunarYearType, int m1, int d1, int m2, int d2);
    static long long getAbsoluteDate(int year, int month, int day);
    static System::DateTime hebrewToGregorian(int hy, int hm, int hd, int hour, int min, int sec, int ms);
    static int  getDatePart(long long ticks, int part);

    static constexpr int PartYear  = 0;
    static constexpr int PartMonth = 2;
    static constexpr int PartDay   = 3;
};

} // namespace System::Globalization
