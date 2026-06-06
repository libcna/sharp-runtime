// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Globalization/Calendar.hpp"
#include "System/Globalization/GregorianCalendarTypes.hpp"

namespace System::Globalization {

    class GregorianCalendar : public Calendar {
        GregorianCalendarTypes type_;

    public:
        static constexpr int ADEra = 1;
        static constexpr int MinYear = 1;
        static constexpr int MaxYear = 9999;

        GregorianCalendar() : type_(GregorianCalendarTypes::Localized) {}
        explicit GregorianCalendar(GregorianCalendarTypes type) : type_(type) {}

        [[nodiscard]] GregorianCalendarTypes getCalendarTypeProperty() const { return type_; }
        void setCalendarTypeProperty(GregorianCalendarTypes t) { type_ = t; }

        [[nodiscard]] int GetEra(const System::DateTime& /*time*/) const override { return ADEra; }
        [[nodiscard]] int GetErasCount() const override { return 1; }

        [[nodiscard]] bool IsLeapYear(int year, int era = Calendar::CurrentEra) const override {
            (void)era;
            return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
        }
    };

} // namespace System::Globalization
