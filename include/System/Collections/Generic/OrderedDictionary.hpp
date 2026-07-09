// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <functional>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Collections/Generic/KeyNotFoundException.hpp"
#include "System/Collections/Generic/KeyValuePair.hpp"

namespace System::Collections::Generic {

using SharpRuntime::intcs;

/**
 * @brief Represents a collection of key/value pairs that are accessible by key or index,
 *        preserving insertion order.
 *
 * C++ counterpart of .NET System.Collections.Generic.OrderedDictionary<TKey,TValue> (.NET 9+).
 * Backed by a std::vector (preserves order) and an std::unordered_map (O(1) key lookup).
 * Remove and RemoveAt are O(n) due to index rebuilding.
 *
 * @tparam TKey   The type of keys in the dictionary.
 * @tparam TValue The type of values in the dictionary.
 */
template<typename TKey, typename TValue>
class OrderedDictionary {
    std::vector<std::pair<TKey, TValue>>       entries_;
    std::unordered_map<TKey, std::size_t>      keyIndex_;

    void rebuildIndex() {
        keyIndex_.clear();
        for (std::size_t i = 0; i < entries_.size(); ++i)
            keyIndex_[entries_[i].first] = i;
    }

public:
    /** @brief Initializes a new empty OrderedDictionary. */
    OrderedDictionary() = default;

    /**
     * @brief Initializes a new empty OrderedDictionary with the specified initial capacity.
     *
     * C++ counterpart of .NET OrderedDictionary<TKey,TValue>(int capacity).
     * @param capacity The initial number of elements the dictionary can hold without resizing.
     * @throws System::ArgumentOutOfRangeException if @p capacity is negative.
     */
    explicit OrderedDictionary(intcs capacity) {
        if (capacity < 0)
            throw System::ArgumentOutOfRangeException("capacity");
        entries_.reserve(static_cast<std::size_t>(capacity));
        keyIndex_.reserve(static_cast<std::size_t>(capacity));
    }

    /**
     * @brief Gets the number of key/value pairs in the dictionary.
     *
     * C++ counterpart of .NET OrderedDictionary<TKey,TValue>.Count.
     * @return The number of elements.
     */
    [[nodiscard]] intcs getCountProperty() const {
        return static_cast<intcs>(entries_.size());
    }

    /**
     * @brief Gets the value associated with the specified key.
     *
     * C++ counterpart of .NET OrderedDictionary<TKey,TValue>.this[TKey] getter.
     * @param key The key whose value to get.
     * @return A const reference to the associated value.
     * @throws System::Collections::Generic::KeyNotFoundException if the key is not found.
     */
    [[nodiscard]] const TValue& operator[](const TKey& key) const {
        auto it = keyIndex_.find(key);
        if (it == keyIndex_.end()) throw KeyNotFoundException("The given key was not present in the dictionary.");
        return entries_[it->second].second;
    }

    /**
     * @brief Gets or sets the value associated with the specified key.
     *
     * C++ counterpart of .NET OrderedDictionary<TKey,TValue>.this[TKey] setter.
     * If the key does not exist, it is added at the end.
     * @param key The key whose value to get or set.
     * @return A reference to the associated value.
     */
    TValue& operator[](const TKey& key) {
        auto it = keyIndex_.find(key);
        if (it != keyIndex_.end()) return entries_[it->second].second;
        keyIndex_[key] = entries_.size();
        entries_.emplace_back(key, TValue{});
        return entries_.back().second;
    }

    /**
     * @brief Gets the key/value pair at the specified index.
     *
     * C++ counterpart of .NET OrderedDictionary<TKey,TValue>.GetAt(int).
     * @param index The zero-based index of the element.
     * @return The KeyValuePair at the specified index.
     * @throws System::ArgumentOutOfRangeException if the index is out of range.
     */
    [[nodiscard]] KeyValuePair<TKey, TValue> GetAt(intcs index) const {
        if (index < 0 || static_cast<std::size_t>(index) >= entries_.size())
            throw System::ArgumentOutOfRangeException("index");
        auto i = static_cast<std::size_t>(index);
        return {entries_[i].first, entries_[i].second};
    }

    /**
     * @brief Adds the specified key and value to the dictionary.
     *
     * C++ counterpart of .NET OrderedDictionary<TKey,TValue>.Add(TKey, TValue).
     * @param key   The key of the element to add.
     * @param value The value of the element to add.
     * @throws System::ArgumentException if the key already exists.
     */
    void Add(const TKey& key, const TValue& value) {
        if (keyIndex_.count(key)) throw System::ArgumentException("An item with the same key has already been added.");
        keyIndex_[key] = entries_.size();
        entries_.emplace_back(key, value);
    }

    /**
     * @brief Attempts to add the specified key and value to the dictionary.
     *
     * C++ counterpart of .NET OrderedDictionary<TKey,TValue>.TryAdd(TKey, TValue).
     * @param key   The key of the element to add.
     * @param value The value of the element to add.
     * @return true if the element was added; false if the key already existed.
     */
    bool TryAdd(const TKey& key, const TValue& value) {
        if (keyIndex_.count(key)) return false;
        keyIndex_[key] = entries_.size();
        entries_.emplace_back(key, value);
        return true;
    }

    /**
     * @brief Determines whether the dictionary contains an element with the specified key.
     *
     * C++ counterpart of .NET OrderedDictionary<TKey,TValue>.ContainsKey(TKey).
     * @param key The key to locate.
     * @return true if the key is found; otherwise false.
     */
    [[nodiscard]] bool ContainsKey(const TKey& key) const {
        return keyIndex_.count(key) > 0;
    }

    /**
     * @brief Determines whether the dictionary contains an element with the specified value.
     *
     * C++ counterpart of .NET OrderedDictionary<TKey,TValue>.ContainsValue(TValue).
     * @param value The value to locate.
     * @return true if the value is found in any entry; otherwise false.
     */
    [[nodiscard]] bool ContainsValue(const TValue& value) const {
        for (const auto& e : entries_)
            if (e.second == value) return true;
        return false;
    }

    /**
     * @brief Gets the value associated with the specified key.
     *
     * C++ counterpart of .NET OrderedDictionary<TKey,TValue>.TryGetValue(TKey, out TValue).
     * @param key   The key to locate.
     * @param value Receives the value if the key is found.
     * @return true if the key was found; otherwise false.
     */
    [[nodiscard]] bool TryGetValue(const TKey& key, TValue& value) const {
        auto it = keyIndex_.find(key);
        if (it == keyIndex_.end()) return false;
        value = entries_[it->second].second;
        return true;
    }

    /**
     * @brief Returns the zero-based index of the specified key, or -1 if not found.
     *
     * C++ counterpart of .NET OrderedDictionary<TKey,TValue>.IndexOf(TKey).
     * @param key The key to locate.
     * @return The insertion-order index, or -1 if not present.
     */
    [[nodiscard]] intcs IndexOf(const TKey& key) const {
        auto it = keyIndex_.find(key);
        if (it == keyIndex_.end()) return -1;
        return static_cast<intcs>(it->second);
    }

    /**
     * @brief Inserts a key/value pair at the specified index.
     *
     * C++ counterpart of .NET OrderedDictionary<TKey,TValue>.Insert(int, TKey, TValue).
     * @param index The zero-based index at which to insert.
     * @param key   The key of the element to insert.
     * @param value The value of the element to insert.
     * @throws System::ArgumentException if the key already exists.
     * @throws System::ArgumentOutOfRangeException if the index is out of range.
     */
    void Insert(intcs index, const TKey& key, const TValue& value) {
        if (index < 0 || static_cast<std::size_t>(index) > entries_.size())
            throw System::ArgumentOutOfRangeException("index");
        if (keyIndex_.count(key)) throw System::ArgumentException("An item with the same key has already been added.");
        entries_.insert(entries_.begin() + index, {key, value});
        rebuildIndex();
    }

    /**
     * @brief Updates the value of the element at the specified index.
     *
     * C++ counterpart of .NET OrderedDictionary<TKey,TValue>.SetAt(int, TValue).
     * @param index The zero-based index of the element to update.
     * @param value The new value.
     * @throws System::ArgumentOutOfRangeException if the index is out of range.
     */
    void SetAt(intcs index, const TValue& value) {
        if (index < 0 || static_cast<std::size_t>(index) >= entries_.size())
            throw System::ArgumentOutOfRangeException("index");
        entries_[static_cast<std::size_t>(index)].second = value;
    }

    /**
     * @brief Removes the element with the specified key from the dictionary.
     *
     * C++ counterpart of .NET OrderedDictionary<TKey,TValue>.Remove(TKey).
     * @param key The key of the element to remove.
     * @return true if the element was found and removed; otherwise false.
     */
    bool Remove(const TKey& key) {
        auto it = keyIndex_.find(key);
        if (it == keyIndex_.end()) return false;
        entries_.erase(entries_.begin() + static_cast<std::ptrdiff_t>(it->second));
        rebuildIndex();
        return true;
    }

    /**
     * @brief Removes the element at the specified index from the dictionary.
     *
     * C++ counterpart of .NET OrderedDictionary<TKey,TValue>.RemoveAt(int).
     * @param index The zero-based index of the element to remove.
     * @throws System::ArgumentOutOfRangeException if the index is out of range.
     */
    void RemoveAt(intcs index) {
        if (index < 0 || static_cast<std::size_t>(index) >= entries_.size())
            throw System::ArgumentOutOfRangeException("index");
        entries_.erase(entries_.begin() + index);
        rebuildIndex();
    }

    /**
     * @brief Removes all key/value pairs from the dictionary.
     *
     * C++ counterpart of .NET OrderedDictionary<TKey,TValue>.Clear().
     */
    void Clear() {
        entries_.clear();
        keyIndex_.clear();
    }

    /**
     * @brief Ensures the internal storage can hold at least @p capacity elements.
     *
     * C++ counterpart of .NET OrderedDictionary<TKey,TValue>.EnsureCapacity(int).
     * @param capacity The minimum capacity to ensure.
     * @return The new capacity.
     * @throws System::ArgumentOutOfRangeException if @p capacity is negative.
     */
    intcs EnsureCapacity(intcs capacity) {
        if (capacity < 0)
            throw System::ArgumentOutOfRangeException("capacity");
        auto cap = static_cast<std::size_t>(capacity);
        if (entries_.capacity() < cap) entries_.reserve(cap);
        return static_cast<intcs>(entries_.capacity());
    }

    /**
     * @brief Reduces the internal storage capacity to fit the current element count.
     *
     * C++ counterpart of .NET OrderedDictionary<TKey,TValue>.TrimExcess().
     */
    void TrimExcess() { entries_.shrink_to_fit(); }

    /** @brief Returns an iterator to the first entry (STL interop). */
    auto begin()       { return entries_.begin(); }
    /** @brief Returns an iterator past the last entry (STL interop). */
    auto end()         { return entries_.end(); }
    /** @brief Returns a const iterator to the first entry (STL interop). */
    auto begin() const { return entries_.cbegin(); }
    /** @brief Returns a const iterator past the last entry (STL interop). */
    auto end()   const { return entries_.cend(); }
};

} // namespace System::Collections::Generic
