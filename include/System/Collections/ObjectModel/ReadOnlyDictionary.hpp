// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include <unordered_map>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::Collections::ObjectModel {

    /** A read-only wrapper around a dictionary. */
    template<typename K, typename V>
    class ReadOnlyDictionary {
        std::shared_ptr<std::unordered_map<K, V>> dict_;

    public:
        /** Constructs a ReadOnlyDictionary wrapping the given shared dictionary. */
        explicit ReadOnlyDictionary(std::shared_ptr<std::unordered_map<K, V>> dictionary)
            : dict_(std::move(dictionary)) {}

        /** Gets the number of key/value pairs in the dictionary. */
        [[nodiscard]] SharpRuntime::intcs getCountProperty() const {
            return static_cast<SharpRuntime::intcs>(dict_->size());
        }

        /** Returns true if the dictionary contains the specified key. */
        [[nodiscard]] bool ContainsKey(const K& key) const { return dict_->count(key) > 0; }

        /** Returns a const reference to the value associated with the given key. */
        [[nodiscard]] const V& operator[](const K& key) const { return dict_->at(key); }

        /** Gets the value associated with the specified key; returns true if found. */
        [[nodiscard]] bool TryGetValue(const K& key, V& value) const {
            auto it = dict_->find(key);
            if (it == dict_->end()) return false;
            value = it->second;
            return true;
        }

        /** Returns a vector of all keys in the dictionary. */
        [[nodiscard]] std::vector<K> getKeysProperty() const {
            std::vector<K> keys;
            keys.reserve(dict_->size());
            for (auto& p : *dict_) keys.push_back(p.first);
            return keys;
        }

        /** Returns a vector of all values in the dictionary. */
        [[nodiscard]] std::vector<V> getValuesProperty() const {
            std::vector<V> vals;
            vals.reserve(dict_->size());
            for (auto& p : *dict_) vals.push_back(p.second);
            return vals;
        }

        /** Returns a const iterator to the beginning of the dictionary. */
        auto begin() const { return dict_->begin(); }
        /** Returns a const iterator past the end of the dictionary. */
        auto end()   const { return dict_->end(); }
    };

} // namespace System::Collections::ObjectModel
