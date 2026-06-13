// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Globalization/Calendar.hpp"

namespace System::Globalization {

/// <summary>Represents the Julian calendar. Every 4th year is a leap year; no century exception.</summary>
class JulianCalendar : public Calendar {
public:
    static constexpr int JulianEra = 1; ///< The only era value for this calendar.

    /// @return Always JulianEra (1).
    [[nodiscard]] int GetEra(const System::DateTime& /*time*/) const override { return JulianEra; }
    /// @return Always 1 — the Julian calendar has a single era.
    [[nodiscard]] int GetErasCount() const override { return 1; }

    /// @return True if @p year is divisible by 4 (Julian leap year rule).
    [[nodiscard]] bool IsLeapYear(int year, int /*era*/ = CurrentEra) const override {
        return year % 4 == 0;
    }

    /// @return Number of days in the given Julian month (accounts for Julian leap years).
    [[nodiscard]] int GetDaysInMonth(int year, int month, int /*era*/ = CurrentEra) const override {
        static const int days[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        if (month == 2 && IsLeapYear(year)) return 29;
        return days[month];
    }
};

} // namespace System::Globalization
