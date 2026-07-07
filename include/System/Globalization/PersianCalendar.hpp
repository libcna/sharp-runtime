// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <algorithm>
#include <stdexcept>
#include "System/ArgumentOutOfRangeException.hpp"
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
     * @brief Gets the algorithm type for this calendar.
     *
     * C++ counterpart of .NET PersianCalendar.AlgorithmType.
     * @return Always CalendarAlgorithmType::SolarCalendar.
     */
    [[nodiscard]] CalendarAlgorithmType getAlgorithmTypeProperty() const override {
        return CalendarAlgorithmType::SolarCalendar;
    }

    /**
     * @brief Gets the earliest date supported by PersianCalendar.
     *
     * C++ counterpart of .NET PersianCalendar.MinSupportedDateTime.
     * @return DateTime(622, 3, 22) — approximate start of the Persian calendar epoch.
     */
    [[nodiscard]] System::DateTime getMinSupportedDateTimeProperty() const override {
        return System::DateTime(622, 3, 22);
    }

    /**
     * @brief Gets the latest date supported by PersianCalendar.
     *
     * C++ counterpart of .NET PersianCalendar.MaxSupportedDateTime.
     * @return DateTime(9999, 12, 31).
     */
    [[nodiscard]] System::DateTime getMaxSupportedDateTimeProperty() const override {
        return System::DateTime(9999, 12, 31);
    }

    /**
     * @brief Gets the last two-digit year that maps into the range of this calendar.
     *
     * C++ counterpart of .NET PersianCalendar.TwoDigitYearMax.
     * @return The maximum two-digit year (default 1410).
     */
    [[nodiscard]] int getTwoDigitYearMaxProperty() const override { return twoDigitYearMax_; }

    /**
     * @brief Sets the last two-digit year that maps into the range of this calendar.
     *
     * C++ counterpart of .NET PersianCalendar.TwoDigitYearMax setter.
     * @param value The new maximum two-digit year.
     * @throws System::InvalidOperationException if this instance is read-only.
     */
    void setTwoDigitYearMaxProperty(int value) override {
        VerifyWritable();
        twoDigitYearMax_ = value;
    }

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
     * @brief Gets the list of era identifiers supported by this calendar.
     *
     * C++ counterpart of .NET PersianCalendar.Eras.
     * @return A vector containing {PersianEra}.
     */
    [[nodiscard]] std::vector<int> getErasProperty() const override { return {PersianEra}; }

    /**
     * @brief Returns the Persian year corresponding to the given DateTime.
     *
     * C++ counterpart of .NET PersianCalendar.GetYear(DateTime).
     * @param time The Gregorian DateTime to convert.
     * @return The Persian year.
     */
    [[nodiscard]] int GetYear(const System::DateTime& time) const override {
        return gregorianToPersian(time.getYearProperty(), time.getMonthProperty(), time.getDayProperty()).year;
    }

    /**
     * @brief Returns the Persian month corresponding to the given DateTime.
     *
     * C++ counterpart of .NET PersianCalendar.GetMonth(DateTime).
     * @param time The Gregorian DateTime to convert.
     * @return The Persian month (1–12).
     */
    [[nodiscard]] int GetMonth(const System::DateTime& time) const override {
        return gregorianToPersian(time.getYearProperty(), time.getMonthProperty(), time.getDayProperty()).month;
    }

    /**
     * @brief Returns the day of the Persian month corresponding to the given DateTime.
     *
     * C++ counterpart of .NET PersianCalendar.GetDayOfMonth(DateTime).
     * @param time The Gregorian DateTime to convert.
     * @return The day of the Persian month (1–31).
     */
    [[nodiscard]] int GetDayOfMonth(const System::DateTime& time) const override {
        return gregorianToPersian(time.getYearProperty(), time.getMonthProperty(), time.getDayProperty()).day;
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
        if (month < 1 || month > 12) throw System::ArgumentOutOfRangeException("month");
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
    [[nodiscard]] int GetDaysInYear(int year, int /*era*/ = CurrentEra) const override {
        return IsLeapYear(year) ? 366 : 365;
    }

    /**
     * @brief Returns a DateTime from the Persian date and time components.
     *
     * C++ counterpart of .NET PersianCalendar.ToDateTime(int, int, int, int, int, int, int, int).
     * @param year        The Persian year.
     * @param month       The Persian month (1–12).
     * @param day         The Persian day (1–31).
     * @param hour        Hour (0–23).
     * @param minute      Minute (0–59).
     * @param second      Second (0–59).
     * @param millisecond Millisecond (0–999).
     * @param era         Era (ignored).
     * @return The Gregorian DateTime corresponding to the given Persian date components.
     */
    System::DateTime ToDateTime(int year, int month, int day, int hour, int minute,
                                int second, int millisecond, int /*era*/ = CurrentEra) const override {
        return persianToDateTime(year, month, day, hour, minute, second, millisecond);
    }

    /**
     * @brief Returns a DateTime that is the specified number of months from the given DateTime.
     *
     * C++ counterpart of .NET PersianCalendar.AddMonths(DateTime, int).
     * Performs month arithmetic in the Persian calendar system.
     * @param time   The starting DateTime.
     * @param months The number of months to add (may be negative).
     * @return A new DateTime offset by @p months Persian months.
     */
    System::DateTime AddMonths(const System::DateTime& time, int months) const override {
        PDate pd = gregorianToPersian(time.getYearProperty(), time.getMonthProperty(), time.getDayProperty());
        int totalMonths = (pd.year - 1) * 12 + (pd.month - 1) + months;
        int newYear  = totalMonths / 12 + 1;
        int newMonth = totalMonths % 12 + 1;
        int newDay   = std::min(pd.day, GetDaysInMonth(newYear, newMonth));
        return persianToDateTime(newYear, newMonth, newDay,
                                 time.getHourProperty(), time.getMinuteProperty(),
                                 time.getSecondProperty(), time.getMillisecondProperty());
    }

    /**
     * @brief Returns a DateTime that is the specified number of years from the given DateTime.
     *
     * C++ counterpart of .NET PersianCalendar.AddYears(DateTime, int).
     * Performs year arithmetic in the Persian calendar system.
     * @param time  The starting DateTime.
     * @param years The number of years to add (may be negative).
     * @return A new DateTime offset by @p years Persian years.
     */
    System::DateTime AddYears(const System::DateTime& time, int years) const override {
        PDate pd = gregorianToPersian(time.getYearProperty(), time.getMonthProperty(), time.getDayProperty());
        int newYear = pd.year + years;
        int newDay  = std::min(pd.day, GetDaysInMonth(newYear, pd.month));
        return persianToDateTime(newYear, pd.month, newDay,
                                 time.getHourProperty(), time.getMinuteProperty(),
                                 time.getSecondProperty(), time.getMillisecondProperty());
    }

private:
    int twoDigitYearMax_{1410};

    struct PDate { int year, month, day; };

    // Converts a Gregorian date to a Persian date (jdf.scr.ir algorithm, base year 1600).
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

    // Converts a Persian date to a Gregorian DateTime (inverse of the jdf.scr.ir algorithm).
    // Persian 979/1/1 = Gregorian March 19, 1600 (j_d_no = 0 in the algorithm).
    static System::DateTime persianToDateTime(int py, int pm, int pd, int h, int mi, int s, int ms) {
        static const int jm[] = {31,31,31,31,31,31,30,30,30,30,30,29};
        static const int gm[] = {31,28,31,30,31,30,31,31,30,31,30,31};

        // Step 1: compute j_d_no (days from Persian 979/1/1) using 33-year cycles.
        int yDiff = py - 979;
        long long j_d_no = 0;
        if (yDiff >= 0) {
            int full33  = yDiff / 33;
            int rem     = yDiff % 33;
            j_d_no = static_cast<long long>(full33) * 12053;
            int baseYear = 979 + full33 * 33;
            for (int y = 0; y < rem; ++y) {
                int yr = baseYear + y;
                j_d_no += (((yr * 8) + 29) % 33 < 8) ? 366 : 365;
            }
        } else {
            for (int y = py; y < 979; ++y)
                j_d_no -= (((y * 8) + 29) % 33 < 8) ? 366 : 365;
        }
        for (int m = 0; m < pm - 1; ++m) j_d_no += jm[m];
        j_d_no += pd - 1;

        long long g_d_no = j_d_no + 79;

        // Step 2: convert g_d_no to Gregorian using floor division to handle years < 1600.
        auto floorDiv = [](long long a, long long b) -> long long {
            return a / b - (a % b != 0 && (a < 0) != (b < 0) ? 1 : 0);
        };
        auto fFunc = [&](long long jy) -> long long {
            return 365 * jy
                 + floorDiv(jy + 3,   4)
                 - floorDiv(jy + 99,  100)
                 + floorDiv(jy + 399, 400);
        };

        long long jy = (g_d_no > 0) ? (g_d_no - 1) / 365 - 2 : g_d_no / 365 - 2;
        while (fFunc(jy + 1) < g_d_no) ++jy;
        while (fFunc(jy) >= g_d_no)    --jy;

        int gy = 1600 + static_cast<int>(jy);
        long long doy = g_d_no - fFunc(jy); // 1-based day of year

        bool isLeap = (gy % 4 == 0 && gy % 100 != 0) || gy % 400 == 0;
        int gmonth = 0;
        while (gmonth < 11) {
            int mdim = (gmonth == 1 && isLeap) ? 29 : gm[gmonth];
            if (doy <= mdim) break;
            doy -= mdim;
            ++gmonth;
        }
        return System::DateTime(gy, gmonth + 1, static_cast<int>(doy), h, mi, s, ms);
    }
};

} // namespace System::Globalization
