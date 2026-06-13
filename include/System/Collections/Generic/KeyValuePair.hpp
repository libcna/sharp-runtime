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
        /// The key component of the pair.
        TKey  Key;
        /// The value component of the pair.
        TValue Value;

        /// Default-constructs a KeyValuePair with default-initialized key and value.
        KeyValuePair() = default;
        /// Constructs a KeyValuePair with copies of the given key and value.
        KeyValuePair(const TKey& key, const TValue& value) : Key(key), Value(value) {}
        /// Constructs a KeyValuePair by moving the given key and value.
        KeyValuePair(TKey&& key, TValue&& value) : Key(std::move(key)), Value(std::move(value)) {}

        /// Returns true if both key and value compare equal to the other pair.
        bool operator==(const KeyValuePair& other) const {
            return Key == other.Key && Value == other.Value;
        }
        /// Returns true if the pairs are not equal.
        bool operator!=(const KeyValuePair& other) const { return !(*this == other); }
    };

} // namespace System::Collections::Generic
