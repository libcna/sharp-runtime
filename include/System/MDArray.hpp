// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System {

    /** Provides constants for multi-dimensional array rank limits. */
    class MDArray {
    public:
        /** The minimum rank (number of dimensions) for a multi-dimensional array. */
        static constexpr int MinRank = 1;
        /** The maximum rank (number of dimensions) for a multi-dimensional array. */
        static constexpr int MaxRank = 32;
    };

} // namespace System
