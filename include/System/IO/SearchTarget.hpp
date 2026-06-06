// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System::IO {

    enum class SearchTarget {
        Files       = 1,
        Directories = 2,
        Both        = 3,
    };

} // namespace System::IO
