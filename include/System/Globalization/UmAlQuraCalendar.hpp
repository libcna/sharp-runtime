// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <stdexcept>
#include "System/Globalization/Calendar.hpp"

namespace System::Globalization {

/// <summary>
/// Represents the Um Al Qura calendar used in Saudi Arabia.
/// Uses a pre-computed table of month lengths for Hijri years 1318–1500.
/// Support range: Gregorian 1900-04-30 – 2077-11-16.
/// </summary>
class UmAlQuraCalendar : public Calendar {
public:
    static constexpr int UmAlQuraEra     = 1;
    static constexpr int MinCalendarYear = 1318;
    static constexpr int MaxCalendarYear = 1500;

    [[nodiscard]] int GetEra(const System::DateTime& time) const override;
    [[nodiscard]] int GetErasCount() const override { return 1; }

    [[nodiscard]] int  GetYear      (const System::DateTime& time) const override;
    [[nodiscard]] int  GetMonth     (const System::DateTime& time) const override;
    [[nodiscard]] int  GetDayOfMonth(const System::DateTime& time) const override;
    [[nodiscard]] int  GetDayOfYear (const System::DateTime& time) const override;

    [[nodiscard]] bool IsLeapYear   (int year, int era = CurrentEra) const override;
    [[nodiscard]] int  GetDaysInMonth (int year, int month, int era = CurrentEra) const override;
    [[nodiscard]] int  GetDaysInYear  (int year, int era = CurrentEra) const override;

    [[nodiscard]] System::DateTime AddMonths(const System::DateTime& time, int months) const override;
    [[nodiscard]] System::DateTime AddYears (const System::DateTime& time, int years)  const override;

private:
    struct DateMapping {
        int  flags;       // bit i set → month (i+1) has 30 days; else 29 days
        int  gYear, gMonth, gDay; // Gregorian start date of this Hijri year
    };

    static const DateMapping s_yearInfo[184]; // indices 0..183 → Hijri 1318..1501

    static void checkYear(int year, int era);
    static void checkYearMonth(int year, int month, int era);
    static long long absoluteDate(int y, int m, int d);
    static int  daysInYear(int year);

    static void gregorianToHijri(const System::DateTime& time,
                                  int& hy, int& hm, int& hd);
    static void hijriToGregorian(int hy, int hm, int hd,
                                  int& gy, int& gm, int& gd);
    static long long absoluteDateUAQ(int year, int month, int day);

    static constexpr int PartYear      = 0;
    static constexpr int PartDayOfYear = 1;
    static constexpr int PartMonth     = 2;
    static constexpr int PartDay       = 3;

    int getDatePart(const System::DateTime& time, int part) const;
};

} // namespace System::Globalization
