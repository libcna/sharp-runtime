// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/IO/MatchCasing.hpp"
#include "System/IO/MatchType.hpp"
#include "System/IO/SearchTarget.hpp"

namespace System::IO {

    /// Options for controlling file-system enumeration behaviour.
    class EnumerationOptions {
    public:
        /// When true, subdirectories are searched recursively.
        bool RecurseSubdirectories      = false;
        /// When true, inaccessible directories are silently skipped.
        bool IgnoreInaccessible         = true;
        /// When true, special entries such as "." and ".." are returned.
        bool ReturnSpecialDirectories   = false;
        /// Maximum recursion depth; INT_MAX means unlimited.
        int  MaxRecursionDepth          = INT_MAX;
        /// Controls case sensitivity when matching names.
        MatchCasing  MatchCasingValue   = MatchCasing::PlatformDefault;
        /// Controls how search patterns are interpreted.
        MatchType    MatchTypeValue      = MatchType::Simple;
        /// Specifies whether to return files, directories, or both.
        SearchTarget SearchTargetValue  = SearchTarget::Files;
        /// File attributes that cause an entry to be skipped.
        FileAttributes AttributesToSkip = static_cast<FileAttributes>(0x022); // Hidden | System
        /// File attributes that an entry must have to be returned.
        FileAttributes AttributesToMatch = FileAttributes::Normal;

        /// Initializes EnumerationOptions with default values.
        EnumerationOptions() = default;
    };

} // namespace System::IO
