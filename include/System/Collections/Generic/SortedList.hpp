// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <map>
#include <vector>
#include <stdexcept>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::Collections::Generic {

    using SharpRuntime::intcs;

    /**
     * @brief A collection of key/value pairs sorted by key, accessible by key and index.
     *
     * Wraps std::map. Partial C++ counterpart of .NET System.Collections.Generic.SortedList<TKey,TValue>.
     *
     * @note Status: Partial
     */
    template<typename TKey, typename TValue>
    class SortedList {
        std::map<TKey, TValue> map_;
    public:
        /** Default-constructs an empty SortedList. */
        SortedList() = default;

        /** Gets the number of key/value pairs in the SortedList. */
        [[nodiscard]] intcs getCountProperty() const { return static_cast<intcs>(map_.size()); }

        /** Returns a reference to the value associated with the given key, inserting a default if absent. */
        TValue& operator[](const TKey& key) { return map_[key]; }

        /** Adds the specified key and value; throws if the key already exists. */
        void Add(const TKey& key, const TValue& value) {
            if (map_.count(key)) throw std::invalid_argument("Key already exists");
            map_[key] = value;
        }

        /** Removes the entry with the specified key; returns true if removed. */
        bool Remove(const TKey& key) { return map_.erase(key) > 0; }
        /** Removes the entry at the specified zero-based index. */
        void RemoveAt(intcs index) {
            if (index < 0 || index >= getCountProperty()) throw std::out_of_range("index");
            auto it = map_.begin();
            std::advance(it, index);
            map_.erase(it);
        }

        /** Returns true if the SortedList contains the specified key. */
        [[nodiscard]] bool ContainsKey(const TKey& key)   const { return map_.count(key) > 0; }
        /** Returns true if the SortedList contains the specified value. */
        [[nodiscard]] bool ContainsValue(const TValue& v) const {
            for (auto& p : map_) if (p.second == v) return true;
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

        /** Returns the zero-based index of the specified key, or -1 if not found. */
        [[nodiscard]] intcs IndexOfKey(const TKey& key) const {
            intcs i = 0;
            for (auto& p : map_) { if (p.first == key) return i; ++i; }
            return -1;
        }

        /** Removes all key/value pairs from the SortedList. */
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

        /** Returns an iterator to the beginning of the SortedList. */
        auto begin()       { return map_.begin(); }
        /** Returns an iterator past the end of the SortedList. */
        auto end()         { return map_.end(); }
        /** Returns a const iterator to the beginning of the SortedList. */
        auto begin() const { return map_.cbegin(); }
        /** Returns a const iterator past the end of the SortedList. */
        auto end()   const { return map_.cend(); }
    };

} // namespace System::Collections::Generic
