// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System::IO {

    enum class HandleInheritability {
        None        = 0,
        Inheritable = 1,
    };

} // namespace System::IO
