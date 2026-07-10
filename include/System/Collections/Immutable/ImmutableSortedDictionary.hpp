// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <map>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentException.hpp"
#include "System/Collections/Generic/KeyNotFoundException.hpp"

namespace System::Collections::Immutable {

using SharpRuntime::intcs;
using System::Collections::Generic::KeyNotFoundException;

/**
 * @brief Represents an immutable dictionary whose entries are sorted by key.
 *
 * C++ counterpart of .NET System.Collections.Immutable.ImmutableSortedDictionary<TKey,TValue>.
 * Backed by std::map; provides O(log n) key access with sorted iteration.
 * All mutating operations return a new instance; the original is unchanged.
 *
 * @tparam TKey   The type of the keys (must support operator<).
 * @tparam TValue The type of the values.
 */
template<typename TKey, typename TValue>
class ImmutableSortedDictionary {
    using MapT = std::map<TKey, TValue>;
    std::shared_ptr<const MapT> data_;

    explicit ImmutableSortedDictionary(std::shared_ptr<const MapT> data) : data_(std::move(data)) {}

public:
    /** @brief Default-constructs an empty ImmutableSortedDictionary. */
    ImmutableSortedDictionary() : data_(std::make_shared<MapT>()) {}

    /**
     * @brief Returns an empty ImmutableSortedDictionary.
     *
     * C++ counterpart of .NET ImmutableSortedDictionary<TKey,TValue>.Empty.
     * @return An empty ImmutableSortedDictionary<TKey,TValue>.
     */
    static ImmutableSortedDictionary<TKey, TValue> Empty() {
        return ImmutableSortedDictionary<TKey, TValue>();
    }

    /**
     * @brief Gets the number of key/value pairs in the dictionary.
     *
     * C++ counterpart of .NET ImmutableSortedDictionary<TKey,TValue>.Count.
     * @return The number of elements.
     */
    [[nodiscard]] intcs getCountProperty() const { return static_cast<intcs>(data_->size()); }

    /**
     * @brief Gets a value indicating whether the dictionary is empty.
     *
     * C++ counterpart of .NET ImmutableSortedDictionary<TKey,TValue>.IsEmpty.
     * @return true if the dictionary contains no elements; otherwise false.
     */
    [[nodiscard]] bool getIsEmptyProperty() const { return data_->empty(); }

    /**
     * @brief Determines whether the dictionary contains the specified key.
     *
     * C++ counterpart of .NET ImmutableSortedDictionary<TKey,TValue>.ContainsKey(TKey).
     * @param key The key to locate.
     * @return true if the key is found; otherwise false.
     */
    [[nodiscard]] bool ContainsKey(const TKey& key) const {
        return data_->find(key) != data_->end();
    }

    /**
     * @brief Determines whether any entry has the specified value.
     *
     * C++ counterpart of .NET ImmutableSortedDictionary<TKey,TValue>.ContainsValue(TValue).
     * @param value The value to locate.
     * @return true if at least one entry has the value; otherwise false.
     */
    [[nodiscard]] bool ContainsValue(const TValue& value) const {
        for (const auto& kv : *data_) if (kv.second == value) return true;
        return false;
    }

    /**
     * @brief Determines whether the dictionary contains the specified key/value pair.
     *
     * C++ counterpart of .NET ImmutableSortedDictionary<TKey,TValue>.Contains(KeyValuePair<TKey,TValue>).
     * @param pair The key/value pair to locate.
     * @return true if the exact pair is found; otherwise false.
     */
    [[nodiscard]] bool Contains(const std::pair<TKey, TValue>& pair) const {
        auto it = data_->find(pair.first);
        return it != data_->end() && it->second == pair.second;
    }

    /**
     * @brief Gets the value associated with the specified key.
     *
     * C++ counterpart of .NET ImmutableSortedDictionary<TKey,TValue>.TryGetValue(TKey, out TValue).
     * @param key   The key to locate.
     * @param value Receives the value if found.
     * @return true if the key was found; otherwise false.
     */
    [[nodiscard]] bool TryGetValue(const TKey& key, TValue& value) const {
        auto it = data_->find(key);
        if (it == data_->end()) return false;
        value = it->second;
        return true;
    }

    /**
     * @brief Returns a const reference to the value for the given key.
     *
     * C++ counterpart of .NET ImmutableSortedDictionary<TKey,TValue>.Item[TKey] getter.
     * @param key The key whose value to get.
     * @return A const reference to the associated value.
     * @throws System::Collections::Generic::KeyNotFoundException if the key is not found.
     */
    const TValue& operator[](const TKey& key) const {
        auto it = data_->find(key);
        if (it == data_->end()) throw KeyNotFoundException("The given key was not present in the dictionary.");
        return it->second;
    }

    /**
     * @brief Returns a new dictionary with the specified key/value pair added.
     *
     * C++ counterpart of .NET ImmutableSortedDictionary<TKey,TValue>.Add(TKey, TValue).
     * Verified against ImmutableSortedDictionary_2.Node.SetOrAdd: if @p key already exists
     * with a value equal to @p value, this is a silent no-op (returns an equal instance,
     * not a copy-with-mutation) rather than throwing -- .NET only throws when the existing
     * value actually differs.
     * @param key   The key to add.
     * @param value The value to add.
     * @return A new ImmutableSortedDictionary with the pair added, or an equivalent instance
     *         if @p key already maps to @p value.
     * @throws System::ArgumentException if the key already exists with a different value.
     */
    [[nodiscard]] ImmutableSortedDictionary<TKey, TValue>
        Add(const TKey& key, const TValue& value) const {
        auto it = data_->find(key);
        if (it != data_->end()) {
            if (it->second == value) return *this;
            throw System::ArgumentException("An item with the same key has already been added.");
        }
        auto m = std::make_shared<MapT>(*data_);
        (*m)[key] = value;
        return ImmutableSortedDictionary<TKey, TValue>(std::move(m));
    }

    /**
     * @brief Returns a new dictionary with multiple key/value pairs added.
     *
     * C++ counterpart of .NET ImmutableSortedDictionary<TKey,TValue>.AddRange(IEnumerable<KeyValuePair<TKey,TValue>>).
     * Each pair is applied via the same equal-value-is-a-no-op rule as Add(TKey, TValue);
     * see that method's doc comment for the verification detail.
     * @param pairs The pairs to add.
     * @return A new ImmutableSortedDictionary with all pairs added.
     * @throws System::ArgumentException if any key already exists with a different value.
     */
    [[nodiscard]] ImmutableSortedDictionary<TKey, TValue>
        AddRange(const std::vector<std::pair<TKey, TValue>>& pairs) const {
        auto m = std::make_shared<MapT>(*data_);
        for (const auto& p : pairs) {
            auto it = m->find(p.first);
            if (it != m->end()) {
                if (it->second == p.second) continue;
                throw System::ArgumentException("An item with the same key has already been added.");
            }
            (*m)[p.first] = p.second;
        }
        return ImmutableSortedDictionary<TKey, TValue>(std::move(m));
    }

    /**
     * @brief Returns a new dictionary with the given key set to @p value, inserting or overwriting.
     *
     * C++ counterpart of .NET ImmutableSortedDictionary<TKey,TValue>.SetItem(TKey, TValue).
     * @param key   The key to set.
     * @param value The new value.
     * @return A new ImmutableSortedDictionary with the key set.
     */
    [[nodiscard]] ImmutableSortedDictionary<TKey, TValue>
        SetItem(const TKey& key, const TValue& value) const {
        auto m = std::make_shared<MapT>(*data_);
        (*m)[key] = value;
        return ImmutableSortedDictionary<TKey, TValue>(std::move(m));
    }

    /**
     * @brief Returns a new dictionary with the given key/value pairs set.
     *
     * C++ counterpart of .NET ImmutableSortedDictionary<TKey,TValue>.SetItems(IEnumerable<KeyValuePair<TKey,TValue>>).
     * @param items The pairs to set (insert or overwrite).
     * @return A new ImmutableSortedDictionary with all pairs set.
     */
    [[nodiscard]] ImmutableSortedDictionary<TKey, TValue>
        SetItems(const std::vector<std::pair<TKey, TValue>>& items) const {
        auto m = std::make_shared<MapT>(*data_);
        for (const auto& p : items) (*m)[p.first] = p.second;
        return ImmutableSortedDictionary<TKey, TValue>(std::move(m));
    }

    /**
     * @brief Returns a new dictionary with the entry for @p key removed.
     *
     * C++ counterpart of .NET ImmutableSortedDictionary<TKey,TValue>.Remove(TKey).
     * @param key The key to remove.
     * @return A new ImmutableSortedDictionary without the specified key.
     */
    [[nodiscard]] ImmutableSortedDictionary<TKey, TValue> Remove(const TKey& key) const {
        auto m = std::make_shared<MapT>(*data_);
        m->erase(key);
        return ImmutableSortedDictionary<TKey, TValue>(std::move(m));
    }

    /**
     * @brief Returns a new dictionary with multiple keys removed.
     *
     * C++ counterpart of .NET ImmutableSortedDictionary<TKey,TValue>.RemoveRange(IEnumerable<TKey>).
     * @param keys The keys to remove.
     * @return A new ImmutableSortedDictionary without the specified keys.
     */
    [[nodiscard]] ImmutableSortedDictionary<TKey, TValue>
        RemoveRange(const std::vector<TKey>& keys) const {
        auto m = std::make_shared<MapT>(*data_);
        for (const auto& k : keys) m->erase(k);
        return ImmutableSortedDictionary<TKey, TValue>(std::move(m));
    }

    /**
     * @brief Returns an empty ImmutableSortedDictionary.
     *
     * C++ counterpart of .NET ImmutableSortedDictionary<TKey,TValue>.Clear().
     * @return An empty ImmutableSortedDictionary.
     */
    [[nodiscard]] ImmutableSortedDictionary<TKey, TValue> Clear() const { return Empty(); }

    /**
     * @brief Gets all keys in sorted order.
     *
     * C++ counterpart of .NET ImmutableSortedDictionary<TKey,TValue>.Keys.
     * @return A std::vector<TKey> of all keys in ascending order.
     */
    [[nodiscard]] std::vector<TKey> getKeysProperty() const {
        std::vector<TKey> keys;
        keys.reserve(data_->size());
        for (const auto& kv : *data_) keys.push_back(kv.first);
        return keys;
    }

    /**
     * @brief Gets all values in key-sorted order.
     *
     * C++ counterpart of .NET ImmutableSortedDictionary<TKey,TValue>.Values.
     * @return A std::vector<TValue> of all values in key-ascending order.
     */
    [[nodiscard]] std::vector<TValue> getValuesProperty() const {
        std::vector<TValue> vals;
        vals.reserve(data_->size());
        for (const auto& kv : *data_) vals.push_back(kv.second);
        return vals;
    }

    /** @brief Returns a const iterator to the beginning of the dictionary (STL interop). */
    auto begin() const { return data_->begin(); }
    /** @brief Returns a const iterator past the end of the dictionary (STL interop). */
    auto end()   const { return data_->end(); }
};

} // namespace System::Collections::Immutable
