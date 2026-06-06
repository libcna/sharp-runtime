// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/DateTime.hpp"
#include "System/DayOfWeek.hpp"

namespace System::Globalization {

    class ISOWeek {
    public:
        ISOWeek() = delete;

        // Returns the ISO 8601 week number (1-53) for the given date.
        static int GetWeekOfYear(const System::DateTime& date) {
            // Use the standard algorithm: Thursday in the week determines the year.
            int doy = date.getDayOfYearProperty();
            int dow = static_cast<int>(date.getDayOfWeekProperty()); // 0=Sun..6=Sat
            // Convert to ISO: Mon=1..Sun=7
            int isoDow = (dow == 0) ? 7 : dow;
            int w = (doy - isoDow + 10) / 7;
            if (w < 1) return GetWeeksInYear(date.getYearProperty() - 1);
            if (w > GetWeeksInYear(date.getYearProperty())) return 1;
            return w;
        }

        // Returns the ISO 8601 year (may differ from calendar year near year boundaries).
        static int GetYear(const System::DateTime& date) {
            int w = GetWeekOfYear(date);
            int month = date.getMonthProperty();
            if (w == 1 && month == 12) return date.getYearProperty() + 1;
            if (w >= 52 && month == 1) return date.getYearProperty() - 1;
            return date.getYearProperty();
        }

        // Returns 52 or 53 depending on whether the given year has a long week.
        static int GetWeeksInYear(int year) {
            // A year has 53 weeks if Jan 1 or Dec 31 is Thursday.
            auto isLong = [](int y) {
                int jan1 = (y + y/4 - y/100 + y/400 + 0) % 7; // 0=Mon Zeller approx
                return jan1 == 3 || jan1 == 4; // Thu or Fri (leap)
            };
            return isLong(year) ? 53 : 52;
        }

        static System::DateTime GetYearStart(int year) {
            // First Monday on or before Jan 4
            System::DateTime jan4(year, 1, 4);
            int dow = static_cast<int>(jan4.getDayOfWeekProperty());
            int isoDow = (dow == 0) ? 7 : dow;
            return jan4.AddDays(1 - isoDow);
        }

        static System::DateTime GetYearEnd(int year) {
            return GetYearStart(year + 1).AddDays(-1);
        }
    };

} // namespace System::Globalization
