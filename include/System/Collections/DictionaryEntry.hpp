// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <any>

namespace System::Collections {

    /** Non-generic key/value pair used by IDictionary. */
    struct DictionaryEntry {
        /** The key of the entry. */
        std::any Key;
        /** The value of the entry. */
        std::any Value;

        /** Default-constructs a DictionaryEntry with empty key and value. */
        DictionaryEntry() = default;
        /** Constructs a DictionaryEntry with the given key and value. */
        DictionaryEntry(std::any key, std::any value) : Key(std::move(key)), Value(std::move(value)) {}
    };

} // namespace System::Collections
