// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Globalization/Calendar.hpp"
#include "System/Globalization/GregorianCalendarTypes.hpp"

namespace System::Globalization {

/**
 * @brief Represents the proleptic Gregorian calendar.
 *
 * C++ counterpart of .NET System.Globalization.GregorianCalendar.
 * The Gregorian calendar is the standard civil calendar used worldwide.
 * Leap year rule: divisible by 4, except centuries unless also divisible by 400.
 * Inherits all Calendar base implementations; only era and calendar-type members
 * are overridden here.
 */
class GregorianCalendar : public Calendar {
    GregorianCalendarTypes type_;

public:
    static constexpr int ADEra   = 1;    ///< Anno Domini era identifier.
    static constexpr int MinYear = 1;    ///< Minimum supported Gregorian year.
    static constexpr int MaxYear = 9999; ///< Maximum supported Gregorian year.

    /**
     * @brief Constructs a GregorianCalendar with the Localized type.
     *
     * C++ counterpart of .NET GregorianCalendar().
     */
    GregorianCalendar() : type_(GregorianCalendarTypes::Localized) {}

    /**
     * @brief Constructs a GregorianCalendar of the specified type.
     *
     * C++ counterpart of .NET GregorianCalendar(GregorianCalendarTypes).
     * @param type The Gregorian calendar type variant.
     */
    explicit GregorianCalendar(GregorianCalendarTypes type) : type_(type) {}

    /**
     * @brief Gets the Gregorian calendar type variant.
     *
     * C++ counterpart of .NET GregorianCalendar.CalendarType.
     * @return The GregorianCalendarTypes value for this instance.
     */
    [[nodiscard]] GregorianCalendarTypes getCalendarTypeProperty() const { return type_; }

    /**
     * @brief Sets the Gregorian calendar type variant.
     *
     * C++ counterpart of .NET GregorianCalendar.CalendarType setter.
     * @param t The new GregorianCalendarTypes value.
     */
    void setCalendarTypeProperty(GregorianCalendarTypes t) { type_ = t; }

    /**
     * @brief Returns the era for the given DateTime.
     *
     * C++ counterpart of .NET GregorianCalendar.GetEra(DateTime).
     * @return Always ADEra (1); the Gregorian calendar has a single era.
     */
    [[nodiscard]] int GetEra(const System::DateTime& /*time*/) const override { return ADEra; }

    /**
     * @brief Returns the number of eras in this calendar.
     *
     * @return Always 1; the Gregorian calendar has only the Anno Domini era.
     */
    [[nodiscard]] int GetErasCount() const override { return 1; }

    /**
     * @brief Determines whether the specified year is a Gregorian leap year.
     *
     * C++ counterpart of .NET GregorianCalendar.IsLeapYear(int, int).
     * Leap year rule: divisible by 4, except centuries unless also divisible by 400.
     * @param year The four-digit year.
     * @param era  The era (ignored; always AD).
     * @return true if @p year is a leap year; otherwise false.
     */
    [[nodiscard]] bool IsLeapYear(int year, int era = Calendar::CurrentEra) const override {
        (void)era;
        return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    }
};

} // namespace System::Globalization
