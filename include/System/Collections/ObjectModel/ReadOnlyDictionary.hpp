// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include <unordered_map>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::Collections::ObjectModel {

    template<typename K, typename V>
    class ReadOnlyDictionary {
        std::shared_ptr<std::unordered_map<K, V>> dict_;

    public:
        explicit ReadOnlyDictionary(std::shared_ptr<std::unordered_map<K, V>> dictionary)
            : dict_(std::move(dictionary)) {}

        [[nodiscard]] SharpRuntime::intcs getCountProperty() const {
            return static_cast<SharpRuntime::intcs>(dict_->size());
        }

        [[nodiscard]] bool ContainsKey(const K& key) const { return dict_->count(key) > 0; }

        [[nodiscard]] const V& operator[](const K& key) const { return dict_->at(key); }

        [[nodiscard]] bool TryGetValue(const K& key, V& value) const {
            auto it = dict_->find(key);
            if (it == dict_->end()) return false;
            value = it->second;
            return true;
        }

        [[nodiscard]] std::vector<K> getKeysProperty() const {
            std::vector<K> keys;
            keys.reserve(dict_->size());
            for (auto& p : *dict_) keys.push_back(p.first);
            return keys;
        }

        [[nodiscard]] std::vector<V> getValuesProperty() const {
            std::vector<V> vals;
            vals.reserve(dict_->size());
            for (auto& p : *dict_) vals.push_back(p.second);
            return vals;
        }

        // Range-for support
        auto begin() const { return dict_->begin(); }
        auto end()   const { return dict_->end(); }
    };

} // namespace System::Collections::ObjectModel
