// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <algorithm>
#include <string>
#include <unordered_map>

namespace System::Collections::Specialized {

    /** A dictionary that maps string keys (lowercased) to string values. */
    class StringDictionary {
        std::unordered_map<std::string, std::string> data_;

        static std::string lower(const std::string& s) {
            std::string out = s;
            std::transform(out.begin(), out.end(), out.begin(), ::tolower);
            return out;
        }

    public:
        /** Default-constructs an empty StringDictionary. */
        StringDictionary() = default;

        /** Gets the number of key/value pairs in the dictionary. */
        [[nodiscard]] int  getCountProperty()    const { return static_cast<int>(data_.size()); }
        /** Returns false (this implementation is not synchronized). */
        [[nodiscard]] bool getIsSynchronizedProperty() const { return false; }

        /** Returns a reference to the value for the given key (key is lowercased). */
        std::string& operator[](const std::string& key) { return data_[lower(key)]; }

        /** Adds or replaces the value for the given key (key is lowercased). */
        void Add(const std::string& key, const std::string& value) {
            data_[lower(key)] = value;
        }

        /** Removes the entry with the given key (key is lowercased). */
        void Remove(const std::string& key) { data_.erase(lower(key)); }

        /** Removes all entries from the dictionary. */
        void Clear() { data_.clear(); }

        /** Returns true if the dictionary contains the given key (case-insensitive). */
        [[nodiscard]] bool ContainsKey(const std::string& key) const {
            return data_.find(lower(key)) != data_.end();
        }

        /** Returns true if any entry has the specified value. */
        [[nodiscard]] bool ContainsValue(const std::string& value) const {
            for (auto& [k, v] : data_) if (v == value) return true;
            return false;
        }

        /** Returns the value for the given key, or an empty string if not found. */
        [[nodiscard]] std::string GetValue(const std::string& key) const {
            auto it = data_.find(lower(key));
            return it != data_.end() ? it->second : std::string{};
        }

        /** Returns a const iterator to the beginning of the dictionary. */
        auto begin() const { return data_.begin(); }
        /** Returns a const iterator past the end of the dictionary. */
        auto end()   const { return data_.end(); }
    };

} // namespace System::Collections::Specialized
