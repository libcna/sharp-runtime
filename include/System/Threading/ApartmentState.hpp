// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System::Threading {

    /// Specifies the apartment state of a managed thread.
    enum class ApartmentState {
        /// The thread will create and enter a single-threaded apartment.
        STA     = 0,
        /// The thread will create and enter a multi-threaded apartment.
        MTA     = 1,
        /// The apartment state has not been set.
        Unknown = 2,
    };

} // namespace System::Threading
