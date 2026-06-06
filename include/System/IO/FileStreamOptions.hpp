// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/IO/FileMode.hpp"
#include "System/IO/FileAccess.hpp"
#include "System/IO/FileShare.hpp"
#include "System/IO/FileOptions.hpp"

namespace System::IO {

    class FileStreamOptions {
    public:
        FileMode   Mode    = FileMode::Open;
        FileAccess Access  = FileAccess::Read;
        FileShare  Share   = FileShare::Read;
        FileOptions Options = FileOptions::None;
        long long  PreallocationSize = 0;
        int        BufferSize = 4096;

        FileStreamOptions() = default;
    };

} // namespace System::IO
