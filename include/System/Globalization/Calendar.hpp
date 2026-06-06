// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <stdexcept>
#include "System/DateTime.hpp"
#include "System/DayOfWeek.hpp"
#include "System/Globalization/CalendarWeekRule.hpp"

namespace System::Globalization {

    class Calendar {
    public:
        static constexpr int CurrentEra = 0;

        virtual ~Calendar() = default;

        virtual int getMinSupportedDateTimeProperty() { return 1; }
        virtual int getMaxSupportedDateTimeProperty() { return 9999; }

        [[nodiscard]] virtual int GetYear(const System::DateTime& time) const {
            return time.getYearProperty();
        }
        [[nodiscard]] virtual int GetMonth(const System::DateTime& time) const {
            return time.getMonthProperty();
        }
        [[nodiscard]] virtual int GetDayOfMonth(const System::DateTime& time) const {
            return time.getDayProperty();
        }
        [[nodiscard]] virtual System::DayOfWeek GetDayOfWeek(const System::DateTime& time) const {
            return time.getDayOfWeekProperty();
        }
        [[nodiscard]] virtual int GetDayOfYear(const System::DateTime& time) const {
            return time.getDayOfYearProperty();
        }
        [[nodiscard]] virtual int GetHour(const System::DateTime& time) const {
            return time.getHourProperty();
        }
        [[nodiscard]] virtual int GetMinute(const System::DateTime& time) const {
            return time.getMinuteProperty();
        }
        [[nodiscard]] virtual int GetSecond(const System::DateTime& time) const {
            return time.getSecondProperty();
        }
        [[nodiscard]] virtual int GetMilliseconds(const System::DateTime& time) const {
            return time.getMillisecondProperty();
        }

        [[nodiscard]] virtual int GetEra(const System::DateTime& /*time*/) const { return CurrentEra; }
        [[nodiscard]] virtual int GetErasCount() const { return 1; }

        [[nodiscard]] virtual bool IsLeapYear(int year, int era = CurrentEra) const {
            (void)era;
            return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
        }

        [[nodiscard]] virtual int GetDaysInMonth(int year, int month, int era = CurrentEra) const {
            (void)era;
            static const int days[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
            if (month == 2 && IsLeapYear(year)) return 29;
            return days[month];
        }

        [[nodiscard]] virtual int GetDaysInYear(int year, int era = CurrentEra) const {
            return IsLeapYear(year, era) ? 366 : 365;
        }

        [[nodiscard]] virtual int GetWeekOfYear(const System::DateTime& time,
                                                 CalendarWeekRule /*rule*/,
                                                 System::DayOfWeek /*firstDayOfWeek*/) const {
            return time.getDayOfYearProperty() / 7 + 1;
        }

        virtual System::DateTime AddYears(const System::DateTime& time, int years) const {
            return System::DateTime(time.getYearProperty() + years,
                                    time.getMonthProperty(),
                                    time.getDayProperty());
        }
        virtual System::DateTime AddMonths(const System::DateTime& time, int months) const {
            int totalMonths = (time.getYearProperty() - 1) * 12 + (time.getMonthProperty() - 1) + months;
            int year  = totalMonths / 12 + 1;
            int month = totalMonths % 12 + 1;
            int day   = std::min(time.getDayProperty(), GetDaysInMonth(year, month));
            return System::DateTime(year, month, day,
                                    time.getHourProperty(), time.getMinuteProperty(),
                                    time.getSecondProperty());
        }
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
