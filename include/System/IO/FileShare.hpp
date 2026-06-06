// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System::IO {

    /**
     * @brief Contains constants for controlling the kind of access other
     * FileStream objects can have to the same file.
     *
     * @note Status: Implemented
     */
    enum class FileShare : int {
        None        = 0,
        Read        = 1,
        Write       = 2,
        ReadWrite   = 3,
        Delete      = 4,
        Inheritable = 16
    };

} // namespace System::IO
