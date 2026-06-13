// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <unordered_map>
#include <vector>

namespace System::Collections::Generic {

    /// Provides extension-style utility methods for generic collections.
    class CollectionExtensions {
    public:
        /// Deleted constructor — all members are static.
        CollectionExtensions() = delete;

        /// Returns the value for the key if it exists, or a default-constructed value otherwise.
        template<typename K, typename V>
        static V GetValueOrDefault(const std::unordered_map<K,V>& dict, const K& key) {
            auto it = dict.find(key);
            return it == dict.end() ? V{} : it->second;
        }

        /// Returns the value for the key if it exists, or the specified default value otherwise.
        template<typename K, typename V>
        static V GetValueOrDefault(const std::unordered_map<K,V>& dict, const K& key, const V& defaultValue) {
            auto it = dict.find(key);
            return it == dict.end() ? defaultValue : it->second;
        }

        /// Adds the key/value pair to the dictionary only if the key is not already present; returns true if added.
        template<typename K, typename V>
        static bool TryAdd(std::unordered_map<K,V>& dict, const K& key, const V& value) {
            return dict.emplace(key, value).second;
        }

        /// Removes the entry with the given key and outputs its value; returns true if removed.
        template<typename K, typename V>
        static bool Remove(std::unordered_map<K,V>& dict, const K& key, V& removedValue) {
            auto it = dict.find(key);
            if (it == dict.end()) return false;
            removedValue = std::move(it->second);
            dict.erase(it);
            return true;
        }

        /// Returns a const reference to the vector as a read-only wrapper.
        template<typename T>
        static const std::vector<T>& AsReadOnly(const std::vector<T>& list) {
            return list;
        }
    };

} // namespace System::Collections::Generic
