// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/ILocalTimeZone.hpp"
#include <string>
#include "System/DateTime.hpp"
#include "System/TimeSpan.hpp"

namespace System {

    /**
     * @brief Represents a time zone.
     *
     * C++ counterpart of .NET System.TimeZone (abstract base class).
     * Concrete subclasses implement GetUtcOffset and IsDaylightSavingTime.
     * Prefer TimeZoneInfo for new code; TimeZone is provided for compatibility.
     */
    /**
     * @note #1941 phase 2 made this an `ILocalTimeZone` (`Core.Base`). The two public pure-virtual
     * members were already declared here; the interface additionally supplies a UTC-instant
     * conversion query with a fixed-zone default. The system adapter overrides that query so DST
     * boundaries are resolved as instants without exposing a `TimeZone` type to `Core.Base`.
     */
    class TimeZone : public System::ILocalTimeZone {
    public:
        /** @brief Virtual destructor. */
        virtual ~TimeZone() = default;

        /**
         * @brief Returns the standard (non-daylight-saving) name of the time zone.
         *
         * C++ counterpart of .NET TimeZone.StandardName.
         */
        virtual const std::string& getStandardNameProperty()  const = 0;

        /**
         * @brief Returns the daylight-saving name of the time zone.
         *
         * C++ counterpart of .NET TimeZone.DaylightName.
         */
        virtual const std::string& getDaylightNameProperty()  const = 0;

        /**
         * @brief Returns the UTC offset for the given DateTime.
         *
         * C++ counterpart of .NET TimeZone.GetUtcOffset(DateTime).
         * A Utc value returns zero. Local and Unspecified values are interpreted as local
         * wall-clock times, including daylight time where the implementation models it.
         * @param time Date/time whose offset is requested.
         */
        TimeSpan GetUtcOffset(const DateTime& time)   const override = 0;

        /**
         * @brief Returns true if the specified time falls within a daylight saving time period.
         *
         * C++ counterpart of .NET TimeZone.IsDaylightSavingTime(DateTime).
         * @param time Local date/time to test.
         */
        bool IsDaylightSavingTime(const DateTime& time) const override = 0;

        /**
         * @brief Returns the current local time zone.
         *
         * C++ counterpart of .NET TimeZone.CurrentTimeZone.
         */
        static const TimeZone& CurrentTimeZone();
    };

} // namespace System
