// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Globalization/Calendar.hpp"

namespace System::Globalization {

/**
 * <summary>
 * Represents the Korean calendar. The year is Gregorian year + 2333.
 * Gregorian 2000/01/01 = Korean 4333/01/01.
 * </summary>
 */
class KoreanCalendar : public Calendar {
public:
    static constexpr int KoreanEra = 1;

    [[nodiscard]] int GetEra(const System::DateTime& /*time*/) const override { return KoreanEra; }
    [[nodiscard]] int GetErasCount() const override { return 1; }

    /** Returns the Korean year (Gregorian year + 2333). */
    [[nodiscard]] int GetYear(const System::DateTime& time) const override {
        return time.getYearProperty() + 2333;
    }
};

} // namespace System::Globalization
