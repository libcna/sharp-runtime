// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <stdexcept>
#include "System/Globalization/Calendar.hpp"

namespace System::Globalization {

/**
 * @brief Represents the Persian (Solar Hijri) calendar.
 *
 * C++ counterpart of .NET System.Globalization.PersianCalendar.
 * The Persian calendar is a solar calendar used in Iran and Afghanistan.
 * Months 1–6 have 31 days, months 7–11 have 30 days, and month 12 has
 * 29 days (30 in a leap year). Leap years follow a 2820-year cycle;
 * the simplified rule (year*8+29)%33 < 8 is used here.
 */
class PersianCalendar : public Calendar {
public:
    static constexpr int PersianEra = 1; ///< The only era value for this calendar.

    /**
     * @brief Returns the era for the given DateTime.
     *
     * C++ counterpart of .NET PersianCalendar.GetEra(DateTime).
     * @return Always PersianEra (1).
     */
    [[nodiscard]] int GetEra(const System::DateTime& /*time*/) const override { return PersianEra; }

    /**
     * @brief Returns the number of eras in this calendar.
     *
     * @return Always 1; the Persian calendar has a single era.
     */
    [[nodiscard]] int GetErasCount() const override { return 1; }

    /**
     * @brief Returns the Persian year corresponding to the given DateTime.
     *
     * C++ counterpart of .NET PersianCalendar.GetYear(DateTime).
     * @param time The Gregorian DateTime to convert.
     * @return The Persian year.
     */
    [[nodiscard]] int GetYear(const System::DateTime& time) const override {
        int g_y = time.getYearProperty(), g_m = time.getMonthProperty(), g_d = time.getDayProperty();
        return gregorianToPersian(g_y, g_m, g_d).year;
    }

    /**
     * @brief Returns the Persian month corresponding to the given DateTime.
     *
     * C++ counterpart of .NET PersianCalendar.GetMonth(DateTime).
     * @param time The Gregorian DateTime to convert.
     * @return The Persian month (1–12).
     */
    [[nodiscard]] int GetMonth(const System::DateTime& time) const override {
        int g_y = time.getYearProperty(), g_m = time.getMonthProperty(), g_d = time.getDayProperty();
        return gregorianToPersian(g_y, g_m, g_d).month;
    }

    /**
     * @brief Returns the day of the Persian month corresponding to the given DateTime.
     *
     * C++ counterpart of .NET PersianCalendar.GetDayOfMonth(DateTime).
     * @param time The Gregorian DateTime to convert.
     * @return The day of the Persian month (1–31).
     */
    [[nodiscard]] int GetDayOfMonth(const System::DateTime& time) const override {
        int g_y = time.getYearProperty(), g_m = time.getMonthProperty(), g_d = time.getDayProperty();
        return gregorianToPersian(g_y, g_m, g_d).day;
    }

    /**
     * @brief Determines whether the specified Persian year is a leap year.
     *
     * C++ counterpart of .NET PersianCalendar.IsLeapYear(int, int).
     * Uses the simplified rule: (year*8 + 29) % 33 < 8.
     * @param year The Persian year.
     * @param era  The era (ignored; always PersianEra).
     * @return true if @p year is a Persian leap year; otherwise false.
     */
    [[nodiscard]] bool IsLeapYear(int year, int /*era*/ = CurrentEra) const override {
        return ((year * 8) + 29) % 33 < 8;
    }

    /**
     * @brief Returns the number of days in the specified Persian month.
     *
     * C++ counterpart of .NET PersianCalendar.GetDaysInMonth(int, int, int).
     * Months 1–6 have 31 days; months 7–11 have 30 days; month 12 has 29 or 30 days.
     * @param year  The Persian year.
     * @param month The Persian month (1–12).
     * @param era   The era (ignored).
     * @return The number of days in the specified month.
     */
    [[nodiscard]] int GetDaysInMonth(int year, int month, int /*era*/ = CurrentEra) const override {
        if (month < 1 || month > 12) throw std::out_of_range("month");
        if (month <= 6)  return 31;
        if (month <= 11) return 30;
        return IsLeapYear(year) ? 30 : 29;
    }

    /**
     * @brief Returns the number of days in the specified Persian year.
     *
     * C++ counterpart of .NET PersianCalendar.GetDaysInYear(int, int).
     * @param year The Persian year.
     * @param era  The era (default CurrentEra; ignored).
     * @return 366 for leap years, 365 otherwise.
     */
    [[nodiscard]] int GetDaysInYear(int year, int era = CurrentEra) const override {
        return IsLeapYear(year, era) ? 366 : 365;
    }

private:
    struct PDate { int year, month, day; };

    // Algorithm based on jdf.scr.ir — base year 1600 Gregorian.
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
