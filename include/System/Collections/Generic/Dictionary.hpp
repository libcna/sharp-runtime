// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cmath>
#include <unordered_map>
#include <stdexcept>
#include <string>
#include <vector>

namespace System::Collections::Generic {

/**
 * @brief Represents a generic collection of key/value pairs.
 *
 * C++ counterpart of .NET System.Collections.Generic.Dictionary<TKey,TValue>.
 * Backed by std::unordered_map; provides O(1) average-case lookup, insertion, and removal.
 *
 * @tparam TKey   The type of the keys.
 * @tparam TValue The type of the values.
 */
template<typename TKey, typename TValue>
class Dictionary {
    std::unordered_map<TKey, TValue> map_;

public:
    /**
     * @brief Initializes a new empty Dictionary.
     *
     * C++ counterpart of .NET Dictionary<TKey,TValue>().
     */
    Dictionary() = default;

    /**
     * @brief Gets the number of key/value pairs contained in the dictionary.
     *
     * C++ counterpart of .NET Dictionary<TKey,TValue>.Count.
     */
    [[nodiscard]] int getCountProperty() const {
        return static_cast<int>(map_.size());
    }

    /**
     * @brief Adds the specified key and value to the dictionary.
     *
     * C++ counterpart of .NET Dictionary<TKey,TValue>.Add(TKey, TValue).
     * @param key   The key of the element to add.
     * @param value The value of the element to add.
     * @throws std::invalid_argument if a key with the same value already exists.
     */
    void Add(const TKey& key, const TValue& value) {
        if (map_.count(key))
            throw std::invalid_argument("An element with the same key already exists.");
        map_[key] = value;
    }

    /**
     * @brief Removes the element with the specified key from the dictionary.
     *
     * C++ counterpart of .NET Dictionary<TKey,TValue>.Remove(TKey).
     * @param key The key of the element to remove.
     * @return true if the element was found and removed; otherwise false.
     */
    bool Remove(const TKey& key) {
        return map_.erase(key) > 0;
    }

    /**
     * @brief Removes the element with the specified key and copies its value to the output parameter.
     *
     * C++ counterpart of .NET Dictionary<TKey,TValue>.Remove(TKey, out TValue).
     * @param key   The key of the element to remove.
     * @param value Receives the removed value if found.
     * @return true if the element was found and removed; otherwise false.
     */
    bool Remove(const TKey& key, TValue& value) {
        auto it = map_.find(key);
        if (it == map_.end()) return false;
        value = std::move(it->second);
        map_.erase(it);
        return true;
    }

    /**
     * @brief Determines whether the dictionary contains the specified key.
     *
     * C++ counterpart of .NET Dictionary<TKey,TValue>.ContainsKey(TKey).
     * @param key The key to locate.
     * @return true if the key is found; otherwise false.
     */
    [[nodiscard]] bool ContainsKey(const TKey& key) const {
        return map_.count(key) > 0;
    }

    /**
     * @brief Determines whether the dictionary contains a specific value.
     *
     * C++ counterpart of .NET Dictionary<TKey,TValue>.ContainsValue(TValue).
     * This is an O(n) operation.
     * @param value The value to locate.
     * @return true if an entry with the value is found; otherwise false.
     */
    [[nodiscard]] bool ContainsValue(const TValue& value) const {
        for (const auto& kv : map_)
            if (kv.second == value) return true;
        return false;
    }

    /**
     * @brief Gets the value associated with the specified key.
     *
     * C++ counterpart of .NET Dictionary<TKey,TValue>.TryGetValue(TKey, out TValue).
     * @param key      The key whose value to get.
     * @param outValue Receives the value if the key is found.
     * @return true if the key was found; otherwise false.
     */
    bool TryGetValue(const TKey& key, TValue& outValue) const {
        auto it = map_.find(key);
        if (it == map_.end()) return false;
        outValue = it->second;
        return true;
    }

    /**
     * @brief Adds a key/value pair only if the key does not already exist.
     *
     * C++ counterpart of .NET Dictionary<TKey,TValue>.TryAdd(TKey, TValue).
     * @param key   The key to add.
     * @param value The value to associate with the key.
     * @return true if the pair was added; false if the key already existed.
     */
    bool TryAdd(const TKey& key, const TValue& value) {
        if (map_.count(key)) return false;
        map_[key] = value;
        return true;
    }

    /**
     * @brief Gets the value for a key, or a default if the key is absent.
     *
     * C++ counterpart of .NET CollectionExtensions.GetValueOrDefault extension method.
     * @param key          The key to look up.
     * @param defaultValue Value to return if the key is absent.
     * @return The associated value, or defaultValue.
     */
    [[nodiscard]] TValue GetValueOrDefault(const TKey& key, const TValue& defaultValue = TValue{}) const {
        auto it = map_.find(key);
        return it != map_.end() ? it->second : defaultValue;
    }

    /**
     * @brief Removes all key/value pairs from the dictionary.
     *
     * C++ counterpart of .NET Dictionary<TKey,TValue>.Clear().
     */
    void Clear() { map_.clear(); }

    /**
     * @brief Gets a vector containing the keys of the dictionary.
     *
     * C++ counterpart of .NET Dictionary<TKey,TValue>.Keys.
     */
    [[nodiscard]] std::vector<TKey> getKeysProperty() const {
        std::vector<TKey> keys;
        keys.reserve(map_.size());
        for (const auto& kv : map_) keys.push_back(kv.first);
        return keys;
    }

    /**
     * @brief Gets a vector containing the values of the dictionary.
     *
     * C++ counterpart of .NET Dictionary<TKey,TValue>.Values.
     */
    [[nodiscard]] std::vector<TValue> getValuesProperty() const {
        std::vector<TValue> vals;
        vals.reserve(map_.size());
        for (const auto& kv : map_) vals.push_back(kv.second);
        return vals;
    }

    /**
     * @brief Ensures the internal bucket count can hold at least capacity entries without rehashing.
     *
     * C++ counterpart of .NET Dictionary<TKey,TValue>.EnsureCapacity(int).
     * @param capacity The minimum number of entries the dictionary should be able to hold.
     */
    void EnsureCapacity(int capacity) {
        map_.reserve(static_cast<std::size_t>(capacity));
    }

    /**
     * @brief Reduces internal memory by resizing the bucket array to fit the current entry count.
     *
     * C++ counterpart of .NET Dictionary<TKey,TValue>.TrimExcess().
     */
    void TrimExcess() {
        map_.rehash(static_cast<std::size_t>(
            std::ceil(static_cast<double>(map_.size()) / map_.max_load_factor())));
    }

    /**
     * @brief Gets or sets the value associated with the specified key (mutable).
     *
     * C++ counterpart of .NET Dictionary<TKey,TValue> indexer setter.
     * Inserts a default value if the key is absent.
     */
    TValue& operator[](const TKey& key) { return map_[key]; }

    /**
     * @brief Gets the value associated with the specified key (const).
     *
     * C++ counterpart of .NET Dictionary<TKey,TValue> indexer getter.
     * @throws std::out_of_range if the key is not found.
     */
    [[nodiscard]] const TValue& operator[](const TKey& key) const {
        auto it = map_.find(key);
        if (it == map_.end())
            throw std::out_of_range("Key not found in Dictionary.");
        return it->second;
    }

    /** @brief Returns an iterator to the first element (for range-based for). */
    auto begin()        { return map_.begin(); }
    /** @brief Returns an iterator past the last element (for range-based for). */
    auto end()          { return map_.end(); }
    /** @brief Returns a const iterator to the first element (for range-based for). */
    [[nodiscard]] auto begin() const { return map_.cbegin(); }
    /** @brief Returns a const iterator past the last element (for range-based for). */
    [[nodiscard]] auto end()   const { return map_.cend(); }

    /**
     * @brief Returns a const reference to the underlying std::unordered_map.
     *
     * Provides direct STL interoperability when needed.
     */
    [[nodiscard]] const std::unordered_map<TKey, TValue>& ToMap() const { return map_; }

    /**
     * @brief Returns a reference to the underlying std::unordered_map.
     *
     * Provides direct STL interoperability when needed.
     */
    std::unordered_map<TKey, TValue>& ToMap() { return map_; }
};

} // namespace System::Collections::Generic
