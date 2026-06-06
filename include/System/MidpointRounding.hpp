// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System {

    enum class MidpointRounding {
        ToEven             = 0,
        AwayFromZero       = 1,
        ToZero             = 2,
        ToNegativeInfinity = 3,
        ToPositiveInfinity = 4,
    };

} // namespace System
