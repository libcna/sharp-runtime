// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <stdexcept>
#include "System/DateTime.hpp"
#include "System/DayOfWeek.hpp"
#include "System/Globalization/CalendarWeekRule.hpp"

namespace System::Globalization {

    /// Abstract base class representing a calendar system.
    class Calendar {
    public:
        /// Constant representing the current era.
        static constexpr int CurrentEra = 0;

        virtual ~Calendar() = default;

        /// Gets the minimum year supported by this calendar.
        virtual int getMinSupportedDateTimeProperty() { return 1; }
        /// Gets the maximum year supported by this calendar.
        virtual int getMaxSupportedDateTimeProperty() { return 9999; }

        /// Returns the year component of the given DateTime in this calendar.
        [[nodiscard]] virtual int GetYear(const System::DateTime& time) const {
            return time.getYearProperty();
        }
        /// Returns the month component of the given DateTime in this calendar.
        [[nodiscard]] virtual int GetMonth(const System::DateTime& time) const {
            return time.getMonthProperty();
        }
        /// Returns the day-of-month for the given DateTime.
        [[nodiscard]] virtual int GetDayOfMonth(const System::DateTime& time) const {
            return time.getDayProperty();
        }
        /// Returns the day-of-week for the given DateTime.
        [[nodiscard]] virtual System::DayOfWeek GetDayOfWeek(const System::DateTime& time) const {
            return time.getDayOfWeekProperty();
        }
        /// Returns the day-of-year for the given DateTime.
        [[nodiscard]] virtual int GetDayOfYear(const System::DateTime& time) const {
            return time.getDayOfYearProperty();
        }
        /// Returns the hour component of the given DateTime.
        [[nodiscard]] virtual int GetHour(const System::DateTime& time) const {
            return time.getHourProperty();
        }
        /// Returns the minute component of the given DateTime.
        [[nodiscard]] virtual int GetMinute(const System::DateTime& time) const {
            return time.getMinuteProperty();
        }
        /// Returns the second component of the given DateTime.
        [[nodiscard]] virtual int GetSecond(const System::DateTime& time) const {
            return time.getSecondProperty();
        }
        /// Returns the millisecond component of the given DateTime.
        [[nodiscard]] virtual int GetMilliseconds(const System::DateTime& time) const {
            return time.getMillisecondProperty();
        }

        /// Returns the era for the given DateTime (always CurrentEra in this base implementation).
        [[nodiscard]] virtual int GetEra(const System::DateTime& /*time*/) const { return CurrentEra; }
        /// Returns the number of eras in this calendar.
        [[nodiscard]] virtual int GetErasCount() const { return 1; }

        /// Returns true if the specified year (and era) is a leap year.
        [[nodiscard]] virtual bool IsLeapYear(int year, int era = CurrentEra) const {
            (void)era;
            return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
        }

        /// Returns the number of days in the specified month of the given year and era.
        [[nodiscard]] virtual int GetDaysInMonth(int year, int month, int era = CurrentEra) const {
            (void)era;
            static const int days[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
            if (month == 2 && IsLeapYear(year)) return 29;
            return days[month];
        }

        /// Returns the number of days in the specified year and era.
        [[nodiscard]] virtual int GetDaysInYear(int year, int era = CurrentEra) const {
            return IsLeapYear(year, era) ? 366 : 365;
        }

        /// Returns the week-of-year for the given DateTime, using the specified rule and first day.
        [[nodiscard]] virtual int GetWeekOfYear(const System::DateTime& time,
                                                 CalendarWeekRule /*rule*/,
                                                 System::DayOfWeek /*firstDayOfWeek*/) const {
            return time.getDayOfYearProperty() / 7 + 1;
        }

        /// Returns a DateTime with the specified number of years added.
        virtual System::DateTime AddYears(const System::DateTime& time, int years) const {
            return System::DateTime(time.getYearProperty() + years,
                                    time.getMonthProperty(),
                                    time.getDayProperty());
        }
        /// Returns a DateTime with the specified number of months added.
        virtual System::DateTime AddMonths(const System::DateTime& time, int months) const {
            int totalMonths = (time.getYearProperty() - 1) * 12 + (time.getMonthProperty() - 1) + months;
            int year  = totalMonths / 12 + 1;
            int month = totalMonths % 12 + 1;
            int day   = std::min(time.getDayProperty(), GetDaysInMonth(year, month));
            return System::DateTime(year, month, day,
                                    time.getHourProperty(), time.getMinuteProperty(),
                                    time.getSecondProperty());
        }
        /// Returns a DateTime with the specified number of days added.
        virtual System::DateTime AddDays(const System::DateTime& time, int days) const {
            return time.AddDays(days);
        }
        virtual System::DateTime AddHours(const System::DateTime& time, int hours) const {
            return time.AddHours(hours);
        }
        virtual System::DateTime AddMinutes(const System::DateTime& time, int minutes) const {
            return time.AddMinutes(minutes);
        }
        virtual System::DateTime AddSeconds(const System::DateTime& time, int seconds) const {
            return time.AddSeconds(seconds);
        }
    };

} // namespace System::Globalization
