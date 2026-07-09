// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <map>
#include <vector>
#include <stdexcept>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Collections/Generic/KeyNotFoundException.hpp"

namespace System::Collections::Generic {

using SharpRuntime::intcs;

/**
 * @brief Represents a collection of key/value pairs sorted by key and accessible by key or index.
 *
 * C++ counterpart of .NET System.Collections.Generic.SortedList<TKey,TValue>.
 * Backed by std::map<TKey,TValue>; provides O(log n) key access and O(n) index access.
 *
 * @note Unlike .NET's SortedList<TKey,TValue>, iterators do not detect concurrent
 * modification: .NET throws InvalidOperationException if the list is structurally
 * modified while an enumerator is active, but sharp-runtime's iterators follow
 * plain std::map invalidation rules instead. Do not mutate the list while
 * iterating it directly.
 *
 * @tparam TKey   The type of the keys (must support operator<).
 * @tparam TValue The type of the values.
 */
template<typename TKey, typename TValue>
class SortedList {
    std::map<TKey, TValue> map_;

public:
    /** @brief Initializes a new empty SortedList. */
    SortedList() = default;

    /**
     * @brief Gets the number of key/value pairs in the SortedList.
     *
     * C++ counterpart of .NET SortedList<TKey,TValue>.Count.
     * @return The number of elements.
     */
    [[nodiscard]] intcs getCountProperty() const { return static_cast<intcs>(map_.size()); }

    /**
     * @brief Gets or sets the value associated with the specified key.
     *
     * C++ counterpart of .NET SortedList<TKey,TValue>.Item[TKey].
     * Inserts a default value if the key is absent.
     * @param key The key whose value to get or set.
     * @return A reference to the associated value.
     */
    TValue& operator[](const TKey& key) { return map_[key]; }

    /**
     * @brief Gets the value associated with the specified key (const).
     *
     * C++ counterpart of .NET SortedList<TKey,TValue>.Item[TKey] getter.
     * @param key The key whose value to get.
     * @return A const reference to the associated value.
     * @throws System::Collections::Generic::KeyNotFoundException if the key is not found.
     */
    [[nodiscard]] const TValue& operator[](const TKey& key) const {
        auto it = map_.find(key);
        if (it == map_.end()) throw KeyNotFoundException("The given key was not present in the dictionary.");
        return it->second;
    }

    /**
     * @brief Adds the specified key and value to the list.
     *
     * C++ counterpart of .NET SortedList<TKey,TValue>.Add(TKey, TValue).
     * @param key   The key of the element to add.
     * @param value The value of the element to add.
     * @throws System::ArgumentException if the key already exists.
     */
    void Add(const TKey& key, const TValue& value) {
        if (map_.count(key)) throw System::ArgumentException("An item with the same key has already been added.");
        map_[key] = value;
    }

    /**
     * @brief Attempts to add the specified key and value to the list.
     *
     * @param key   The key of the element to add.
     * @param value The value of the element to add.
     * @return true if the element was added; false if the key already existed.
     */
    bool TryAdd(const TKey& key, const TValue& value) {
        if (map_.count(key)) return false;
        map_[key] = value;
        return true;
    }

    /**
     * @brief Removes the element with the specified key.
     *
     * C++ counterpart of .NET SortedList<TKey,TValue>.Remove(TKey).
     * @param key The key of the element to remove.
     * @return true if the element was removed; otherwise false.
     */
    bool Remove(const TKey& key) { return map_.erase(key) > 0; }

    /**
     * @brief Removes the element at the specified zero-based index.
     *
     * C++ counterpart of .NET SortedList<TKey,TValue>.RemoveAt(int).
     * @param index The index of the element to remove.
     * @throws System::ArgumentOutOfRangeException if the index is out of range.
     */
    void RemoveAt(intcs index) {
        if (index < 0 || index >= getCountProperty()) throw System::ArgumentOutOfRangeException("index");
        auto it = map_.begin();
        std::advance(it, index);
        map_.erase(it);
    }

    /**
     * @brief Determines whether the list contains the specified key.
     *
     * C++ counterpart of .NET SortedList<TKey,TValue>.ContainsKey(TKey).
     * @param key The key to locate.
     * @return true if found; otherwise false.
     */
    [[nodiscard]] bool ContainsKey(const TKey& key) const { return map_.count(key) > 0; }

    /**
     * @brief Determines whether the list contains the specified value.
     *
     * C++ counterpart of .NET SortedList<TKey,TValue>.ContainsValue(TValue).
     * @param value The value to locate.
     * @return true if at least one entry has the value; otherwise false.
     */
    [[nodiscard]] bool ContainsValue(const TValue& value) const {
        for (const auto& p : map_) if (p.second == value) return true;
        return false;
    }

    /**
     * @brief Gets the value associated with the specified key.
     *
     * C++ counterpart of .NET SortedList<TKey,TValue>.TryGetValue(TKey, out TValue).
     * @param key   The key to locate.
     * @param value Receives the value if the key is found.
     * @return true if the key was found; otherwise false.
     */
    bool TryGetValue(const TKey& key, TValue& value) const {
        auto it = map_.find(key);
        if (it == map_.end()) return false;
        value = it->second;
        return true;
    }

    /**
     * @brief Returns the value for the key, or @p defaultValue if not found.
     *
     * @param key          The key to locate.
     * @param defaultValue The value to return if the key is absent.
     * @return The associated value, or @p defaultValue.
     */
    [[nodiscard]] TValue GetValueOrDefault(const TKey& key, const TValue& defaultValue = TValue{}) const {
        auto it = map_.find(key);
        return it != map_.end() ? it->second : defaultValue;
    }

    /**
     * @brief Returns the zero-based index of the specified key, or -1 if not found.
     *
     * C++ counterpart of .NET SortedList<TKey,TValue>.IndexOfKey(TKey).
     * @param key The key to locate.
     * @return The index of the key, or -1.
     */
    [[nodiscard]] intcs IndexOfKey(const TKey& key) const {
        intcs i = 0;
        for (const auto& p : map_) {
            if (p.first == key) return i;
            ++i;
        }
        return -1;
    }

    /**
     * @brief Returns the zero-based index of the first entry with the specified value, or -1 if not found.
     *
     * C++ counterpart of .NET SortedList<TKey,TValue>.IndexOfValue(TValue).
     * @param value The value to locate.
     * @return The index of the first entry with the value, or -1.
     */
    [[nodiscard]] intcs IndexOfValue(const TValue& value) const {
        intcs i = 0;
        for (const auto& p : map_) {
            if (p.second == value) return i;
            ++i;
        }
        return -1;
    }

    /**
     * @brief Gets all keys in sorted order.
     *
     * C++ counterpart of .NET SortedList<TKey,TValue>.Keys.
     * @return A std::vector<TKey> of all keys in ascending order.
     */
    [[nodiscard]] std::vector<TKey> getKeysProperty() const {
        std::vector<TKey> keys;
        keys.reserve(map_.size());
        for (const auto& kv : map_) keys.push_back(kv.first);
        return keys;
    }

    /**
     * @brief Gets all values in key-sorted order.
     *
     * C++ counterpart of .NET SortedList<TKey,TValue>.Values.
     * @return A std::vector<TValue> of all values in key-ascending order.
     */
    [[nodiscard]] std::vector<TValue> getValuesProperty() const {
        std::vector<TValue> vals;
        vals.reserve(map_.size());
        for (const auto& kv : map_) vals.push_back(kv.second);
        return vals;
    }

    /** @brief Returns all keys in sorted order (alias for getKeysProperty()). */
    [[nodiscard]] std::vector<TKey> Keys() const { return getKeysProperty(); }
    /** @brief Returns all values in key-sorted order (alias for getValuesProperty()). */
    [[nodiscard]] std::vector<TValue> Values() const { return getValuesProperty(); }

    /**
     * @brief Removes all key/value pairs from the list.
     *
     * C++ counterpart of .NET SortedList<TKey,TValue>.Clear().
     */
    void Clear() { map_.clear(); }

    /** @brief Returns an iterator to the beginning of the list (STL interop). */
    auto begin()       { return map_.begin(); }
    /** @brief Returns an iterator past the end of the list (STL interop). */
    auto end()         { return map_.end(); }
    /** @brief Returns a const iterator to the beginning of the list (STL interop). */
    auto begin() const { return map_.cbegin(); }
    /** @brief Returns a const iterator past the end of the list (STL interop). */
    auto end()   const { return map_.cend(); }
};

} // namespace System::Collections::Generic
