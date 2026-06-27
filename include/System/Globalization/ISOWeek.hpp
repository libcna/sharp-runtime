// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/DateTime.hpp"
#include "System/DayOfWeek.hpp"

namespace System::Globalization {

/**
 * @brief Provides static helpers for ISO 8601 week-based calendars.
 *
 * C++ counterpart of .NET System.Globalization.ISOWeek.
 * ISO 8601 weeks start on Monday; week 1 is the week containing the first Thursday
 * of the year. The ISO week-year may differ from the Gregorian year near year boundaries.
 */
class ISOWeek {
public:
    /** @brief Not instantiable — all members are static. */
    ISOWeek() = delete;

    /**
     * @brief Returns the ISO 8601 week number for the given date.
     *
     * C++ counterpart of .NET ISOWeek.GetWeekOfYear(DateTime).
     * Week 1 is the week containing the first Thursday of the year.
     * @param date The date to query.
     * @return The ISO 8601 week number (1–53).
     */
    static int GetWeekOfYear(const System::DateTime& date) {
        // Thursday-in-the-week rule: subtract ISO day-of-week, add 10, divide by 7.
        int doy = date.getDayOfYearProperty();
        int dow = static_cast<int>(date.getDayOfWeekProperty()); // 0=Sun..6=Sat
        int isoDow = (dow == 0) ? 7 : dow;                       // Mon=1..Sun=7
        int w = (doy - isoDow + 10) / 7;
        if (w < 1) return GetWeeksInYear(date.getYearProperty() - 1);
        if (w > GetWeeksInYear(date.getYearProperty())) return 1;
        return w;
    }

    /**
     * @brief Returns the ISO 8601 week-year for the given date.
     *
     * C++ counterpart of .NET ISOWeek.GetYear(DateTime).
     * May differ from the Gregorian year near year boundaries.
     * @param date The date to query.
     * @return The ISO 8601 week-year.
     */
    static int GetYear(const System::DateTime& date) {
        int w = GetWeekOfYear(date);
        int month = date.getMonthProperty();
        if (w == 1 && month == 12) return date.getYearProperty() + 1;
        if (w >= 52 && month == 1) return date.getYearProperty() - 1;
        return date.getYearProperty();
    }

    /**
     * @brief Returns the number of ISO weeks in the given year (52 or 53).
     *
     * C++ counterpart of .NET ISOWeek.GetWeeksInYear(int).
     * A year has 53 ISO weeks if January 1 or December 31 falls on Thursday.
     * @param year The Gregorian year.
     * @return 52 or 53.
     */
    static int GetWeeksInYear(int year) {
        auto isLong = [](int y) {
            int jan1 = (y + y/4 - y/100 + y/400 + 0) % 7;
            return jan1 == 3 || jan1 == 4; // Thursday or Friday (leap boundary)
        };
        return isLong(year) ? 53 : 52;
    }

    /**
     * @brief Returns the DateTime of the first day (Monday) of ISO week 1 in the given year.
     *
     * C++ counterpart of .NET ISOWeek.GetYearStart(int).
     * @param year The ISO week-year.
     * @return The Monday that begins ISO week 1.
     */
    static System::DateTime GetYearStart(int year) {
        System::DateTime jan4(year, 1, 4);
        int dow = static_cast<int>(jan4.getDayOfWeekProperty());
        int isoDow = (dow == 0) ? 7 : dow;
        return jan4.AddDays(1 - isoDow);
    }

    /**
     * @brief Returns the DateTime of the last day (Sunday) of the last ISO week in the given year.
     *
     * C++ counterpart of .NET ISOWeek.GetYearEnd(int).
     * @param year The ISO week-year.
     * @return The Sunday that ends the last ISO week of @p year.
     */
    static System::DateTime GetYearEnd(int year) {
        return GetYearStart(year + 1).AddDays(-1);
    }
};

} // namespace System::Globalization
