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
        DateOnly() = default;
        DateOnly(intcs year, intcs month, intcs day) : year_(year), month_(month), day_(day) {}

        [[nodiscard]] intcs getYearProperty()  const { return year_; }
        [[nodiscard]] intcs getMonthProperty() const { return month_; }
        [[nodiscard]] intcs getDayProperty()   const { return day_; }

        [[nodiscard]] std::string ToString() const {
            std::ostringstream oss;
            oss << year_ << '-'
                << std::setw(2) << std::setfill('0') << month_ << '-'
                << std::setw(2) << std::setfill('0') << day_;
            return oss.str();
        }

        bool operator==(const DateOnly& o) const { return year_==o.year_ && month_==o.month_ && day_==o.day_; }
        bool operator!=(const DateOnly& o) const { return !(*this==o); }
        bool operator< (const DateOnly& o) const { return toDays() <  o.toDays(); }
        bool operator<=(const DateOnly& o) const { return toDays() <= o.toDays(); }
        bool operator> (const DateOnly& o) const { return toDays() >  o.toDays(); }
        bool operator>=(const DateOnly& o) const { return toDays() >= o.toDays(); }

    private:
        intcs toDays() const { return year_*10000 + month_*100 + day_; }
    };

} // namespace System
