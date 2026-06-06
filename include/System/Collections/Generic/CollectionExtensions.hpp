// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <unordered_map>
#include <vector>

namespace System::Collections::Generic {

    class CollectionExtensions {
    public:
        CollectionExtensions() = delete;

        // GetValueOrDefault: returns value or default if key not found.
        template<typename K, typename V>
        static V GetValueOrDefault(const std::unordered_map<K,V>& dict, const K& key) {
            auto it = dict.find(key);
            return it == dict.end() ? V{} : it->second;
        }

        template<typename K, typename V>
        static V GetValueOrDefault(const std::unordered_map<K,V>& dict, const K& key, const V& defaultValue) {
            auto it = dict.find(key);
            return it == dict.end() ? defaultValue : it->second;
        }

        // TryAdd: adds only if key not present.
        template<typename K, typename V>
        static bool TryAdd(std::unordered_map<K,V>& dict, const K& key, const V& value) {
            return dict.emplace(key, value).second;
        }

        // Remove with out value.
        template<typename K, typename V>
        static bool Remove(std::unordered_map<K,V>& dict, const K& key, V& removedValue) {
            auto it = dict.find(key);
            if (it == dict.end()) return false;
            removedValue = std::move(it->second);
            dict.erase(it);
            return true;
        }

        // AsReadOnly: wrap a vector (returns a const reference here).
        template<typename T>
        static const std::vector<T>& AsReadOnly(const std::vector<T>& list) {
            return list;
        }
    };

} // namespace System::Collections::Generic
