// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include <sstream>
#include <iomanip>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System {

    using SharpRuntime::intcs;

    /**
     * @brief Represents a date (year, month, day) with no time or time-zone information.
     *
     * Partial C++ counterpart of .NET System.DateOnly.
     *
     * @note Status: Partial
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
