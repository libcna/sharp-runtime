// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System::Globalization {

    enum class CalendarAlgorithmType {
        Unknown        = 0,
        SolarCalendar  = 1,
        LunarCalendar  = 2,
        LunisolarCalendar = 3,
    };

} // namespace System::Globalization
