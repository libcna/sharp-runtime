// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Created by robertvokac on 6/7/25.
//

#pragma once

#include <string>

#include "System/Object.hpp"
#include "System/DateTime.hpp"
#include "System/TimeSpan.hpp"
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System {

    using SharpRuntime::intcs;
    using SharpRuntime::longcs;

    /**
     * @brief Represents a point in time relative to UTC.
     *
     * This is a partial C++ counterpart of the .NET System::DateTimeOffset structure.
     *
     * @note Status: Partial.
     * @note This implementation stores a DateTime value and its UTC offset.
     */
    class DateTimeOffset : public Object {
    private:
        DateTime dateTime_;
        TimeSpan offset_;

    public:
        DateTimeOffset();
        DateTimeOffset(const DateTime& dateTime, const TimeSpan& offset);

        // Static factory
        [[nodiscard]] static DateTimeOffset getNowProperty();
        [[nodiscard]] static DateTimeOffset getUtcNowProperty();

        // Component accessors
        [[nodiscard]] const DateTime& getDateTimeProperty() const;
        [[nodiscard]] const TimeSpan& getOffsetProperty() const;
        [[nodiscard]] longcs getUtcTicksProperty() const;
        [[nodiscard]] intcs getYearProperty()        const;
        [[nodiscard]] intcs getMonthProperty()       const;
        [[nodiscard]] intcs getDayProperty()         const;
        [[nodiscard]] intcs getHourProperty()        const;
        [[nodiscard]] intcs getMinuteProperty()      const;
        [[nodiscard]] intcs getSecondProperty()      const;
        [[nodiscard]] intcs getMillisecondProperty() const;
        [[nodiscard]] DateTime getDateProperty()          const;
        [[nodiscard]] DateTime getUtcDateTimeProperty()   const;

        // Arithmetic
        [[nodiscard]] DateTimeOffset Add(const TimeSpan& ts) const;
        [[nodiscard]] DateTimeOffset AddDays(double days) const;
        [[nodiscard]] DateTimeOffset AddHours(double hours) const;
        [[nodiscard]] DateTimeOffset AddMinutes(double minutes) const;
        [[nodiscard]] DateTimeOffset AddSeconds(double seconds) const;
        [[nodiscard]] DateTimeOffset AddMilliseconds(double ms) const;
        [[nodiscard]] DateTimeOffset AddMonths(intcs months) const;
        [[nodiscard]] DateTimeOffset AddYears(intcs years) const;
        [[nodiscard]] TimeSpan Subtract(const DateTimeOffset& other) const;
        [[nodiscard]] DateTimeOffset Subtract(const TimeSpan& ts) const;
        [[nodiscard]] DateTimeOffset ToUniversalTime() const;

        // Parsing / formatting
        [[nodiscard]] static DateTimeOffset Parse(const std::string& s);
        static bool TryParse(const std::string& s, DateTimeOffset& result);
        [[nodiscard]] std::string ToString() const override;
        [[nodiscard]] std::string ToString(const std::string& format) const;

        using Object::Equals;

        // Comparison
        [[nodiscard]] intcs CompareTo(const DateTimeOffset& other) const;
        [[nodiscard]] bool Equals(const DateTimeOffset& other) const;
        bool operator==(const DateTimeOffset& other) const;
        bool operator!=(const DateTimeOffset& other) const;
        bool operator< (const DateTimeOffset& other) const;
        bool operator<=(const DateTimeOffset& other) const;
        bool operator> (const DateTimeOffset& other) const;
        bool operator>=(const DateTimeOffset& other) const;

        // Operators
        [[nodiscard]] DateTimeOffset operator+(const TimeSpan& ts) const;
        [[nodiscard]] DateTimeOffset operator-(const TimeSpan& ts) const;
        [[nodiscard]] TimeSpan       operator-(const DateTimeOffset& other) const;

        GetTypeNameHPP()
    };

} // namespace System