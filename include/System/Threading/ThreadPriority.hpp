// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System::Threading {

    enum class ThreadPriority {
        Lowest      = 0,
        BelowNormal = 1,
        Normal      = 2,
        AboveNormal = 3,
        Highest     = 4,
    };

} // namespace System::Threading
