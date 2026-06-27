// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <stdexcept>
#include "System/Globalization/Calendar.hpp"

namespace System::Globalization {

/**
 * @brief Represents the Japanese imperial calendar.
 *
 * C++ counterpart of .NET System.Globalization.JapaneseCalendar.
 * Month and day values are identical to the Gregorian calendar; the year
 * is counted from the start of the current imperial era.
 *
 * Supported eras (most recent first):
 *  - 5 Reiwa   — from 2019-05-01 AD
 *  - 4 Heisei  — from 1989-01-08 AD
 *  - 3 Showa   — from 1926-12-25 AD
 *  - 2 Taisho  — from 1912-07-30 AD
 *  - 1 Meiji   — from 1868-01-01 AD
 */
class JapaneseCalendar : public Calendar {
public:
    static constexpr int MeijiEra  = 1; ///< Meiji era identifier (1868-01-01 – 1912-07-29).
    static constexpr int TaishoEra = 2; ///< Taisho era identifier (1912-07-30 – 1926-12-24).
    static constexpr int ShowaEra  = 3; ///< Showa era identifier (1926-12-25 – 1989-01-07).
    static constexpr int HeiseiEra = 4; ///< Heisei era identifier (1989-01-08 – 2019-04-30).
    static constexpr int ReiwaEra  = 5; ///< Reiwa era identifier (from 2019-05-01).

    /**
     * @brief Returns the number of Japanese imperial eras.
     *
     * @return Always 5 (Meiji through Reiwa).
     */
    [[nodiscard]] int GetErasCount() const override { return 5; }

    /**
     * @brief Returns the imperial era for the given DateTime.
     *
     * C++ counterpart of .NET JapaneseCalendar.GetEra(DateTime).
     * @param time The date to classify.
     * @return The era identifier (1–5).
     * @throws std::out_of_range if @p time precedes the Meiji era (1868-01-01).
     */
    [[nodiscard]] int GetEra(const System::DateTime& time) const override {
        int y = time.getYearProperty(), m = time.getMonthProperty(), d = time.getDayProperty();
        if (y > 2019 || (y == 2019 && (m > 5 || (m == 5 && d >= 1)))) return ReiwaEra;
        if (y > 1989 || (y == 1989 && (m > 1 || (m == 1 && d >= 8)))) return HeiseiEra;
        if (y > 1926 || (y == 1926 && (m > 12 || (m == 12 && d >= 25)))) return ShowaEra;
        if (y > 1912 || (y == 1912 && (m > 7 || (m == 7 && d >= 30)))) return TaishoEra;
        if (y >= 1868) return MeijiEra;
        throw std::out_of_range("JapaneseCalendar: date is before the Meiji era (1868-01-01).");
    }

    /**
     * @brief Returns the year within the current imperial era for the given DateTime.
     *
     * C++ counterpart of .NET JapaneseCalendar.GetYear(DateTime).
     * Year 1 of each era corresponds to the first Gregorian year of that era.
     * @param time The date to query.
     * @return The era-relative year (1-based).
     */
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
