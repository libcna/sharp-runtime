// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <array>
#include <stdexcept>
#include <string>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/DayOfWeek.hpp"
#include "System/InvalidOperationException.hpp"
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

    /** @brief Gets the custom format string for a long date and time value. C++ counterpart of .NET DateTimeFormatInfo.FullDateTimePattern. */
    [[nodiscard]] const std::string& getFullDateTimePatternProperty() const { return fullDateTimePattern_; }
    /** @brief Sets the custom format string for a long date and time value. */
    void setFullDateTimePatternProperty(const std::string& value) { VerifyWritable(); fullDateTimePattern_ = value; }

    /** @brief Gets the custom format string for a long date value. C++ counterpart of .NET DateTimeFormatInfo.LongDatePattern. */
    [[nodiscard]] const std::string& getLongDatePatternProperty() const { return longDatePattern_; }
    /** @brief Sets the custom format string for a long date value. */
    void setLongDatePatternProperty(const std::string& value) { VerifyWritable(); longDatePattern_ = value; }

    /** @brief Gets the custom format string for a long time value. C++ counterpart of .NET DateTimeFormatInfo.LongTimePattern. */
    [[nodiscard]] const std::string& getLongTimePatternProperty() const { return longTimePattern_; }
    /** @brief Sets the custom format string for a long time value. */
    void setLongTimePatternProperty(const std::string& value) { VerifyWritable(); longTimePattern_ = value; }

    /** @brief Gets the custom format string for a short date value. C++ counterpart of .NET DateTimeFormatInfo.ShortDatePattern. */
    [[nodiscard]] const std::string& getShortDatePatternProperty() const { return shortDatePattern_; }
    /** @brief Sets the custom format string for a short date value. */
    void setShortDatePatternProperty(const std::string& value) { VerifyWritable(); shortDatePattern_ = value; }

    /** @brief Gets the custom format string for a short time value. C++ counterpart of .NET DateTimeFormatInfo.ShortTimePattern. */
    [[nodiscard]] const std::string& getShortTimePatternProperty() const { return shortTimePattern_; }
    /** @brief Sets the custom format string for a short time value. */
    void setShortTimePatternProperty(const std::string& value) { VerifyWritable(); shortTimePattern_ = value; }

    /** @brief Gets the custom format string for a month and day value. C++ counterpart of .NET DateTimeFormatInfo.MonthDayPattern. */
    [[nodiscard]] const std::string& getMonthDayPatternProperty() const { return monthDayPattern_; }
    /** @brief Sets the custom format string for a month and day value. */
    void setMonthDayPatternProperty(const std::string& value) { VerifyWritable(); monthDayPattern_ = value; }

    /** @brief Gets the custom format string for a year and month value. C++ counterpart of .NET DateTimeFormatInfo.YearMonthPattern. */
    [[nodiscard]] const std::string& getYearMonthPatternProperty() const { return yearMonthPattern_; }
    /** @brief Sets the custom format string for a year and month value. */
    void setYearMonthPatternProperty(const std::string& value) { VerifyWritable(); yearMonthPattern_ = value; }

    /** @brief Gets the string that separates date components. C++ counterpart of .NET DateTimeFormatInfo.DateSeparator. */
    [[nodiscard]] const std::string& getDateSeparatorProperty() const { return dateSeparator_; }
    /** @brief Sets the string that separates date components. */
    void setDateSeparatorProperty(const std::string& value) { VerifyWritable(); dateSeparator_ = value; }

    /** @brief Gets the string that separates time components. C++ counterpart of .NET DateTimeFormatInfo.TimeSeparator. */
    [[nodiscard]] const std::string& getTimeSeparatorProperty() const { return timeSeparator_; }
    /** @brief Sets the string that separates time components. */
    void setTimeSeparatorProperty(const std::string& value) { VerifyWritable(); timeSeparator_ = value; }

    /** @brief Gets the string designator for hours before noon. C++ counterpart of .NET DateTimeFormatInfo.AMDesignator. */
    [[nodiscard]] const std::string& getAMDesignatorProperty() const { return amDesignator_; }
    /** @brief Sets the string designator for hours before noon. */
    void setAMDesignatorProperty(const std::string& value) { VerifyWritable(); amDesignator_ = value; }

    /** @brief Gets the string designator for hours after noon. C++ counterpart of .NET DateTimeFormatInfo.PMDesignator. */
    [[nodiscard]] const std::string& getPMDesignatorProperty() const { return pmDesignator_; }
    /** @brief Sets the string designator for hours after noon. */
    void setPMDesignatorProperty(const std::string& value) { VerifyWritable(); pmDesignator_ = value; }

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

    /** @brief Gets the first day of the week. C++ counterpart of .NET DateTimeFormatInfo.FirstDayOfWeek. */
    [[nodiscard]] System::DayOfWeek getFirstDayOfWeekProperty() const { return firstDayOfWeek_; }
    /**
     * @brief Sets the first day of the week.
     * @throws System::ArgumentOutOfRangeException if @p value is outside Sunday..Saturday.
     */
    void setFirstDayOfWeekProperty(System::DayOfWeek value) {
        VerifyWritable();
        if (value < System::DayOfWeek::Sunday || value > System::DayOfWeek::Saturday) {
            throw System::ArgumentOutOfRangeException("value");
        }
        firstDayOfWeek_ = value;
    }

    /** @brief Gets the rule used to determine the first week of the year. C++ counterpart of .NET DateTimeFormatInfo.CalendarWeekRule. */
    [[nodiscard]] CalendarWeekRule getCalendarWeekRuleProperty() const { return calendarWeekRule_; }
    /**
     * @brief Sets the rule used to determine the first week of the year.
     * @throws System::ArgumentOutOfRangeException if @p value is outside FirstDay..FirstFourDayWeek.
     */
    void setCalendarWeekRuleProperty(CalendarWeekRule value) {
        VerifyWritable();
        if (value < CalendarWeekRule::FirstDay || value > CalendarWeekRule::FirstFourDayWeek) {
            throw System::ArgumentOutOfRangeException("value");
        }
        calendarWeekRule_ = value;
    }

    /** @brief Gets the abbreviated day names (Sun-Sat). C++ counterpart of .NET DateTimeFormatInfo.AbbreviatedDayNames. Returns a copy, matching .NET. */
    [[nodiscard]] std::array<std::string, 7> getAbbreviatedDayNamesProperty() const { return abbreviatedDayNames_; }
    /** @brief Sets the abbreviated day names. */
    void setAbbreviatedDayNamesProperty(const std::array<std::string, 7>& value) { VerifyWritable(); abbreviatedDayNames_ = value; }

    /** @brief Gets the full day names. C++ counterpart of .NET DateTimeFormatInfo.DayNames. Returns a copy, matching .NET. */
    [[nodiscard]] std::array<std::string, 7> getDayNamesProperty() const { return dayNames_; }
    /** @brief Sets the full day names. */
    void setDayNamesProperty(const std::array<std::string, 7>& value) { VerifyWritable(); dayNames_ = value; }

    /** @brief Gets the shortest unique day name abbreviations. C++ counterpart of .NET DateTimeFormatInfo.ShortestDayNames. Returns a copy, matching .NET. */
    [[nodiscard]] std::array<std::string, 7> getShortestDayNamesProperty() const { return shortestDayNames_; }
    /** @brief Sets the shortest unique day name abbreviations. */
    void setShortestDayNamesProperty(const std::array<std::string, 7>& value) { VerifyWritable(); shortestDayNames_ = value; }

    /** @brief Gets the abbreviated month names. C++ counterpart of .NET DateTimeFormatInfo.AbbreviatedMonthNames. Returns a copy, matching .NET. */
    [[nodiscard]] std::array<std::string, 13> getAbbreviatedMonthNamesProperty() const { return abbreviatedMonthNames_; }
    /** @brief Sets the abbreviated month names. */
    void setAbbreviatedMonthNamesProperty(const std::array<std::string, 13>& value) { VerifyWritable(); abbreviatedMonthNames_ = value; }

    /** @brief Gets the full month names. C++ counterpart of .NET DateTimeFormatInfo.MonthNames. Returns a copy, matching .NET. */
    [[nodiscard]] std::array<std::string, 13> getMonthNamesProperty() const { return monthNames_; }
    /** @brief Sets the full month names. */
    void setMonthNamesProperty(const std::array<std::string, 13>& value) { VerifyWritable(); monthNames_ = value; }

    /** @brief Gets the genitive-form abbreviated month names. C++ counterpart of .NET DateTimeFormatInfo.AbbreviatedMonthGenitiveNames. Returns a copy, matching .NET. */
    [[nodiscard]] std::array<std::string, 13> getAbbreviatedMonthGenitiveNamesProperty() const { return abbreviatedMonthGenitiveNames_; }
    /** @brief Sets the genitive-form abbreviated month names. */
    void setAbbreviatedMonthGenitiveNamesProperty(const std::array<std::string, 13>& value) { VerifyWritable(); abbreviatedMonthGenitiveNames_ = value; }

    /** @brief Gets the genitive-form full month names. C++ counterpart of .NET DateTimeFormatInfo.MonthGenitiveNames. Returns a copy, matching .NET. */
    [[nodiscard]] std::array<std::string, 13> getMonthGenitiveNamesProperty() const { return monthGenitiveNames_; }
    /** @brief Sets the genitive-form full month names. */
    void setMonthGenitiveNamesProperty(const std::array<std::string, 13>& value) { VerifyWritable(); monthGenitiveNames_ = value; }

    /**
     * @brief Gets the full name of the specified day of the week.
     *
     * C++ counterpart of .NET DateTimeFormatInfo.GetDayName(DayOfWeek).
     * @param dayofweek The day of the week.
     * @return The full name of the day.
     * @throws System::ArgumentOutOfRangeException if @p dayofweek is not a valid DayOfWeek
     *         value. Without this check, indexing the internal array with an invalid value
     *         is undefined behavior (std::array::operator[] does not bounds-check), not a
     *         thrown exception -- verified against DateTimeFormatInfo.cs's
     *         `(uint)dow >= (uint)names.Length` check.
     */
    [[nodiscard]] std::string GetDayName(System::DayOfWeek dayofweek) const {
        if (static_cast<size_t>(dayofweek) >= dayNames_.size())
            throw System::ArgumentOutOfRangeException("dayofweek");
        return dayNames_[static_cast<size_t>(dayofweek)];
    }

    /**
     * @brief Gets the abbreviated name of the specified day of the week.
     *
     * C++ counterpart of .NET DateTimeFormatInfo.GetAbbreviatedDayName(DayOfWeek).
     * @param dayofweek The day of the week.
     * @return The abbreviated name of the day.
     * @throws System::ArgumentOutOfRangeException if @p dayofweek is not a valid DayOfWeek value.
     */
    [[nodiscard]] std::string GetAbbreviatedDayName(System::DayOfWeek dayofweek) const {
        if (static_cast<size_t>(dayofweek) >= abbreviatedDayNames_.size())
            throw System::ArgumentOutOfRangeException("dayofweek");
        return abbreviatedDayNames_[static_cast<size_t>(dayofweek)];
    }

    /**
     * @brief Gets the shortest abbreviated name of the specified day of the week.
     *
     * C++ counterpart of .NET DateTimeFormatInfo.GetShortestDayName(DayOfWeek).
     * @param dayOfWeek The day of the week.
     * @return The shortest day name abbreviation.
     * @throws System::ArgumentOutOfRangeException if @p dayOfWeek is not a valid DayOfWeek value.
     */
    [[nodiscard]] std::string GetShortestDayName(System::DayOfWeek dayOfWeek) const {
        if (static_cast<size_t>(dayOfWeek) >= shortestDayNames_.size())
            throw System::ArgumentOutOfRangeException("dayOfWeek");
        return shortestDayNames_[static_cast<size_t>(dayOfWeek)];
    }

    /**
     * @brief Gets the full name of the specified month.
     *
     * C++ counterpart of .NET DateTimeFormatInfo.GetMonthName(int).
     * @param month Month number 1–13.
     * @return The full name of the month.
     */
    [[nodiscard]] std::string GetMonthName(intcs month) const {
        if (month < 1 || month > 13) throw System::ArgumentOutOfRangeException("month");
        return monthNames_[static_cast<size_t>(month - 1)];
    }

    /**
     * @brief Gets the abbreviated name of the specified month.
     *
     * C++ counterpart of .NET DateTimeFormatInfo.GetAbbreviatedMonthName(int).
     * @param month Month number 1–13.
     * @return The abbreviated name of the month.
     */
    [[nodiscard]] std::string GetAbbreviatedMonthName(intcs month) const {
        if (month < 1 || month > 13) throw System::ArgumentOutOfRangeException("month");
        return abbreviatedMonthNames_[static_cast<size_t>(month - 1)];
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
        return {fullDateTimePattern_, longDatePattern_, shortDatePattern_,
                longTimePattern_, shortTimePattern_, monthDayPattern_, yearMonthPattern_};
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
            case 'd': return {shortDatePattern_};
            case 'D': return {longDatePattern_};
            case 'f': return {longDatePattern_ + " " + shortTimePattern_};
            case 'F': return {fullDateTimePattern_};
            case 'g': return {shortDatePattern_ + " " + shortTimePattern_};
            case 'G': return {shortDatePattern_ + " " + longTimePattern_};
            case 'm': case 'M': return {monthDayPattern_};
            case 'r': case 'R': return {getRFC1123PatternProperty()};
            case 's': return {getSortableDateTimePatternProperty()};
            case 't': return {shortTimePattern_};
            case 'T': return {longTimePattern_};
            case 'u': return {getUniversalSortableDateTimePatternProperty()};
            case 'y': case 'Y': return {yearMonthPattern_};
            default: return {};
        }
    }

private:
    bool isReadOnly_{false};

    std::string fullDateTimePattern_{"dddd, dd MMMM yyyy HH:mm:ss"};
    std::string longDatePattern_{"dddd, dd MMMM yyyy"};
    std::string longTimePattern_{"HH:mm:ss"};
    std::string shortDatePattern_{"MM/dd/yyyy"};
    std::string shortTimePattern_{"HH:mm"};
    std::string monthDayPattern_{"MMMM dd"};
    std::string yearMonthPattern_{"yyyy MMMM"};
    std::string dateSeparator_{"/"};
    std::string timeSeparator_{":"};
    std::string amDesignator_{"AM"};
    std::string pmDesignator_{"PM"};

    System::DayOfWeek firstDayOfWeek_{System::DayOfWeek::Sunday};
    CalendarWeekRule calendarWeekRule_{CalendarWeekRule::FirstDay};

    std::array<std::string, 7> abbreviatedDayNames_{
        "Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    std::array<std::string, 7> dayNames_{
        "Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
    std::array<std::string, 7> shortestDayNames_{
        "Su","Mo","Tu","We","Th","Fr","Sa"};

    std::array<std::string, 13> abbreviatedMonthNames_{
        "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec",""};
    std::array<std::string, 13> monthNames_{
        "January","February","March","April","May","June",
        "July","August","September","October","November","December",""};

    std::array<std::string, 13> abbreviatedMonthGenitiveNames_{
        "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec",""};
    std::array<std::string, 13> monthGenitiveNames_{
        "January","February","March","April","May","June",
        "July","August","September","October","November","December",""};

    /** @brief Throws InvalidOperationException if this instance is read-only. C++ counterpart of the internal check .NET applies before every mutating setter. */
    void VerifyWritable() const {
        if (isReadOnly_) throw System::InvalidOperationException("Instance is read-only.");
    }

    static DateTimeFormatInfo makeReadOnly() {
        DateTimeFormatInfo info;
        info.isReadOnly_ = true;
        return info;
    }
};

} // namespace System::Globalization
