// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <any>
#include <string>
#include <unordered_map>

namespace System::Collections::Specialized {

    /** A dictionary that uses a list for small sizes and a hash table for larger sizes (stub uses hash throughout). */
    class HybridDictionary {
        std::unordered_map<std::string, std::any> data_;

    public:
        /** Default-constructs an empty HybridDictionary. */
        HybridDictionary() = default;
        /** Constructs a HybridDictionary; the caseInsensitive parameter is accepted but not implemented. */
        explicit HybridDictionary(bool /*caseInsensitive*/) {}

        /** Gets the number of key/value pairs in the dictionary. */
        [[nodiscard]] int  getCountProperty()    const { return static_cast<int>(data_.size()); }
        /** Returns false (this dictionary is not read-only). */
        [[nodiscard]] bool getIsReadOnlyProperty() const { return false; }

        /** Adds or replaces the value for the given key. */
        void Add(const std::string& key, const std::any& value) {
            data_[key] = value;
        }

        /** Removes the entry with the given key. */
        void Remove(const std::string& key) { data_.erase(key); }
        /** Removes all entries from the dictionary. */
        void Clear() { data_.clear(); }

        /** Returns true if the dictionary contains the given key. */
        [[nodiscard]] bool Contains(const std::string& key) const {
            return data_.find(key) != data_.end();
        }

        /** Returns the value for the given key, or an empty std::any if not found. */
        std::any operator[](const std::string& key) const {
            auto it = data_.find(key);
            return (it != data_.end()) ? it->second : std::any{};
        }

        /** Returns a const iterator to the beginning of the dictionary. */
        auto begin() const { return data_.begin(); }
        /** Returns a const iterator past the end of the dictionary. */
        auto end()   const { return data_.end(); }
    };

} // namespace System::Collections::Specialized
