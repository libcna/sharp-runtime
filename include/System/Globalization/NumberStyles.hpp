// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System::Globalization {

    enum class NumberStyles {
        None                = 0x00000000,
        AllowLeadingWhite   = 0x00000001,
        AllowTrailingWhite  = 0x00000002,
        AllowLeadingSign    = 0x00000004,
        AllowTrailingSign   = 0x00000008,
        AllowParentheses    = 0x00000010,
        AllowDecimalPoint   = 0x00000020,
        AllowThousands      = 0x00000040,
        AllowExponent       = 0x00000080,
        AllowCurrencySymbol = 0x00000100,
        AllowHexSpecifier   = 0x00000200,
        AllowBinarySpecifier = 0x00000400,
        Integer    = 0x00000007, // AllowLeadingWhite|AllowTrailingWhite|AllowLeadingSign
        HexNumber  = 0x00000203, // AllowLeadingWhite|AllowTrailingWhite|AllowHexSpecifier
        BinaryNumber = 0x00000403,
        Number     = 0x0000006F, // AllowLeadingWhite|AllowTrailingWhite|AllowLeadingSign|AllowTrailingSign|AllowDecimalPoint|AllowThousands
        Float      = 0x000000A7, // AllowLeadingWhite|AllowTrailingWhite|AllowLeadingSign|AllowDecimalPoint|AllowExponent
        Currency   = 0x000001FF,
        Any        = 0x000001FF | 0x00000080,
    };

    inline NumberStyles operator|(NumberStyles a, NumberStyles b) {
        return static_cast<NumberStyles>(static_cast<int>(a) | static_cast<int>(b));
    }
    inline NumberStyles operator&(NumberStyles a, NumberStyles b) {
        return static_cast<NumberStyles>(static_cast<int>(a) & static_cast<int>(b));
    }

} // namespace System::Globalization
