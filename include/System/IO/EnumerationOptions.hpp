// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/IO/MatchCasing.hpp"
#include "System/IO/MatchType.hpp"
#include "System/IO/SearchTarget.hpp"

namespace System::IO {

    class EnumerationOptions {
    public:
        bool RecurseSubdirectories      = false;
        bool IgnoreInaccessible         = true;
        bool ReturnSpecialDirectories   = false;
        int  MaxRecursionDepth          = INT_MAX;
        MatchCasing  MatchCasingValue   = MatchCasing::PlatformDefault;
        MatchType    MatchTypeValue      = MatchType::Simple;
        SearchTarget SearchTargetValue  = SearchTarget::Files;
        FileAttributes AttributesToSkip = static_cast<FileAttributes>(0x022); // Hidden | System
        FileAttributes AttributesToMatch = FileAttributes::Normal;

        EnumerationOptions() = default;
    };

} // namespace System::IO
