// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <any>
#include <stdexcept>
#include <string>
#include <vector>

namespace System::Collections::Specialized {

    // Simple ordered key-value list (backed by vector of pairs for small collections)
    class ListDictionary {
        std::vector<std::pair<std::string, std::any>> data_;

        int findIndex(const std::string& key) const {
            for (int i = 0; i < static_cast<int>(data_.size()); ++i)
                if (data_[i].first == key) return i;
            return -1;
        }

    public:
        [[nodiscard]] int  getCountProperty()    const { return static_cast<int>(data_.size()); }
        [[nodiscard]] bool getIsEmptyProperty()  const { return data_.empty(); }

        void Add(const std::string& key, const std::any& value) {
            if (findIndex(key) >= 0) throw std::invalid_argument("Duplicate key: " + key);
            data_.emplace_back(key, value);
        }

        void Remove(const std::string& key) {
            int idx = findIndex(key);
            if (idx >= 0) data_.erase(data_.begin() + idx);
        }

        void Clear() { data_.clear(); }

        [[nodiscard]] bool Contains(const std::string& key) const { return findIndex(key) >= 0; }

        const std::any& operator[](const std::string& key) const {
            int idx = findIndex(key);
            if (idx < 0) throw std::out_of_range("Key not found: " + key);
            return data_[idx].second;
        }

        std::any& operator[](const std::string& key) {
            int idx = findIndex(key);
            if (idx >= 0) return data_[idx].second;
            data_.emplace_back(key, std::any{});
            return data_.back().second;
        }

        auto begin() const { return data_.begin(); }
        auto end()   const { return data_.end(); }
    };

} // namespace System::Collections::Specialized
