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
        /** Opens the file for reading only. */
        Read      = 1,
        /** Opens the file for writing only. */
        Write     = 2,
        /** Opens the file for both reading and writing. */
        ReadWrite = 3
    };

} // namespace System::IO
