// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <array>
#include <stdexcept>
#include <string>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/DayOfWeek.hpp"
#include "System/Globalization/CalendarWeekRule.hpp"

namespace System::Globalization {

using SharpRuntime::intcs;

/**
 * @brief Defines how DateTime values are formatted and displayed, depending on the culture.
 *
 * C++ counterpart of .NET System.Globalization.DateTimeFormatInfo.
 * The default constructor initializes an invariant-culture instance.
 */
class DateTimeFormatInfo {
public:
    /**
     * @brief Constructs an invariant-culture DateTimeFormatInfo.
     *
     * C++ counterpart of .NET DateTimeFormatInfo().
     */
    DateTimeFormatInfo() = default;

    /**
     * @brief Returns a read-only DateTimeFormatInfo for the invariant culture.
     *
     * C++ counterpart of .NET DateTimeFormatInfo.InvariantInfo.
     * @return A read-only invariant DateTimeFormatInfo instance.
     */
    static const DateTimeFormatInfo& getInvariantInfoProperty() {
        static DateTimeFormatInfo inv = makeReadOnly();
        return inv;
    }

    /**
     * @brief Returns a read-only DateTimeFormatInfo for the current culture.
     *
     * C++ counterpart of .NET DateTimeFormatInfo.CurrentInfo.
     * Stub — returns the invariant info.
     * @return A const reference to the invariant DateTimeFormatInfo.
     */
    static const DateTimeFormatInfo& getCurrentInfoProperty() {
        return getInvariantInfoProperty();
    }

    /**
     * @brief Gets a value indicating whether this instance is read-only.
     *
     * C++ counterpart of .NET DateTimeFormatInfo.IsReadOnly.
     * @return true if this instance is read-only; otherwise false.
     */
    [[nodiscard]] bool getIsReadOnlyProperty() const { return isReadOnly_; }

    /**
     * @brief Returns a read-only copy of the given DateTimeFormatInfo.
     *
     * C++ counterpart of .NET DateTimeFormatInfo.ReadOnly(DateTimeFormatInfo).
     * @param dtfi The source instance.
     * @return A read-only copy.
     */
    static DateTimeFormatInfo ReadOnly(const DateTimeFormatInfo& dtfi) {
        DateTimeFormatInfo copy = dtfi;
        copy.isReadOnly_ = true;
        return copy;
    }

    /**
     * @brief Returns a mutable copy of this instance.
     *
     * C++ counterpart of .NET DateTimeFormatInfo.Clone().
     * @return A modifiable copy.
     */
    [[nodiscard]] DateTimeFormatInfo Clone() const { return *this; }

    /**
     * @brief Gets the native name of the calendar associated with this format info.
     *
     * C++ counterpart of .NET DateTimeFormatInfo.NativeCalendarName.
     * Stub — returns an empty string.
     * @return An empty string.
     */
    [[nodiscard]] std::string getNativeCalendarNameProperty() const { return ""; }

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

    /**
     * @brief Gets the RFC 1123 date/time format pattern (read-only).
     *
     * C++ counterpart of .NET DateTimeFormatInfo.RFC1123Pattern.
     * @return The RFC 1123 pattern string.
     */
    [[nodiscard]] std::string getRFC1123PatternProperty() const {
        return "ddd, dd MMM yyyy HH':'mm':'ss 'GMT'";
    }

    /**
     * @brief Gets the sortable date/time format pattern (ISO 8601, read-only).
     *
     * C++ counterpart of .NET DateTimeFormatInfo.SortableDateTimePattern.
     * @return The sortable date/time pattern string.
     */
    [[nodiscard]] std::string getSortableDateTimePatternProperty() const {
        return "yyyy'-'MM'-'dd'T'HH':'mm':'ss";
    }

    /**
     * @brief Gets the universal sortable date/time format pattern (read-only).
     *
     * C++ counterpart of .NET DateTimeFormatInfo.UniversalSortableDateTimePattern.
     * @return The universal sortable pattern string.
     */
    [[nodiscard]] std::string getUniversalSortableDateTimePatternProperty() const {
        return "yyyy'-'MM'-'dd HH':'mm':'ss'Z'";
    }

    System::DayOfWeek FirstDayOfWeek{System::DayOfWeek::Sunday};           ///< First day of the week.
    CalendarWeekRule CalendarWeekRuleValue{CalendarWeekRule::FirstDay};     ///< Rule for determining the first week of the year.

    // --- Day names ---
    std::array<std::string, 7> AbbreviatedDayNames{
        "Sun","Mon","Tue","Wed","Thu","Fri","Sat"};                          ///< Abbreviated day names (Sun–Sat).
    std::array<std::string, 7> DayNames{
        "Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"}; ///< Full day names.
    std::array<std::string, 7> ShortestDayNames{
        "Su","Mo","Tu","We","Th","Fr","Sa"};                                 ///< Shortest day name abbreviations.

    // --- Month names ---
    std::array<std::string, 13> AbbreviatedMonthNames{
        "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec",""}; ///< Abbreviated month names (index 12 empty for non-leap 13th month).
    std::array<std::string, 13> MonthNames{
        "January","February","March","April","May","June",
        "July","August","September","October","November","December",""};     ///< Full month names.

    std::array<std::string, 13> AbbreviatedMonthGenitiveNames{
        "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec",""}; ///< Abbreviated genitive month names (same as abbreviated names in invariant culture).
    std::array<std::string, 13> MonthGenitiveNames{
        "January","February","March","April","May","June",
        "July","August","September","October","November","December",""};     ///< Genitive month names (same as month names in invariant culture).

    /**
     * @brief Gets the full name of the specified day of the week.
     *
     * C++ counterpart of .NET DateTimeFormatInfo.GetDayName(DayOfWeek).
     * @param dayofweek The day of the week.
     * @return The full name of the day.
     */
    [[nodiscard]] std::string GetDayName(System::DayOfWeek dayofweek) const {
        return DayNames[static_cast<size_t>(dayofweek)];
    }

    /**
     * @brief Gets the abbreviated name of the specified day of the week.
     *
     * C++ counterpart of .NET DateTimeFormatInfo.GetAbbreviatedDayName(DayOfWeek).
     * @param dayofweek The day of the week.
     * @return The abbreviated name of the day.
     */
    [[nodiscard]] std::string GetAbbreviatedDayName(System::DayOfWeek dayofweek) const {
        return AbbreviatedDayNames[static_cast<size_t>(dayofweek)];
    }

    /**
     * @brief Gets the shortest abbreviated name of the specified day of the week.
     *
     * C++ counterpart of .NET DateTimeFormatInfo.GetShortestDayName(DayOfWeek).
     * @param dayOfWeek The day of the week.
     * @return The shortest day name abbreviation.
     */
    [[nodiscard]] std::string GetShortestDayName(System::DayOfWeek dayOfWeek) const {
        return ShortestDayNames[static_cast<size_t>(dayOfWeek)];
    }

    /**
     * @brief Gets the full name of the specified month.
     *
     * C++ counterpart of .NET DateTimeFormatInfo.GetMonthName(int).
     * @param month Month number 1–13.
     * @return The full name of the month.
     */
    [[nodiscard]] std::string GetMonthName(intcs month) const {
        if (month < 1 || month > 13) throw std::out_of_range("month");
        return MonthNames[static_cast<size_t>(month - 1)];
    }

    /**
     * @brief Gets the abbreviated name of the specified month.
     *
     * C++ counterpart of .NET DateTimeFormatInfo.GetAbbreviatedMonthName(int).
     * @param month Month number 1–13.
     * @return The abbreviated name of the month.
     */
    [[nodiscard]] std::string GetAbbreviatedMonthName(intcs month) const {
        if (month < 1 || month > 13) throw std::out_of_range("month");
        return AbbreviatedMonthNames[static_cast<size_t>(month - 1)];
    }

    /**
     * @brief Gets the string representation of the specified era.
     *
     * C++ counterpart of .NET DateTimeFormatInfo.GetEraName(int).
     * Stub — returns "AD" for era 1, empty string otherwise.
     * @param era The era index (1-based).
     * @return The era name string.
     */
    [[nodiscard]] std::string GetEraName(intcs era) const {
        return era == 1 ? "AD" : "";
    }

    /**
     * @brief Gets the abbreviated string representation of the specified era.
     *
     * C++ counterpart of .NET DateTimeFormatInfo.GetAbbreviatedEraName(int).
     * Stub — returns "AD" for era 1, empty string otherwise.
     * @param era The era index (1-based).
     * @return The abbreviated era name string.
     */
    [[nodiscard]] std::string GetAbbreviatedEraName(intcs era) const {
        return era == 1 ? "AD" : "";
    }

    /**
     * @brief Returns the era that corresponds to the specified string.
     *
     * C++ counterpart of .NET DateTimeFormatInfo.GetEra(string).
     * Stub — returns 1 for "AD"/"A.D.", -1 otherwise.
     * @param eraName The name of the era.
     * @return The era integer, or -1 if not found.
     */
    [[nodiscard]] intcs GetEra(const std::string& eraName) const {
        if (eraName == "AD" || eraName == "A.D.") return 1;
        return -1;
    }

    /**
     * @brief Returns all date and time format patterns for this instance.
     *
     * C++ counterpart of .NET DateTimeFormatInfo.GetAllDateTimePatterns().
     * Stub — returns the most common patterns.
     * @return A vector of format pattern strings.
     */
    [[nodiscard]] std::vector<std::string> GetAllDateTimePatterns() const {
        return {FullDateTimePattern, LongDatePattern, ShortDatePattern,
                LongTimePattern, ShortTimePattern, MonthDayPattern, YearMonthPattern};
    }

    /**
     * @brief Returns all date and time format patterns for the given format specifier.
     *
     * C++ counterpart of .NET DateTimeFormatInfo.GetAllDateTimePatterns(char).
     * Stub — returns a single-element vector with the pattern for the format character.
     * @param format The format specifier character.
     * @return A vector of pattern strings.
     */
    [[nodiscard]] std::vector<std::string> GetAllDateTimePatterns(char format) const {
        switch (format) {
            case 'd': return {ShortDatePattern};
            case 'D': return {LongDatePattern};
            case 'f': return {LongDatePattern + " " + ShortTimePattern};
            case 'F': return {FullDateTimePattern};
            case 'g': return {ShortDatePattern + " " + ShortTimePattern};
            case 'G': return {ShortDatePattern + " " + LongTimePattern};
            case 'm': case 'M': return {MonthDayPattern};
            case 'r': case 'R': return {getRFC1123PatternProperty()};
            case 's': return {getSortableDateTimePatternProperty()};
            case 't': return {ShortTimePattern};
            case 'T': return {LongTimePattern};
            case 'u': return {getUniversalSortableDateTimePatternProperty()};
            case 'y': case 'Y': return {YearMonthPattern};
            default: return {};
        }
    }

private:
    bool isReadOnly_{false};

    static DateTimeFormatInfo makeReadOnly() {
        DateTimeFormatInfo info;
        info.isReadOnly_ = true;
        return info;
    }
};

} // namespace System::Globalization
