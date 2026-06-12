// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <array>
#include <stdexcept>
#include <string>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/DayOfWeek.hpp"
#include "System/Globalization/CalendarWeekRule.hpp"

namespace System::Globalization {

using SharpRuntime::intcs;

/// <summary>Defines how DateTime values are formatted and displayed, depending on the culture.</summary>
class DateTimeFormatInfo {
public:
    DateTimeFormatInfo() { initInvariant(); }

    static DateTimeFormatInfo getInvariantInfoProperty() {
        static DateTimeFormatInfo inv;
        return inv;
    }

    [[nodiscard]] bool getIsReadOnlyProperty() const { return _isReadOnly; }

    static DateTimeFormatInfo ReadOnly(const DateTimeFormatInfo& dtfi) {
        DateTimeFormatInfo copy = dtfi;
        copy._isReadOnly = true;
        return copy;
    }

    DateTimeFormatInfo Clone() const { return *this; }

    // --- Format patterns ---
    std::string FullDateTimePattern{"dddd, dd MMMM yyyy HH:mm:ss"};
    std::string LongDatePattern{"dddd, dd MMMM yyyy"};
    std::string LongTimePattern{"HH:mm:ss"};
    std::string ShortDatePattern{"MM/dd/yyyy"};
    std::string ShortTimePattern{"HH:mm"};
    std::string MonthDayPattern{"MMMM dd"};
    std::string YearMonthPattern{"yyyy MMMM"};
    std::string DateSeparator{"/"};
    std::string TimeSeparator{":"};
    std::string AMDesignator{"AM"};
    std::string PMDesignator{"PM"};

    // Read-only patterns
    [[nodiscard]] std::string getRFC1123PatternProperty() const { return "ddd, dd MMM yyyy HH':'mm':'ss 'GMT'"; }
    [[nodiscard]] std::string getSortableDateTimePatternProperty() const { return "yyyy'-'MM'-'dd'T'HH':'mm':'ss"; }
    [[nodiscard]] std::string getUniversalSortableDateTimePatternProperty() const { return "yyyy'-'MM'-'dd HH':'mm':'ss'Z'"; }

    System::DayOfWeek FirstDayOfWeek{System::DayOfWeek::Sunday};
    CalendarWeekRule CalendarWeekRuleValue{CalendarWeekRule::FirstDay};

    // --- Day names ---
    std::array<std::string, 7> AbbreviatedDayNames{"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    std::array<std::string, 7> DayNames{"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
    std::array<std::string, 7> ShortestDayNames{"Su","Mo","Tu","We","Th","Fr","Sa"};

    // --- Month names ---
    std::array<std::string, 13> AbbreviatedMonthNames{
        "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec",""};
    std::array<std::string, 13> MonthNames{
        "January","February","March","April","May","June",
        "July","August","September","October","November","December",""};

    std::string GetDayName(System::DayOfWeek dayofweek) const {
        return DayNames[static_cast<int>(dayofweek)];
    }
    std::string GetAbbreviatedDayName(System::DayOfWeek dayofweek) const {
        return AbbreviatedDayNames[static_cast<int>(dayofweek)];
    }
    std::string GetShortestDayName(System::DayOfWeek dayOfWeek) const {
        return ShortestDayNames[static_cast<int>(dayOfWeek)];
    }
    std::string GetMonthName(intcs month) const {
        if (month < 1 || month > 13) throw std::out_of_range("month");
        return MonthNames[static_cast<size_t>(month - 1)];
    }
    std::string GetAbbreviatedMonthName(intcs month) const {
        if (month < 1 || month > 13) throw std::out_of_range("month");
        return AbbreviatedMonthNames[static_cast<size_t>(month - 1)];
    }

private:
    bool _isReadOnly{false};

    void initInvariant() {
        // already initialized by member initializers
    }
};

} // namespace System::Globalization
