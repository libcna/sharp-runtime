// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include <sstream>
#include <iomanip>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System {

    class DateTime; // forward declaration for FromDateTime

    using SharpRuntime::intcs;

    /**
     * @brief Represents a date (year, month, day) with no time or time-zone information.
     *
     * C++ counterpart of .NET System.DateOnly.
     *
     * @note Status: Done
     */
    class DateOnly {
        intcs year_  = 1;
        intcs month_ = 1;
        intcs day_   = 1;
    public:
        /// Initializes a new DateOnly with year=1, month=1, day=1.
        DateOnly() = default;
        /// Initializes a new DateOnly with the specified year, month, and day.
        DateOnly(intcs year, intcs month, intcs day) : year_(year), month_(month), day_(day) {}

        /// Returns the year component of this date.
        [[nodiscard]] intcs getYearProperty()  const { return year_; }
        /// Returns the month component of this date.
        [[nodiscard]] intcs getMonthProperty() const { return month_; }
        /// Returns the day component of this date.
        [[nodiscard]] intcs getDayProperty()   const { return day_; }

        /// Returns a string representation in the form "yyyy-MM-dd".
        [[nodiscard]] std::string ToString() const {
            std::ostringstream oss;
            oss << year_ << '-'
                << std::setw(2) << std::setfill('0') << month_ << '-'
                << std::setw(2) << std::setfill('0') << day_;
            return oss.str();
        }

        /// Returns the date formatted according to @p format.
        /// Tokens: yyyy/yy (year), MM/M (month), dd/d (day). Literals in single quotes.
        [[nodiscard]] std::string ToString(const std::string& format) const;

        /// Returns a new DateOnly with @p n days added (may be negative).
        [[nodiscard]] DateOnly AddDays(int n) const;

        /// Returns a new DateOnly with @p n months added (may be negative; clamps day to end of month).
        [[nodiscard]] DateOnly AddMonths(int n) const;

        /// Returns a new DateOnly with @p n years added (may be negative).
        [[nodiscard]] DateOnly AddYears(int n) const;

        /// Extracts the date part from the specified DateTime.
        [[nodiscard]] static DateOnly FromDateTime(const DateTime& dt);

        /// Parses a date string in the form "yyyy-MM-dd".
        /// @throws System::FormatException on failure.
        [[nodiscard]] static DateOnly Parse(const std::string& s);

        /// Tries to parse a date string in the form "yyyy-MM-dd"; returns false on failure.
        static bool TryParse(const std::string& s, DateOnly& result);

        /// Returns true if this date equals the specified date.
        bool operator==(const DateOnly& o) const { return year_==o.year_ && month_==o.month_ && day_==o.day_; }
        /// Returns true if this date does not equal the specified date.
        bool operator!=(const DateOnly& o) const { return !(*this==o); }
        /// Returns true if this date is earlier than the specified date.
        bool operator< (const DateOnly& o) const { return toDays() <  o.toDays(); }
        /// Returns true if this date is earlier than or equal to the specified date.
        bool operator<=(const DateOnly& o) const { return toDays() <= o.toDays(); }
        /// Returns true if this date is later than the specified date.
        bool operator> (const DateOnly& o) const { return toDays() >  o.toDays(); }
        /// Returns true if this date is later than or equal to the specified date.
        bool operator>=(const DateOnly& o) const { return toDays() >= o.toDays(); }

    private:
        intcs toDays() const { return year_*10000 + month_*100 + day_; }
    };

} // namespace System
