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
     * @brief Represents a time of day, independent of date (hours, minutes, seconds, milliseconds).
     *
     * Partial C++ counterpart of .NET System.TimeOnly.
     *
     * @note Status: Partial
     */
    class TimeOnly {
        intcs hour_   = 0; ///< Hour component (0–23).
        intcs minute_ = 0; ///< Minute component (0–59).
        intcs second_ = 0; ///< Second component (0–59).
        intcs ms_     = 0; ///< Millisecond component (0–999).
    public:
        /// Constructs a TimeOnly at midnight (00:00:00.000).
        TimeOnly() = default;
        /// @brief Constructs a TimeOnly from hours and minutes.
        /// @param hour Hour (0–23).
        /// @param minute Minute (0–59).
        TimeOnly(intcs hour, intcs minute) : hour_(hour), minute_(minute) {}
        /// @brief Constructs a TimeOnly from hours, minutes, and seconds.
        /// @param hour Hour (0–23).
        /// @param minute Minute (0–59).
        /// @param second Second (0–59).
        TimeOnly(intcs hour, intcs minute, intcs second) : hour_(hour), minute_(minute), second_(second) {}
        /// @brief Constructs a TimeOnly from hours, minutes, seconds, and milliseconds.
        /// @param hour Hour (0–23).
        /// @param minute Minute (0–59).
        /// @param second Second (0–59).
        /// @param millisecond Millisecond (0–999).
        TimeOnly(intcs hour, intcs minute, intcs second, intcs millisecond)
            : hour_(hour), minute_(minute), second_(second), ms_(millisecond) {}

        /// Returns the hour component.
        [[nodiscard]] intcs getHourProperty()        const { return hour_; }
        /// Returns the minute component.
        [[nodiscard]] intcs getMinuteProperty()      const { return minute_; }
        /// Returns the second component.
        [[nodiscard]] intcs getSecondProperty()      const { return second_; }
        /// Returns the millisecond component.
        [[nodiscard]] intcs getMillisecondProperty() const { return ms_; }

        /// Returns the time formatted as "HH:MM:SS".
        [[nodiscard]] std::string ToString() const {
            std::ostringstream oss;
            oss << std::setw(2) << std::setfill('0') << hour_ << ':'
                << std::setw(2) << std::setfill('0') << minute_ << ':'
                << std::setw(2) << std::setfill('0') << second_;
            return oss.str();
        }

        /// Returns true if this TimeOnly is equal to @p o.
        bool operator==(const TimeOnly& o) const { return hour_==o.hour_ && minute_==o.minute_ && second_==o.second_ && ms_==o.ms_; }
        /// Returns true if this TimeOnly is not equal to @p o.
        bool operator!=(const TimeOnly& o) const { return !(*this==o); }
        /// Returns true if this TimeOnly is earlier than @p o.
        bool operator< (const TimeOnly& o) const { return toMs() <  o.toMs(); }
        /// Returns true if this TimeOnly is earlier than or equal to @p o.
        bool operator<=(const TimeOnly& o) const { return toMs() <= o.toMs(); }
        /// Returns true if this TimeOnly is later than @p o.
        bool operator> (const TimeOnly& o) const { return toMs() >  o.toMs(); }
        /// Returns true if this TimeOnly is later than or equal to @p o.
        bool operator>=(const TimeOnly& o) const { return toMs() >= o.toMs(); }

    private:
        intcs toMs() const { return ((hour_*60+minute_)*60+second_)*1000+ms_; }
    };

} // namespace System
