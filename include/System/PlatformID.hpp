// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System {

    enum class PlatformID {
        Win32S       = 0,
        Win32Windows = 1,
        Win32NT      = 2,
        WinCE        = 3,
        Unix         = 4,
        Xbox         = 5,
        MacOSX       = 6,
        Other        = 7,
    };

} // namespace System
