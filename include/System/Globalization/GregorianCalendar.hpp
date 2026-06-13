// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Globalization/Calendar.hpp"
#include "System/Globalization/GregorianCalendarTypes.hpp"

namespace System::Globalization {

/// <summary>Represents the proleptic Gregorian calendar.</summary>
class GregorianCalendar : public Calendar {
    GregorianCalendarTypes type_;

public:
    static constexpr int ADEra   = 1;    ///< Anno Domini era identifier.
    static constexpr int MinYear = 1;    ///< Minimum supported Gregorian year.
    static constexpr int MaxYear = 9999; ///< Maximum supported Gregorian year.

    /// Constructs a Localized-type GregorianCalendar.
    GregorianCalendar() : type_(GregorianCalendarTypes::Localized) {}
    /// Constructs a GregorianCalendar of the specified @p type.
    explicit GregorianCalendar(GregorianCalendarTypes type) : type_(type) {}

    /// @return The Gregorian calendar type (e.g. Localized, USEnglish).
    [[nodiscard]] GregorianCalendarTypes getCalendarTypeProperty() const { return type_; }
    /// Sets the Gregorian calendar type to @p t.
    void setCalendarTypeProperty(GregorianCalendarTypes t) { type_ = t; }

    /// @return Always ADEra (1).
    [[nodiscard]] int GetEra(const System::DateTime& /*time*/) const override { return ADEra; }
    /// @return Always 1 — the Gregorian calendar has a single era.
    [[nodiscard]] int GetErasCount() const override { return 1; }

    /// @return True if @p year is a Gregorian leap year.
    [[nodiscard]] bool IsLeapYear(int year, int era = Calendar::CurrentEra) const override {
        (void)era;
        return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    }
};

} // namespace System::Globalization
