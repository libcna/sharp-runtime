// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <cmath>
#include <unordered_map>
#include <stdexcept>
#include <string>
#include <vector>

namespace System::Collections::Generic
{
    /**
     * @brief A generic collection of key-value pairs.
     *
     * C++ counterpart of the .NET System.Collections.Generic.Dictionary<TKey,TValue> class.
     * Backed by std::unordered_map<TKey, TValue>.
     *
     * @tparam TKey   The type of keys in the dictionary.
     * @tparam TValue The type of values in the dictionary.
     */
    template<typename TKey, typename TValue>
    class Dictionary
    {
    private:
        std::unordered_map<TKey, TValue> map_;

    public:
        Dictionary() = default;

        /** Gets the number of key-value pairs. */
        [[nodiscard]] int getCountProperty() const
        {
            return static_cast<int>(map_.size());
        }

        /** Adds a key-value pair. Throws if the key already exists. */
        void Add(const TKey& key, const TValue& value)
        {
            if (map_.count(key))
                throw std::invalid_argument("An element with the same key already exists.");
            map_[key] = value;
        }

        /** Removes the entry for the given key. Returns true if found. */
        bool Remove(const TKey& key)
        {
            return map_.erase(key) > 0;
        }

        /** Returns true if the dictionary contains the given key. */
        [[nodiscard]] bool ContainsKey(const TKey& key) const
        {
            return map_.count(key) > 0;
        }

        /** Tries to get a value; returns false if not found. */
        bool TryGetValue(const TKey& key, TValue& outValue) const
        {
            auto it = map_.find(key);
            if (it == map_.end()) return false;
            outValue = it->second;
            return true;
        }

        /** Removes all entries. */
        void Clear() { map_.clear(); }

        TValue& operator[](const TKey& key) { return map_[key]; }

        [[nodiscard]] const TValue& operator[](const TKey& key) const
        {
            auto it = map_.find(key);
            if (it == map_.end())
                throw std::out_of_range("Key not found in Dictionary.");
            return it->second;
        }

        auto begin()        { return map_.begin(); }
        auto end()          { return map_.end(); }
        [[nodiscard]] auto begin() const { return map_.cbegin(); }
        [[nodiscard]] auto end()   const { return map_.cend(); }

        /** Adds the key/value pair if the key does not already exist. Returns true if added. */
        bool TryAdd(const TKey& key, const TValue& value)
        {
            if (map_.count(key)) return false;
            map_[key] = value;
            return true;
        }

        /** Returns true if any entry has a value equal to @p value (uses operator==). */
        [[nodiscard]] bool ContainsValue(const TValue& value) const
        {
            for (const auto& kv : map_)
                if (kv.second == value) return true;
            return false;
        }

        /** Returns the value for @p key, or @p defaultValue if the key is absent. */
        [[nodiscard]] TValue GetValueOrDefault(const TKey& key, const TValue& defaultValue = TValue{}) const
        {
            auto it = map_.find(key);
            return it != map_.end() ? it->second : defaultValue;
        }

        /** Returns a vector of all keys. */
        [[nodiscard]] std::vector<TKey> getKeysProperty() const
        {
            std::vector<TKey> keys;
            keys.reserve(map_.size());
            for (const auto& kv : map_) keys.push_back(kv.first);
            return keys;
        }

        /** Returns a vector of all values. */
        [[nodiscard]] std::vector<TValue> getValuesProperty() const
        {
            std::vector<TValue> vals;
            vals.reserve(map_.size());
            for (const auto& kv : map_) vals.push_back(kv.second);
            return vals;
        }

        /** @brief Removes the entry for @p key and copies the associated value to @p value. Returns true if found. */
        bool Remove(const TKey& key, TValue& value)
        {
            auto it = map_.find(key);
            if (it == map_.end()) return false;
            value = std::move(it->second);
            map_.erase(it);
            return true;
        }

        /** @brief Ensures the bucket count is sufficient for at least @p capacity entries without rehashing. */
        void EnsureCapacity(int capacity)
        {
            map_.reserve(static_cast<std::size_t>(capacity));
        }

        /** @brief Reduces memory usage by setting the load factor to current size. */
        void TrimExcess()
        {
            map_.rehash(static_cast<std::size_t>(std::ceil(static_cast<double>(map_.size()) / map_.max_load_factor())));
        }

        /** Returns the underlying std::unordered_map for STL interop. */
        [[nodiscard]] const std::unordered_map<TKey, TValue>& ToMap() const { return map_; }
        [[nodiscard]] std::unordered_map<TKey, TValue>& ToMap() { return map_; }
    };
}
