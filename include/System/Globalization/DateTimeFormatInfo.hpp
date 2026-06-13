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
    /// Constructs an invariant-culture DateTimeFormatInfo.
    DateTimeFormatInfo() { initInvariant(); }

    /// @return A read-only DateTimeFormatInfo for the invariant culture.
    static DateTimeFormatInfo getInvariantInfoProperty() {
        static DateTimeFormatInfo inv;
        return inv;
    }

    /// @return True if this instance is read-only.
    [[nodiscard]] bool getIsReadOnlyProperty() const { return _isReadOnly; }

    /// @return A read-only copy of @p dtfi.
    static DateTimeFormatInfo ReadOnly(const DateTimeFormatInfo& dtfi) {
        DateTimeFormatInfo copy = dtfi;
        copy._isReadOnly = true;
        return copy;
    }

    /// @return A mutable copy of this instance.
    DateTimeFormatInfo Clone() const { return *this; }

    // --- Format patterns ---
    std::string FullDateTimePattern{"dddd, dd MMMM yyyy HH:mm:ss"}; ///< Full date and time pattern.
    std::string LongDatePattern{"dddd, dd MMMM yyyy"};              ///< Long date pattern.
    std::string LongTimePattern{"HH:mm:ss"};                        ///< Long time pattern.
    std::string ShortDatePattern{"MM/dd/yyyy"};                     ///< Short date pattern.
    std::string ShortTimePattern{"HH:mm"};                          ///< Short time pattern.
    std::string MonthDayPattern{"MMMM dd"};                         ///< Month and day pattern.
    std::string YearMonthPattern{"yyyy MMMM"};                      ///< Year and month pattern.
    std::string DateSeparator{"/"};                                  ///< Separator between date components.
    std::string TimeSeparator{":"};                                  ///< Separator between time components.
    std::string AMDesignator{"AM"};                                  ///< Designator for ante meridiem.
    std::string PMDesignator{"PM"};                                  ///< Designator for post meridiem.

    /// @return RFC 1123 format pattern (read-only).
    [[nodiscard]] std::string getRFC1123PatternProperty() const { return "ddd, dd MMM yyyy HH':'mm':'ss 'GMT'"; }
    /// @return Sortable date/time pattern (ISO 8601, read-only).
    [[nodiscard]] std::string getSortableDateTimePatternProperty() const { return "yyyy'-'MM'-'dd'T'HH':'mm':'ss"; }
    /// @return Universal sortable date/time pattern (read-only).
    [[nodiscard]] std::string getUniversalSortableDateTimePatternProperty() const { return "yyyy'-'MM'-'dd HH':'mm':'ss'Z'"; }

    System::DayOfWeek FirstDayOfWeek{System::DayOfWeek::Sunday};           ///< First day of the week.
    CalendarWeekRule CalendarWeekRuleValue{CalendarWeekRule::FirstDay};     ///< Rule for determining the first week of the year.

    // --- Day names ---
    std::array<std::string, 7> AbbreviatedDayNames{"Sun","Mon","Tue","Wed","Thu","Fri","Sat"}; ///< Abbreviated day names (Sun–Sat).
    std::array<std::string, 7> DayNames{"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"}; ///< Full day names.
    std::array<std::string, 7> ShortestDayNames{"Su","Mo","Tu","We","Th","Fr","Sa"}; ///< Shortest day name abbreviations.

    // --- Month names ---
    std::array<std::string, 13> AbbreviatedMonthNames{
        "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec",""}; ///< Abbreviated month names (index 12 is empty for non-leap 13th month).
    std::array<std::string, 13> MonthNames{
        "January","February","March","April","May","June",
        "July","August","September","October","November","December",""}; ///< Full month names.

    /// @return Full name of the given day of week.
    std::string GetDayName(System::DayOfWeek dayofweek) const {
        return DayNames[static_cast<int>(dayofweek)];
    }
    /// @return Abbreviated name of the given day of week.
    std::string GetAbbreviatedDayName(System::DayOfWeek dayofweek) const {
        return AbbreviatedDayNames[static_cast<int>(dayofweek)];
    }
    /// @return Shortest name of the given day of week.
    std::string GetShortestDayName(System::DayOfWeek dayOfWeek) const {
        return ShortestDayNames[static_cast<int>(dayOfWeek)];
    }
    /// @param month Month number 1–13.
    /// @return Full name of the month.
    std::string GetMonthName(intcs month) const {
        if (month < 1 || month > 13) throw std::out_of_range("month");
        return MonthNames[static_cast<size_t>(month - 1)];
    }
    /// @param month Month number 1–13.
    /// @return Abbreviated name of the month.
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
