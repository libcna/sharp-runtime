// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System::IO {

    /**
     * @brief Specifies the position in a stream to use for seeking.
     *
     * @note Status: Implemented
     */
    enum class SeekOrigin : int {
        Begin   = 0,
        Current = 1,
        End     = 2
    };

} // namespace System::IO
