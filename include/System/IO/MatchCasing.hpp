// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System::IO {

    enum class MatchCasing {
        PlatformDefault  = 0,
        CaseSensitive    = 1,
        CaseInsensitive  = 2,
    };

} // namespace System::IO
