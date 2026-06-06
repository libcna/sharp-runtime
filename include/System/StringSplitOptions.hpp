// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System {

    enum class StringSplitOptions {
        None             = 0,
        RemoveEmptyEntries = 1,
        TrimEntries      = 2,
    };

    inline StringSplitOptions operator|(StringSplitOptions a, StringSplitOptions b) {
        return static_cast<StringSplitOptions>(static_cast<int>(a) | static_cast<int>(b));
    }
    inline StringSplitOptions operator&(StringSplitOptions a, StringSplitOptions b) {
        return static_cast<StringSplitOptions>(static_cast<int>(a) & static_cast<int>(b));
    }

} // namespace System
