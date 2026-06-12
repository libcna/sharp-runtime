// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <stdexcept>
#include "System/Globalization/Calendar.hpp"

namespace System::Globalization {

/// <summary>Represents the Persian (Solar Hijri) calendar.</summary>
class PersianCalendar : public Calendar {
public:
    static constexpr int PersianEra = 1;

    [[nodiscard]] int GetEra(const System::DateTime& /*time*/) const override { return PersianEra; }
    [[nodiscard]] int GetErasCount() const override { return 1; }

    /// <summary>Converts a Gregorian DateTime to Persian year.</summary>
    [[nodiscard]] int GetYear(const System::DateTime& time) const override {
        int g_y = time.getYearProperty(), g_m = time.getMonthProperty(), g_d = time.getDayProperty();
        return gregorianToPersian(g_y, g_m, g_d).year;
    }

    [[nodiscard]] int GetMonth(const System::DateTime& time) const override {
        int g_y = time.getYearProperty(), g_m = time.getMonthProperty(), g_d = time.getDayProperty();
        return gregorianToPersian(g_y, g_m, g_d).month;
    }

    [[nodiscard]] int GetDayOfMonth(const System::DateTime& time) const override {
        int g_y = time.getYearProperty(), g_m = time.getMonthProperty(), g_d = time.getDayProperty();
        return gregorianToPersian(g_y, g_m, g_d).day;
    }

    [[nodiscard]] bool IsLeapYear(int year, int /*era*/ = CurrentEra) const override {
        // Persian leap years follow a 2820-year cycle; simplified check
        static const int leapMod[] = {1,5,9,13,17,22,26,30};
        int y = year % 2820 + 474 + (year % 2820 <= 0 ? 2820 : 0);
        int rem = (y + 38) % 2820;
        for (int m : leapMod) if (rem == m * 682 % 2820 + 474) return true;
        // Simpler approximation: (year*8 + 29) % 33 < 8
        return ((year * 8) + 29) % 33 < 8;
    }

    [[nodiscard]] int GetDaysInMonth(int year, int month, int /*era*/ = CurrentEra) const override {
        if (month < 1 || month > 12) throw std::out_of_range("month");
        if (month <= 6) return 31;
        if (month <= 11) return 30;
        return IsLeapYear(year) ? 30 : 29;
    }

    [[nodiscard]] int GetDaysInYear(int year, int era = CurrentEra) const override {
        return IsLeapYear(year, era) ? 366 : 365;
    }

private:
    struct PDate { int year, month, day; };

    // Algorithm based on jdf.scr.ir — base year 1600 Gregorian
    static PDate gregorianToPersian(int gy, int gm, int gd) {
        static const int g_days_in_month[] = {31,28,31,30,31,30,31,31,30,31,30,31};
        static const int j_days_in_month[] = {31,31,31,31,31,31,30,30,30,30,30,29};

        int jy = gy - 1600;
        int jm_idx = gm - 1;

        int g_d_no = 365*jy + (jy+3)/4 - (jy+99)/100 + (jy+399)/400;
        for (int i = 0; i < jm_idx; ++i) g_d_no += g_days_in_month[i];
        if (gm > 1 && ((gy%4==0 && gy%100!=0) || gy%400==0)) ++g_d_no;
        g_d_no += gd;

        int j_d_no = g_d_no - 79;

        int j_np = j_d_no / 12053;
        j_d_no %= 12053;

        int jy_out = 979 + 33*j_np + 4*(j_d_no/1461);
        j_d_no %= 1461;

        if (j_d_no >= 366) {
            jy_out += (j_d_no-1) / 365;
            j_d_no  = (j_d_no-1) % 365;
        }

        int jm_out = 0;
        for (int i = 0; i < 11; ++i) {
            if (j_d_no >= j_days_in_month[i]) {
                j_d_no -= j_days_in_month[i];
                ++jm_out;
            } else break;
        }

        return {jy_out, jm_out + 1, j_d_no + 1};
    }
};

} // namespace System::Globalization
