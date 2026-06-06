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
        SortedList() = default;

        [[nodiscard]] intcs getCountProperty() const { return static_cast<intcs>(map_.size()); }

        TValue& operator[](const TKey& key) { return map_[key]; }

        void Add(const TKey& key, const TValue& value) {
            if (map_.count(key)) throw std::invalid_argument("Key already exists");
            map_[key] = value;
        }

        bool Remove(const TKey& key) { return map_.erase(key) > 0; }
        void RemoveAt(intcs index) {
            if (index < 0 || index >= getCountProperty()) throw std::out_of_range("index");
            auto it = map_.begin();
            std::advance(it, index);
            map_.erase(it);
        }

        [[nodiscard]] bool ContainsKey(const TKey& key)   const { return map_.count(key) > 0; }
        [[nodiscard]] bool ContainsValue(const TValue& v) const {
            for (auto& p : map_) if (p.second == v) return true;
            return false;
        }

        bool TryGetValue(const TKey& key, TValue& value) const {
            auto it = map_.find(key);
            if (it == map_.end()) return false;
            value = it->second;
            return true;
        }

        [[nodiscard]] intcs IndexOfKey(const TKey& key) const {
            intcs i = 0;
            for (auto& p : map_) { if (p.first == key) return i; ++i; }
            return -1;
        }

        void Clear() { map_.clear(); }

        [[nodiscard]] std::vector<TKey> Keys() const {
            std::vector<TKey> k;
            for (auto& p : map_) k.push_back(p.first);
            return k;
        }

        [[nodiscard]] std::vector<TValue> Values() const {
            std::vector<TValue> v;
            for (auto& p : map_) v.push_back(p.second);
            return v;
        }

        auto begin()       { return map_.begin(); }
        auto end()         { return map_.end(); }
        auto begin() const { return map_.cbegin(); }
        auto end()   const { return map_.cend(); }
    };

} // namespace System::Collections::Generic
