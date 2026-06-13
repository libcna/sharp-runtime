// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/DateTime.hpp"
#include "System/TimeSpan.hpp"

namespace System {

    /// Abstract base class representing a time zone, mirroring .NET System.TimeZone.
    class TimeZone {
    public:
        /// Virtual destructor.
        virtual ~TimeZone() = default;

        /// Returns the standard (non-daylight-saving) name of the time zone.
        virtual const std::string& getStandardNameProperty()  const = 0;
        /// Returns the daylight-saving name of the time zone.
        virtual const std::string& getDaylightNameProperty()  const = 0;
        /// @brief Returns the UTC offset for the given local @p time.
        /// @param time Local date/time whose offset is requested.
        virtual TimeSpan GetUtcOffset(const DateTime& time)   const = 0;
        /// @brief Returns true if @p time falls within daylight saving time.
        /// @param time Local date/time to test.
        virtual bool IsDaylightSavingTime(const DateTime& time) const = 0;

        /// Returns the current local time zone.
        static const TimeZone& CurrentTimeZone();
    };

} // namespace System
