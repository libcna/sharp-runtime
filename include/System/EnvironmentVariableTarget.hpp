// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System {

    enum class EnvironmentVariableTarget {
        Process = 0,
        User    = 1,
        Machine = 2,
    };

} // namespace System
