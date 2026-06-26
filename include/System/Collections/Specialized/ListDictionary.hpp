// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <any>
#include <stdexcept>
#include <string>
#include <vector>

namespace System::Collections::Specialized {

    /** Simple ordered key-value list backed by a vector of pairs, suitable for small collections. */
    class ListDictionary {
        std::vector<std::pair<std::string, std::any>> data_;

        int findIndex(const std::string& key) const {
            for (int i = 0; i < static_cast<int>(data_.size()); ++i)
                if (data_[i].first == key) return i;
            return -1;
        }

    public:
        /** Gets the number of key/value pairs in the dictionary. */
        [[nodiscard]] int  getCountProperty()    const { return static_cast<int>(data_.size()); }
        /** Returns true if the dictionary contains no elements. */
        [[nodiscard]] bool getIsEmptyProperty()  const { return data_.empty(); }

        /** Adds a key/value pair; throws if the key already exists. */
        void Add(const std::string& key, const std::any& value) {
            if (findIndex(key) >= 0) throw std::invalid_argument("Duplicate key: " + key);
            data_.emplace_back(key, value);
        }

        /** Removes the entry with the given key; does nothing if not found. */
        void Remove(const std::string& key) {
            int idx = findIndex(key);
            if (idx >= 0) data_.erase(data_.begin() + idx);
        }

        /** Removes all entries from the dictionary. */
        void Clear() { data_.clear(); }

        /** Returns true if the dictionary contains the given key. */
        [[nodiscard]] bool Contains(const std::string& key) const { return findIndex(key) >= 0; }

        /** Returns a const reference to the value for the given key; throws if not found. */
        const std::any& operator[](const std::string& key) const {
            int idx = findIndex(key);
            if (idx < 0) throw std::out_of_range("Key not found: " + key);
            return data_[idx].second;
        }

        /** Returns a reference to the value for the given key, inserting an empty entry if absent. */
        std::any& operator[](const std::string& key) {
            int idx = findIndex(key);
            if (idx >= 0) return data_[idx].second;
            data_.emplace_back(key, std::any{});
            return data_.back().second;
        }

        /** Returns a const iterator to the beginning of the dictionary. */
        auto begin() const { return data_.begin(); }
        /** Returns a const iterator past the end of the dictionary. */
        auto end()   const { return data_.end(); }
    };

} // namespace System::Collections::Specialized
