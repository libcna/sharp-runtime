// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System::IO {

    enum class FileAttributes {
        None               = 0x00000,
        ReadOnly           = 0x00001,
        Hidden             = 0x00002,
        System             = 0x00004,
        Directory          = 0x00010,
        Archive            = 0x00020,
        Device             = 0x00040,
        Normal             = 0x00080,
        Temporary          = 0x00100,
        SparseFile         = 0x00200,
        ReparsePoint       = 0x00400,
        Compressed         = 0x00800,
        Offline            = 0x01000,
        NotContentIndexed  = 0x02000,
        Encrypted          = 0x04000,
        IntegrityStream    = 0x08000,
        NoScrubData        = 0x20000,
    };

    inline FileAttributes operator|(FileAttributes a, FileAttributes b) {
        return static_cast<FileAttributes>(static_cast<int>(a) | static_cast<int>(b));
    }
    inline FileAttributes operator&(FileAttributes a, FileAttributes b) {
        return static_cast<FileAttributes>(static_cast<int>(a) & static_cast<int>(b));
    }
    inline FileAttributes operator~(FileAttributes a) {
        return static_cast<FileAttributes>(~static_cast<int>(a));
    }

} // namespace System::IO
