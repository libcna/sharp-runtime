// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System::Globalization {

    enum class CompareOptions {
        None            = 0x00000000,
        IgnoreCase      = 0x00000001,
        IgnoreNonSpace  = 0x00000002,
        IgnoreSymbols   = 0x00000004,
        IgnoreKanaType  = 0x00000008,
        IgnoreWidth     = 0x00000010,
        NumericOrdering = 0x00000020,
        StringSort      = 0x20000000,
        Ordinal         = 0x40000000,
        OrdinalIgnoreCase = 0x10000000,
    };

    inline CompareOptions operator|(CompareOptions a, CompareOptions b) {
        return static_cast<CompareOptions>(static_cast<int>(a) | static_cast<int>(b));
    }
    inline CompareOptions operator&(CompareOptions a, CompareOptions b) {
        return static_cast<CompareOptions>(static_cast<int>(a) & static_cast<int>(b));
    }

} // namespace System::Globalization
