// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Globalization/Calendar.hpp"

namespace System::Globalization {

/// <summary>Represents the Julian calendar. Every 4th year is a leap year; no century exception.</summary>
class JulianCalendar : public Calendar {
public:
    static constexpr int JulianEra = 1;

    [[nodiscard]] int GetEra(const System::DateTime& /*time*/) const override { return JulianEra; }
    [[nodiscard]] int GetErasCount() const override { return 1; }

    [[nodiscard]] bool IsLeapYear(int year, int /*era*/ = CurrentEra) const override {
        return year % 4 == 0;
    }

    [[nodiscard]] int GetDaysInMonth(int year, int month, int /*era*/ = CurrentEra) const override {
        static const int days[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        if (month == 2 && IsLeapYear(year)) return 29;
        return days[month];
    }
};

} // namespace System::Globalization
