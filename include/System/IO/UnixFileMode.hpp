// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System::IO {

    enum class UnixFileMode {
        None             = 0,
        OtherExecute     = 0001,
        OtherWrite       = 0002,
        OtherRead        = 0004,
        GroupExecute     = 0010,
        GroupWrite       = 0020,
        GroupRead        = 0040,
        UserExecute      = 0100,
        UserWrite        = 0200,
        UserRead         = 0400,
        StickyBit        = 01000,
        SetGroup         = 02000,
        SetUser          = 04000,
    };

    inline UnixFileMode operator|(UnixFileMode a, UnixFileMode b) {
        return static_cast<UnixFileMode>(static_cast<int>(a) | static_cast<int>(b));
    }
    inline UnixFileMode operator&(UnixFileMode a, UnixFileMode b) {
        return static_cast<UnixFileMode>(static_cast<int>(a) & static_cast<int>(b));
    }

} // namespace System::IO
