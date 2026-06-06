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
     * @note Status: Partial
     */
    template<typename TKey, typename TValue>
    class ConcurrentDictionary {
        mutable std::mutex             mutex_;
        std::unordered_map<TKey,TValue> map_;
    public:
        ConcurrentDictionary() = default;

        [[nodiscard]] intcs getCountProperty() const {
            std::lock_guard<std::mutex> lk(mutex_);
            return static_cast<intcs>(map_.size());
        }

        [[nodiscard]] bool getIsEmptyProperty() const {
            std::lock_guard<std::mutex> lk(mutex_);
            return map_.empty();
        }

        bool TryAdd(const TKey& key, const TValue& value) {
            std::lock_guard<std::mutex> lk(mutex_);
            return map_.emplace(key, value).second;
        }

        bool TryGetValue(const TKey& key, TValue& value) const {
            std::lock_guard<std::mutex> lk(mutex_);
            auto it = map_.find(key);
            if (it == map_.end()) return false;
            value = it->second;
            return true;
        }

        bool TryRemove(const TKey& key, TValue& value) {
            std::lock_guard<std::mutex> lk(mutex_);
            auto it = map_.find(key);
            if (it == map_.end()) return false;
            value = it->second;
            map_.erase(it);
            return true;
        }

        bool TryUpdate(const TKey& key, const TValue& newValue, const TValue& comparisonValue) {
            std::lock_guard<std::mutex> lk(mutex_);
            auto it = map_.find(key);
            if (it == map_.end() || !(it->second == comparisonValue)) return false;
            it->second = newValue;
            return true;
        }

        TValue& operator[](const TKey& key) {
            std::lock_guard<std::mutex> lk(mutex_);
            return map_[key];
        }

        TValue GetOrAdd(const TKey& key, const TValue& defaultValue) {
            std::lock_guard<std::mutex> lk(mutex_);
            auto [it, inserted] = map_.emplace(key, defaultValue);
            return it->second;
        }

        TValue GetOrAdd(const TKey& key, std::function<TValue(const TKey&)> factory) {
            std::lock_guard<std::mutex> lk(mutex_);
            auto it = map_.find(key);
            if (it != map_.end()) return it->second;
            TValue v = factory(key);
            map_[key] = v;
            return v;
        }

        TValue AddOrUpdate(const TKey& key, const TValue& addValue,
                           std::function<TValue(const TKey&, const TValue&)> updateFactory) {
            std::lock_guard<std::mutex> lk(mutex_);
            auto it = map_.find(key);
            if (it == map_.end()) { map_[key] = addValue; return addValue; }
            it->second = updateFactory(key, it->second);
            return it->second;
        }

        [[nodiscard]] bool ContainsKey(const TKey& key) const {
            std::lock_guard<std::mutex> lk(mutex_);
            return map_.count(key) > 0;
        }

        void Clear() {
            std::lock_guard<std::mutex> lk(mutex_);
            map_.clear();
        }

        [[nodiscard]] std::vector<TKey> Keys() const {
            std::lock_guard<std::mutex> lk(mutex_);
            std::vector<TKey> k;
            for (auto& p : map_) k.push_back(p.first);
            return k;
        }

        [[nodiscard]] std::vector<TValue> Values() const {
            std::lock_guard<std::mutex> lk(mutex_);
            std::vector<TValue> v;
            for (auto& p : map_) v.push_back(p.second);
            return v;
        }
    };

} // namespace System::Collections::Concurrent
