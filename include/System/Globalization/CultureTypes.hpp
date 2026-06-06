// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System::Globalization {

    enum class CultureTypes {
        NeutralCultures         = 0x0001,
        SpecificCultures        = 0x0002,
        InstalledWin32Cultures  = 0x0004,
        AllCultures             = 0x0007,
        UserCustomCulture       = 0x0008,
        ReplacementCultures     = 0x0010,
        WindowsOnlyCultures     = 0x0020,
        FrameworkCultures       = 0x0040,
    };

    inline CultureTypes operator|(CultureTypes a, CultureTypes b) {
        return static_cast<CultureTypes>(static_cast<int>(a) | static_cast<int>(b));
    }
    inline CultureTypes operator&(CultureTypes a, CultureTypes b) {
        return static_cast<CultureTypes>(static_cast<int>(a) & static_cast<int>(b));
    }

} // namespace System::Globalization
