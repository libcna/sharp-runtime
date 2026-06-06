// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/DateTime.hpp"
#include "System/TimeSpan.hpp"

namespace System {

    class TimeZone {
    public:
        virtual ~TimeZone() = default;

        virtual const std::string& getStandardNameProperty()  const = 0;
        virtual const std::string& getDaylightNameProperty()  const = 0;
        virtual TimeSpan GetUtcOffset(const DateTime& time)   const = 0;
        virtual bool IsDaylightSavingTime(const DateTime& time) const = 0;

        static const TimeZone& CurrentTimeZone();
    };

} // namespace System
