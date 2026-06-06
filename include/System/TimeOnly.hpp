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
        intcs hour_   = 0;
        intcs minute_ = 0;
        intcs second_ = 0;
        intcs ms_     = 0;
    public:
        TimeOnly() = default;
        TimeOnly(intcs hour, intcs minute) : hour_(hour), minute_(minute) {}
        TimeOnly(intcs hour, intcs minute, intcs second) : hour_(hour), minute_(minute), second_(second) {}
        TimeOnly(intcs hour, intcs minute, intcs second, intcs millisecond)
            : hour_(hour), minute_(minute), second_(second), ms_(millisecond) {}

        [[nodiscard]] intcs getHourProperty()        const { return hour_; }
        [[nodiscard]] intcs getMinuteProperty()      const { return minute_; }
        [[nodiscard]] intcs getSecondProperty()      const { return second_; }
        [[nodiscard]] intcs getMillisecondProperty() const { return ms_; }

        [[nodiscard]] std::string ToString() const {
            std::ostringstream oss;
            oss << std::setw(2) << std::setfill('0') << hour_ << ':'
                << std::setw(2) << std::setfill('0') << minute_ << ':'
                << std::setw(2) << std::setfill('0') << second_;
            return oss.str();
        }

        bool operator==(const TimeOnly& o) const { return hour_==o.hour_ && minute_==o.minute_ && second_==o.second_ && ms_==o.ms_; }
        bool operator!=(const TimeOnly& o) const { return !(*this==o); }
        bool operator< (const TimeOnly& o) const { return toMs() <  o.toMs(); }
        bool operator<=(const TimeOnly& o) const { return toMs() <= o.toMs(); }
        bool operator> (const TimeOnly& o) const { return toMs() >  o.toMs(); }
        bool operator>=(const TimeOnly& o) const { return toMs() >= o.toMs(); }

    private:
        intcs toMs() const { return ((hour_*60+minute_)*60+second_)*1000+ms_; }
    };

} // namespace System
