// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System::Threading {

    enum class EventResetMode {
        AutoReset   = 0,
        ManualReset = 1,
    };

} // namespace System::Threading
