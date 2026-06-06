// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System::Globalization {

    enum class DateTimeStyles {
        None                 = 0x00000000,
        AllowLeadingWhite    = 0x00000001,
        AllowTrailingWhite   = 0x00000002,
        AllowInnerWhite      = 0x00000004,
        AllowWhiteSpaces     = 0x00000007, // AllowLeadingWhite | AllowInnerWhite | AllowTrailingWhite
        NoCurrentDateDefault = 0x00000008,
        AdjustToUniversal    = 0x00000010,
        AssumeLocal          = 0x00000020,
        AssumeUniversal      = 0x00000040,
        RoundtripKind        = 0x00000080,
    };

    inline DateTimeStyles operator|(DateTimeStyles a, DateTimeStyles b) {
        return static_cast<DateTimeStyles>(static_cast<int>(a) | static_cast<int>(b));
    }
    inline DateTimeStyles operator&(DateTimeStyles a, DateTimeStyles b) {
        return static_cast<DateTimeStyles>(static_cast<int>(a) & static_cast<int>(b));
    }

} // namespace System::Globalization
