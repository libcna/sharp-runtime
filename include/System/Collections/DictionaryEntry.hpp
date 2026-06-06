// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <any>

namespace System::Collections {

    // Non-generic key/value pair used by IDictionary.
    struct DictionaryEntry {
        std::any Key;
        std::any Value;

        DictionaryEntry() = default;
        DictionaryEntry(std::any key, std::any value) : Key(std::move(key)), Value(std::move(value)) {}
    };

} // namespace System::Collections
