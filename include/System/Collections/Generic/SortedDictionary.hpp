// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <map>
#include <vector>
#include <stdexcept>
#include "System/Collections/Generic/KeyValuePair.hpp"
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::Collections::Generic {

    using SharpRuntime::intcs;

    /**
     * @brief A collection of key/value pairs sorted on the key.
     *
     * Wraps std::map. Partial C++ counterpart of .NET System.Collections.Generic.SortedDictionary<TKey,TValue>.
     *
     * @note Status: Partial
     */
    template<typename TKey, typename TValue>
    class SortedDictionary {
        std::map<TKey, TValue> map_;
    public:
        /** Default-constructs an empty SortedDictionary. */
        SortedDictionary() = default;

        /** Gets the number of key/value pairs in the SortedDictionary. */
        [[nodiscard]] intcs getCountProperty() const { return static_cast<intcs>(map_.size()); }

        /** Returns a reference to the value associated with the given key, inserting a default if absent. */
        TValue& operator[](const TKey& key) { return map_[key]; }
        /** Returns a const reference to the value associated with the given key. */
        [[nodiscard]] const TValue& operator[](const TKey& key) const { return map_.at(key); }

        /** Adds the specified key and value to the dictionary; throws if the key already exists. */
        void Add(const TKey& key, const TValue& value) {
            if (map_.count(key)) throw std::invalid_argument("Key already exists");
            map_[key] = value;
        }

        /** Removes the entry with the specified key; returns true if removed. */
        bool Remove(const TKey& key) { return map_.erase(key) > 0; }

        /** Returns true if the dictionary contains an entry with the specified key. */
        [[nodiscard]] bool ContainsKey(const TKey& key) const { return map_.count(key) > 0; }

        /** Returns true if any entry has a value equal to @p value (uses operator==). */
        [[nodiscard]] bool ContainsValue(const TValue& value) const {
            for (const auto& kv : map_)
                if (kv.second == value) return true;
            return false;
        }

        /** Adds the key/value pair if the key does not already exist. Returns true if added. */
        bool TryAdd(const TKey& key, const TValue& value) {
            if (map_.count(key)) return false;
            map_[key] = value;
            return true;
        }

        /** Returns the value for @p key, or @p defaultValue if the key is absent. */
        [[nodiscard]] TValue GetValueOrDefault(const TKey& key, const TValue& defaultValue = TValue{}) const {
            auto it = map_.find(key);
            return it != map_.end() ? it->second : defaultValue;
        }

        /** Returns a vector of all keys in sorted order. */
        [[nodiscard]] std::vector<TKey> getKeysProperty() const {
            std::vector<TKey> keys;
            keys.reserve(map_.size());
            for (const auto& kv : map_) keys.push_back(kv.first);
            return keys;
        }

        /** Returns a vector of all values in key-sorted order. */
        [[nodiscard]] std::vector<TValue> getValuesProperty() const {
            std::vector<TValue> vals;
            vals.reserve(map_.size());
            for (const auto& kv : map_) vals.push_back(kv.second);
            return vals;
        }

        /** Gets the value associated with the specified key; returns true if found. */
        bool TryGetValue(const TKey& key, TValue& value) const {
            auto it = map_.find(key);
            if (it == map_.end()) return false;
            value = it->second;
            return true;
        }

        /** Removes all key/value pairs from the dictionary. */
        void Clear() { map_.clear(); }

        /** Returns a vector of all keys in sorted order. */
        [[nodiscard]] std::vector<TKey> Keys() const {
            std::vector<TKey> k;
            for (auto& p : map_) k.push_back(p.first);
            return k;
        }

        /** Returns a vector of all values in key-sorted order. */
        [[nodiscard]] std::vector<TValue> Values() const {
            std::vector<TValue> v;
            for (auto& p : map_) v.push_back(p.second);
            return v;
        }

        /** Returns an iterator to the beginning of the dictionary. */
        auto begin()        { return map_.begin(); }
        /** Returns an iterator past the end of the dictionary. */
        auto end()          { return map_.end(); }
        /** Returns a const iterator to the beginning of the dictionary. */
        auto begin()  const { return map_.cbegin(); }
        /** Returns a const iterator past the end of the dictionary. */
        auto end()    const { return map_.cend(); }
    };

} // namespace System::Collections::Generic
