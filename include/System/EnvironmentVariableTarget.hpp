// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System {

    /// Specifies the location where an environment variable is stored.
    enum class EnvironmentVariableTarget {
        /// The environment variable is stored in the process block.
        Process = 0,
        /// The environment variable is stored in the user-level registry key.
        User    = 1,
        /// The environment variable is stored in the machine-level registry key.
        Machine = 2,
    };

} // namespace System
