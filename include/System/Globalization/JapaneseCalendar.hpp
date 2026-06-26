// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <stdexcept>
#include "System/Globalization/Calendar.hpp"

namespace System::Globalization {

/**
 * <summary>
 * Represents the Japanese calendar. Month and day are the same as Gregorian;
 * the year is counted from the start of the current imperial era.
 * 
 * Eras (most recent first):
 *   5 – Reiwa   (2019-05-01 AD)
 *   4 – Heisei  (1989-01-08 AD)
 *   3 – Showa   (1926-12-25 AD)
 *   2 – Taisho  (1912-07-30 AD)
 *   1 – Meiji   (1868-01-01 AD)
 * </summary>
 */
class JapaneseCalendar : public Calendar {
public:
    static constexpr int MeijiEra   = 1;
    static constexpr int TaishoEra  = 2;
    static constexpr int ShowaEra   = 3;
    static constexpr int HeiseiEra  = 4;
    static constexpr int ReiwaEra   = 5;

    [[nodiscard]] int GetErasCount() const override { return 5; }

    [[nodiscard]] int GetEra(const System::DateTime& time) const override {
        int y = time.getYearProperty(), m = time.getMonthProperty(), d = time.getDayProperty();
        if (y > 2019 || (y == 2019 && (m > 5 || (m == 5 && d >= 1)))) return ReiwaEra;
        if (y > 1989 || (y == 1989 && (m > 1 || (m == 1 && d >= 8)))) return HeiseiEra;
        if (y > 1926 || (y == 1926 && (m > 12 || (m == 12 && d >= 25)))) return ShowaEra;
        if (y > 1912 || (y == 1912 && (m > 7 || (m == 7 && d >= 30)))) return TaishoEra;
        if (y >= 1868) return MeijiEra;
        throw std::out_of_range("JapaneseCalendar: date is before Meiji era (1868-01-01).");
    }

    /** Returns the year within the current imperial era. */
    [[nodiscard]] int GetYear(const System::DateTime& time) const override {
        int era = GetEra(time);
        int gy  = time.getYearProperty();
        switch (era) {
            case ReiwaEra:  return gy - 2018; // Reiwa 1 = 2019
            case HeiseiEra: return gy - 1988; // Heisei 1 = 1989
            case ShowaEra:  return gy - 1925; // Showa 1 = 1926
            case TaishoEra: return gy - 1911; // Taisho 1 = 1912
            case MeijiEra:  return gy - 1867; // Meiji 1 = 1868
            default:        return gy;
        }
    }
};

} // namespace System::Globalization
