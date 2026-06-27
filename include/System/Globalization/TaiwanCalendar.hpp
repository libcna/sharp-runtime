// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Globalization/Calendar.hpp"

namespace System::Globalization {

/**
 * @brief Represents the Taiwan calendar (Republic of China calendar).
 *
 * C++ counterpart of .NET System.Globalization.TaiwanCalendar.
 * The Taiwan calendar uses the same month, day, and time structure as the Gregorian
 * calendar but counts years from the founding of the Republic of China in 1912.
 * The ROC year equals the Gregorian year minus 1911.
 */
class TaiwanCalendar : public Calendar {
public:
    static constexpr int TaiwanEra      = 1;    ///< The only era value for this calendar.
    static constexpr int GregorianOffset = 1911; ///< Offset subtracted from the Gregorian year to get the ROC year.

    /**
     * @brief Returns the era for the given DateTime.
     *
     * C++ counterpart of .NET TaiwanCalendar.GetEra(DateTime).
     * @return Always TaiwanEra (1).
     */
    [[nodiscard]] int GetEra(const System::DateTime& /*time*/) const override { return TaiwanEra; }

    /**
     * @brief Returns the number of eras in this calendar.
     *
     * @return Always 1; the Taiwan calendar has a single era.
     */
    [[nodiscard]] int GetErasCount() const override { return 1; }

    /**
     * @brief Returns the Republic of China year corresponding to the given DateTime.
     *
     * C++ counterpart of .NET TaiwanCalendar.GetYear(DateTime).
     * The ROC year equals the Gregorian year minus 1911.
     * @param time The Gregorian DateTime to convert.
     * @return The ROC year (Gregorian year - 1911).
     */
    [[nodiscard]] int GetYear(const System::DateTime& time) const override {
        return time.getYearProperty() - GregorianOffset;
    }
};

} // namespace System::Globalization
