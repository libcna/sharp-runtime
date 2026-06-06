// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System::IO {

    enum class FileOptions {
        None           = 0,
        WriteThrough   = static_cast<int>(0x80000000u),
        RandomAccess   = 0x10000000,
        DeleteOnClose  = 0x04000000,
        SequentialScan = 0x08000000,
    };

    inline FileOptions operator|(FileOptions a, FileOptions b) {
        return static_cast<FileOptions>(static_cast<int>(a) | static_cast<int>(b));
    }
    inline FileOptions operator&(FileOptions a, FileOptions b) {
        return static_cast<FileOptions>(static_cast<int>(a) & static_cast<int>(b));
    }

} // namespace System::IO
