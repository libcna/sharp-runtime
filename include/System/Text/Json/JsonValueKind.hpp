// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System::Text::Json {

    enum class JsonValueKind {
        Undefined = 0,
        Object    = 1,
        Array     = 2,
        String    = 3,
        Number    = 4,
        True      = 5,
        False     = 6,
        Null      = 7,
    };

} // namespace System::Text::Json
