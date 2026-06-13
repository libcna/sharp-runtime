// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <map>
#include <memory>
#include <stdexcept>
#include <vector>

namespace System::Collections::Immutable {

    /// An immutable dictionary whose entries are sorted by key.
    template<typename TKey, typename TValue>
    class ImmutableSortedDictionary {
        using MapT = std::map<TKey, TValue>;
        std::shared_ptr<const MapT> data_;

        explicit ImmutableSortedDictionary(std::shared_ptr<const MapT> data) : data_(std::move(data)) {}

    public:
        /// Default-constructs an empty ImmutableSortedDictionary.
        ImmutableSortedDictionary() : data_(std::make_shared<MapT>()) {}

        /// Returns an empty ImmutableSortedDictionary.
        static ImmutableSortedDictionary<TKey,TValue> Empty() { return ImmutableSortedDictionary<TKey,TValue>(); }

        /// Gets the number of key/value pairs in the dictionary.
        [[nodiscard]] int  getCountProperty()   const { return static_cast<int>(data_->size()); }
        /// Returns true if the dictionary contains no elements.
        [[nodiscard]] bool getIsEmptyProperty()  const { return data_->empty(); }

        /// Returns true if the dictionary contains the specified key.
        [[nodiscard]] bool ContainsKey(const TKey& key) const {
            return data_->find(key) != data_->end();
        }

        /// Gets the value associated with the specified key; returns true if found.
        [[nodiscard]] bool TryGetValue(const TKey& key, TValue& value) const {
            auto it = data_->find(key);
            if (it == data_->end()) return false;
            value = it->second;
            return true;
        }

        /// Returns a const reference to the value for the given key; throws if not found.
        const TValue& operator[](const TKey& key) const {
            auto it = data_->find(key);
            if (it == data_->end()) throw std::out_of_range("Key not found.");
            return it->second;
        }

        /// Returns a new dictionary with the given key/value pair added; throws if key already exists.
        ImmutableSortedDictionary<TKey,TValue> Add(const TKey& key, const TValue& value) const {
            auto m = std::make_shared<MapT>(*data_);
            if (m->find(key) != m->end()) throw std::invalid_argument("An item with the same key has already been added.");
            (*m)[key] = value;
            return ImmutableSortedDictionary<TKey,TValue>(std::move(m));
        }

        /// Returns a new dictionary with the given key set to value, replacing any existing entry.
        ImmutableSortedDictionary<TKey,TValue> SetItem(const TKey& key, const TValue& value) const {
            auto m = std::make_shared<MapT>(*data_);
            (*m)[key] = value;
            return ImmutableSortedDictionary<TKey,TValue>(std::move(m));
        }

        /// Returns a new dictionary with the entry for key removed.
        ImmutableSortedDictionary<TKey,TValue> Remove(const TKey& key) const {
            auto m = std::make_shared<MapT>(*data_);
            m->erase(key);
            return ImmutableSortedDictionary<TKey,TValue>(std::move(m));
        }

        /// Returns an empty ImmutableSortedDictionary.
        ImmutableSortedDictionary<TKey,TValue> Clear() const { return Empty(); }

        /// Returns a vector of all keys in sorted order.
        [[nodiscard]] std::vector<TKey> getKeysProperty() const {
            std::vector<TKey> keys;
            keys.reserve(data_->size());
            for (auto& kv : *data_) keys.push_back(kv.first);
            return keys;
        }

        /// Returns a vector of all values in key-sorted order.
        [[nodiscard]] std::vector<TValue> getValuesProperty() const {
            std::vector<TValue> vals;
            vals.reserve(data_->size());
            for (auto& kv : *data_) vals.push_back(kv.second);
            return vals;
        }

        /// Returns a const iterator to the beginning of the dictionary.
        auto begin() const { return data_->begin(); }
        /// Returns a const iterator past the end of the dictionary.
        auto end()   const { return data_->end(); }
    };

} // namespace System::Collections::Immutable
