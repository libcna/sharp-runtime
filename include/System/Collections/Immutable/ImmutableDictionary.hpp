// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace System::Collections::Immutable {

    template<typename TKey, typename TValue>
    class ImmutableDictionary {
        using MapT = std::unordered_map<TKey, TValue>;
        std::shared_ptr<const MapT> data_;

        explicit ImmutableDictionary(std::shared_ptr<const MapT> data) : data_(std::move(data)) {}

    public:
        ImmutableDictionary() : data_(std::make_shared<MapT>()) {}

        static ImmutableDictionary<TKey,TValue> Empty() { return ImmutableDictionary<TKey,TValue>(); }

        [[nodiscard]] int  getCountProperty()   const { return static_cast<int>(data_->size()); }
        [[nodiscard]] bool getIsEmptyProperty()  const { return data_->empty(); }

        [[nodiscard]] bool ContainsKey(const TKey& key) const {
            return data_->find(key) != data_->end();
        }

        [[nodiscard]] bool TryGetValue(const TKey& key, TValue& value) const {
            auto it = data_->find(key);
            if (it == data_->end()) return false;
            value = it->second;
            return true;
        }

        const TValue& operator[](const TKey& key) const {
            auto it = data_->find(key);
            if (it == data_->end()) throw std::out_of_range("Key not found.");
            return it->second;
        }

        ImmutableDictionary<TKey,TValue> Add(const TKey& key, const TValue& value) const {
            auto m = std::make_shared<MapT>(*data_);
            if (m->find(key) != m->end()) throw std::invalid_argument("An item with the same key has already been added.");
            (*m)[key] = value;
            return ImmutableDictionary<TKey,TValue>(std::move(m));
        }

        ImmutableDictionary<TKey,TValue> SetItem(const TKey& key, const TValue& value) const {
            auto m = std::make_shared<MapT>(*data_);
            (*m)[key] = value;
            return ImmutableDictionary<TKey,TValue>(std::move(m));
        }

        ImmutableDictionary<TKey,TValue> Remove(const TKey& key) const {
            auto m = std::make_shared<MapT>(*data_);
            m->erase(key);
            return ImmutableDictionary<TKey,TValue>(std::move(m));
        }

        ImmutableDictionary<TKey,TValue> Clear() const {
            return Empty();
        }

        [[nodiscard]] std::vector<TKey> getKeysProperty() const {
            std::vector<TKey> keys;
            keys.reserve(data_->size());
            for (auto& kv : *data_) keys.push_back(kv.first);
            return keys;
        }

        [[nodiscard]] std::vector<TValue> getValuesProperty() const {
            std::vector<TValue> vals;
            vals.reserve(data_->size());
            for (auto& kv : *data_) vals.push_back(kv.second);
            return vals;
        }

        auto begin() const { return data_->begin(); }
        auto end()   const { return data_->end(); }
    };

} // namespace System::Collections::Immutable
