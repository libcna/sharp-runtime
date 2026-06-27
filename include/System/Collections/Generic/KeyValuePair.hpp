// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <utility>

namespace System::Collections::Generic {

/**
 * @brief Defines a key/value pair that can be set or retrieved.
 *
 * C++ counterpart of .NET System.Collections.Generic.KeyValuePair<TKey,TValue>.
 * Used as the element type of dictionary enumerators and read-only dictionary interfaces.
 *
 * @tparam TKey   The type of the key.
 * @tparam TValue The type of the value.
 */
template<typename TKey, typename TValue>
struct KeyValuePair {
    /** @brief The key component of the pair. */
    TKey Key{};
    /** @brief The value component of the pair. */
    TValue Value{};

    /** @brief Value-initializes a KeyValuePair (primitives zeroed, objects default-constructed). */
    KeyValuePair() = default;

    /**
     * @brief Constructs a KeyValuePair with copies of the given key and value.
     * @param key   The key.
     * @param value The value.
     */
    KeyValuePair(const TKey& key, const TValue& value) : Key(key), Value(value) {}

    /**
     * @brief Constructs a KeyValuePair by moving the given key and value.
     * @param key   The key (moved).
     * @param value The value (moved).
     */
    KeyValuePair(TKey&& key, TValue&& value) : Key(std::move(key)), Value(std::move(value)) {}

    /**
     * @brief Returns true if both key and value compare equal to those of the other pair.
     * @param other The pair to compare to.
     * @return true if Key == other.Key and Value == other.Value; otherwise false.
     */
    bool operator==(const KeyValuePair& other) const {
        return Key == other.Key && Value == other.Value;
    }

    /**
     * @brief Returns true if either key or value differs from the other pair.
     * @param other The pair to compare to.
     * @return true if the pairs are not equal; otherwise false.
     */
    bool operator!=(const KeyValuePair& other) const { return !(*this == other); }
};

} // namespace System::Collections::Generic
