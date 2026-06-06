// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System::Collections::Generic {

    /**
     * @brief Defines a key/value pair that can be set or retrieved.
     *
     * @tparam TKey   The type of the key.
     * @tparam TValue The type of the value.
     *
     * @note Status: Implemented
     */
    template<typename TKey, typename TValue>
    struct KeyValuePair {
        TKey  Key;
        TValue Value;

        KeyValuePair() = default;
        KeyValuePair(const TKey& key, const TValue& value) : Key(key), Value(value) {}
        KeyValuePair(TKey&& key, TValue&& value) : Key(std::move(key)), Value(std::move(value)) {}

        bool operator==(const KeyValuePair& other) const {
            return Key == other.Key && Value == other.Value;
        }
        bool operator!=(const KeyValuePair& other) const { return !(*this == other); }
    };

} // namespace System::Collections::Generic
