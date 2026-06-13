// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Globalization/Calendar.hpp"

namespace System::Globalization {

/// <summary>Represents the Taiwan calendar (Republic of China, Gregorian - 1911).</summary>
class TaiwanCalendar : public Calendar {
public:
    static constexpr int TaiwanEra = 1;       ///< The only era value for this calendar.
    static constexpr int GregorianOffset = 1911; ///< Years subtracted from the Gregorian year to get the ROC year.

    /// @return Always TaiwanEra (1).
    [[nodiscard]] int GetEra(const System::DateTime& /*time*/) const override { return TaiwanEra; }
    /// @return Always 1 — the Taiwan calendar has a single era.
    [[nodiscard]] int GetErasCount() const override { return 1; }

    /// @return The Republic of China year for @p time (Gregorian year - 1911).
    [[nodiscard]] int GetYear(const System::DateTime& time) const override {
        return time.getYearProperty() - GregorianOffset;
    }
};

} // namespace System::Globalization
