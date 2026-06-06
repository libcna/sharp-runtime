// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System::Threading {

    enum class LazyThreadSafetyMode {
        None                   = 0,
        PublicationOnly        = 1,
        ExecutionAndPublication = 2,
    };

} // namespace System::Threading
