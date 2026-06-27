// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Globalization/Calendar.hpp"

namespace System::Globalization {

/**
 * @brief Represents the Korean calendar.
 *
 * C++ counterpart of .NET System.Globalization.KoreanCalendar.
 * The Korean calendar uses the same month/day/time structure as the Gregorian calendar,
 * but adds 2333 to the Gregorian year. For example, Gregorian 2000-01-01 = Korean 4333-01-01.
 */
class KoreanCalendar : public Calendar {
public:
    static constexpr int KoreanEra = 1; ///< The only era value for this calendar.

    /**
     * @brief Returns the era for the given DateTime.
     *
     * C++ counterpart of .NET KoreanCalendar.GetEra(DateTime).
     * @return Always KoreanEra (1).
     */
    [[nodiscard]] int GetEra(const System::DateTime& /*time*/) const override { return KoreanEra; }

    /**
     * @brief Returns the number of eras in this calendar.
     *
     * @return Always 1; the Korean calendar has a single era.
     */
    [[nodiscard]] int GetErasCount() const override { return 1; }

    /**
     * @brief Returns the Korean year corresponding to the given DateTime.
     *
     * C++ counterpart of .NET KoreanCalendar.GetYear(DateTime).
     * The Korean year equals the Gregorian year plus 2333.
     * @param time The DateTime to convert.
     * @return The Korean year (Gregorian year + 2333).
     */
    [[nodiscard]] int GetYear(const System::DateTime& time) const override {
        return time.getYearProperty() + 2333;
    }
};

} // namespace System::Globalization
