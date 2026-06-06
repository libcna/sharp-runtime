// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System::IO {

    /**
     * @brief Defines constants for read, write, or read/write access to a file.
     *
     * @note Status: Implemented
     */
    enum class FileAccess : int {
        Read      = 1,
        Write     = 2,
        ReadWrite = 3
    };

} // namespace System::IO
