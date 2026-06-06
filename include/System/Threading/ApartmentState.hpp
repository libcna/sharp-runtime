// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System::Threading {

    enum class ApartmentState {
        STA     = 0,
        MTA     = 1,
        Unknown = 2,
    };

} // namespace System::Threading
