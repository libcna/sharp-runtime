// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Globalization/Calendar.hpp"

namespace System::Globalization {

/// <summary>Represents the Taiwan calendar (Republic of China, Gregorian - 1911).</summary>
class TaiwanCalendar : public Calendar {
public:
    static constexpr int TaiwanEra = 1;
    static constexpr int GregorianOffset = 1911;

    [[nodiscard]] int GetEra(const System::DateTime& /*time*/) const override { return TaiwanEra; }
    [[nodiscard]] int GetErasCount() const override { return 1; }

    [[nodiscard]] int GetYear(const System::DateTime& time) const override {
        return time.getYearProperty() - GregorianOffset;
    }
};

} // namespace System::Globalization
