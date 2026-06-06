// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System::Threading {

    enum class LockRecursionPolicy {
        NoRecursion       = 0,
        SupportsRecursion = 1,
    };

} // namespace System::Threading
