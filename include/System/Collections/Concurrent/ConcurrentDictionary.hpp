// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <mutex>
#include <unordered_map>
#include <vector>
#include <functional>
#include <stdexcept>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::Collections::Concurrent {

    using SharpRuntime::intcs;

    /**
     * @brief A thread-safe collection of key/value pairs.
     *
     * Wraps std::unordered_map with std::mutex.
     * Partial C++ counterpart of .NET System.Collections.Concurrent.ConcurrentDictionary<TKey,TValue>.
     *
     */
    template<typename TKey, typename TValue>
    class ConcurrentDictionary {
        mutable std::mutex             mutex_;
        std::unordered_map<TKey,TValue> map_;
    public:
        /** Default-constructs an empty ConcurrentDictionary. */
        ConcurrentDictionary() = default;

        /** Gets the number of key/value pairs in the dictionary (thread-safe). */
        [[nodiscard]] intcs getCountProperty() const {
            std::lock_guard<std::mutex> lk(mutex_);
            return static_cast<intcs>(map_.size());
        }

        /** Returns true if the dictionary contains no elements (thread-safe). */
        [[nodiscard]] bool getIsEmptyProperty() const {
            std::lock_guard<std::mutex> lk(mutex_);
            return map_.empty();
        }

        /** Thread-safely adds key/value only if key is not already present; returns true if added. */
        bool TryAdd(const TKey& key, const TValue& value) {
            std::lock_guard<std::mutex> lk(mutex_);
            return map_.emplace(key, value).second;
        }

        /** Thread-safely retrieves the value for key; returns true if found. */
        bool TryGetValue(const TKey& key, TValue& value) const {
            std::lock_guard<std::mutex> lk(mutex_);
            auto it = map_.find(key);
            if (it == map_.end()) return false;
            value = it->second;
            return true;
        }

        /** Thread-safely removes the entry for key and outputs its value; returns true if removed. */
        bool TryRemove(const TKey& key, TValue& value) {
            std::lock_guard<std::mutex> lk(mutex_);
            auto it = map_.find(key);
            if (it == map_.end()) return false;
            value = it->second;
            map_.erase(it);
            return true;
        }

        /** Thread-safely updates the value for key if the current value equals comparisonValue; returns true if updated. */
        bool TryUpdate(const TKey& key, const TValue& newValue, const TValue& comparisonValue) {
            std::lock_guard<std::mutex> lk(mutex_);
            auto it = map_.find(key);
            if (it == map_.end() || !(it->second == comparisonValue)) return false;
            it->second = newValue;
            return true;
        }

        /** Returns a reference to the value for key, inserting a default if not present (thread-safe). */
        TValue& operator[](const TKey& key) {
            std::lock_guard<std::mutex> lk(mutex_);
            return map_[key];
        }

        /** Thread-safely returns the value for key if present, otherwise inserts and returns defaultValue. */
        TValue GetOrAdd(const TKey& key, const TValue& defaultValue) {
            std::lock_guard<std::mutex> lk(mutex_);
            auto [it, inserted] = map_.emplace(key, defaultValue);
            return it->second;
        }

        /** Thread-safely returns the value for key if present, otherwise inserts the value produced by factory. */
        TValue GetOrAdd(const TKey& key, std::function<TValue(const TKey&)> factory) {
            std::lock_guard<std::mutex> lk(mutex_);
            auto it = map_.find(key);
            if (it != map_.end()) return it->second;
            TValue v = factory(key);
            map_[key] = v;
            return v;
        }

        /** Thread-safely adds addValue if key is absent, or replaces the existing value using updateFactory. */
        TValue AddOrUpdate(const TKey& key, const TValue& addValue,
                           std::function<TValue(const TKey&, const TValue&)> updateFactory) {
            std::lock_guard<std::mutex> lk(mutex_);
            auto it = map_.find(key);
            if (it == map_.end()) { map_[key] = addValue; return addValue; }
            it->second = updateFactory(key, it->second);
            return it->second;
        }

        /** Returns true if the dictionary contains the specified key (thread-safe). */
        [[nodiscard]] bool ContainsKey(const TKey& key) const {
            std::lock_guard<std::mutex> lk(mutex_);
            return map_.count(key) > 0;
        }

        /** Thread-safely removes all elements from the dictionary. */
        void Clear() {
            std::lock_guard<std::mutex> lk(mutex_);
            map_.clear();
        }

        /** Returns a snapshot vector of all keys (thread-safe). */
        [[nodiscard]] std::vector<TKey> Keys() const {
            std::lock_guard<std::mutex> lk(mutex_);
            std::vector<TKey> k;
            for (auto& p : map_) k.push_back(p.first);
            return k;
        }

        /** Returns a snapshot vector of all values (thread-safe). */
        [[nodiscard]] std::vector<TValue> Values() const {
            std::lock_guard<std::mutex> lk(mutex_);
            std::vector<TValue> v;
            for (auto& p : map_) v.push_back(p.second);
            return v;
        }
    };

} // namespace System::Collections::Concurrent
