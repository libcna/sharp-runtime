// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System {

    enum class StringComparison {
        CurrentCulture           = 0,
        CurrentCultureIgnoreCase = 1,
        InvariantCulture         = 2,
        InvariantCultureIgnoreCase = 3,
        Ordinal                  = 4,
        OrdinalIgnoreCase        = 5,
    };

} // namespace System
