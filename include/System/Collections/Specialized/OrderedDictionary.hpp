// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::Collections::Specialized {

    using SharpRuntime::intcs;

    /**
     * @brief A dictionary that preserves insertion order and allows access by key or index.
     *
     * Partial C++ counterpart of .NET System.Collections.Specialized.OrderedDictionary.
     * Keys and values are std::string (non-generic, like the .NET version).
     *
     * @note Status: Partial
     */
    class OrderedDictionary {
        std::vector<std::string>                  keys_;
        std::vector<std::string>                  values_;
        std::unordered_map<std::string, intcs>    index_;
    public:
        OrderedDictionary() = default;

        [[nodiscard]] intcs getCountProperty() const { return static_cast<intcs>(keys_.size()); }

        void Add(const std::string& key, const std::string& value) {
            if (index_.count(key)) throw std::invalid_argument("Key already exists: " + key);
            index_[key] = static_cast<intcs>(keys_.size());
            keys_.push_back(key);
            values_.push_back(value);
        }

        void Insert(intcs index, const std::string& key, const std::string& value) {
            if (index_.count(key)) throw std::invalid_argument("Key already exists: " + key);
            keys_.insert(keys_.begin() + index, key);
            values_.insert(values_.begin() + index, value);
            // rebuild index
            index_.clear();
            for (intcs i = 0; i < static_cast<intcs>(keys_.size()); ++i) index_[keys_[i]] = i;
        }

        void Remove(const std::string& key) {
            auto it = index_.find(key);
            if (it == index_.end()) return;
            intcs i = it->second;
            keys_.erase(keys_.begin() + i);
            values_.erase(values_.begin() + i);
            index_.clear();
            for (intcs j = 0; j < static_cast<intcs>(keys_.size()); ++j) index_[keys_[j]] = j;
        }

        void RemoveAt(intcs index) {
            if (index < 0 || index >= getCountProperty()) throw std::out_of_range("index");
            Remove(keys_[index]);
        }

        [[nodiscard]] bool Contains(const std::string& key) const { return index_.count(key) > 0; }

        [[nodiscard]] const std::string& operator[](const std::string& key) const {
            auto it = index_.find(key);
            if (it == index_.end()) throw std::out_of_range("Key not found: " + key);
            return values_[it->second];
        }

        std::string& operator[](const std::string& key) {
            auto it = index_.find(key);
            if (it == index_.end()) throw std::out_of_range("Key not found: " + key);
            return values_[it->second];
        }

        [[nodiscard]] const std::string& GetByIndex(intcs i) const { return values_.at(static_cast<size_t>(i)); }
        [[nodiscard]] const std::string& GetKey(intcs i)     const { return keys_.at(static_cast<size_t>(i)); }

        void Clear() { keys_.clear(); values_.clear(); index_.clear(); }

        [[nodiscard]] const std::vector<std::string>& Keys()   const { return keys_; }
        [[nodiscard]] const std::vector<std::string>& Values() const { return values_; }
    };

} // namespace System::Collections::Specialized
