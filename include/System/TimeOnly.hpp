// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include <sstream>
#include <iomanip>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/TimeSpan.hpp"

namespace System {

    class DateTime; // forward declaration for FromDateTime

    using SharpRuntime::intcs;

    /**
     * @brief Represents a time of day, independent of date (hours, minutes, seconds, milliseconds).
     *
     * C++ counterpart of .NET System.TimeOnly.
     *
     * @note Status: Done
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
        TimeOnly(intcs hour, intcs minute) : hour_(hour), minute_(minute) {}
        /// @brief Constructs a TimeOnly from hours, minutes, and seconds.
        TimeOnly(intcs hour, intcs minute, intcs second) : hour_(hour), minute_(minute), second_(second) {}
        /// @brief Constructs a TimeOnly from hours, minutes, seconds, and milliseconds.
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

        /// Returns the time formatted according to @p format.
        /// Tokens: HH/H (24h hour), hh/h (12h hour), mm/m (minute), ss/s (second), fff/ff/f (ms).
        [[nodiscard]] std::string ToString(const std::string& format) const;

        /// Returns a new TimeOnly with @p n hours added (wraps around midnight).
        [[nodiscard]] TimeOnly AddHours(int n) const;

        /// Returns a new TimeOnly with @p n minutes added (wraps around midnight).
        [[nodiscard]] TimeOnly AddMinutes(int n) const;

        /// Returns a TimeOnly representing the time-of-day component of @p ts.
        [[nodiscard]] static TimeOnly FromTimeSpan(const TimeSpan& ts);

        /// Returns a TimeSpan representing the elapsed time since midnight.
        [[nodiscard]] TimeSpan ToTimeSpan() const {
            return TimeSpan(0, hour_, minute_, second_, ms_);
        }

        /// Extracts the time-of-day part from the specified DateTime.
        [[nodiscard]] static TimeOnly FromDateTime(const DateTime& dt);

        /// Parses a time string in the form "HH:MM:SS" or "HH:MM:SS.fff".
        /// @throws System::FormatException on failure.
        [[nodiscard]] static TimeOnly Parse(const std::string& s);

        /// Tries to parse a time string in the form "HH:MM:SS[.fff]"; returns false on failure.
        static bool TryParse(const std::string& s, TimeOnly& result);

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
