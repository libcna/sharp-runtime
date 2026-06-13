// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Globalization/Calendar.hpp"

namespace System::Globalization {

/// <summary>Represents the Thai Buddhist calendar (Gregorian + 543 years).</summary>
class ThaiBuddhistCalendar : public Calendar {
public:
    static constexpr int ThaiBuddhistEra = 1;   ///< The only era value for this calendar.
    static constexpr int GregorianOffset = 543; ///< Years added to the Gregorian year to get the Buddhist Era year.

    /// @return Always ThaiBuddhistEra (1).
    [[nodiscard]] int GetEra(const System::DateTime& /*time*/) const override { return ThaiBuddhistEra; }
    /// @return Always 1 — the Thai Buddhist calendar has a single era.
    [[nodiscard]] int GetErasCount() const override { return 1; }

    /// @return The Thai Buddhist year for @p time (Gregorian year + 543).
    [[nodiscard]] int GetYear(const System::DateTime& time) const override {
        return time.getYearProperty() + GregorianOffset;
    }
};

} // namespace System::Globalization
