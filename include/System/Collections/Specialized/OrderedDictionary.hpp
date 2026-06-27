// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::Collections::Specialized {

using SharpRuntime::intcs;

/**
 * @brief A dictionary that preserves insertion order and allows access by key or integer index.
 *
 * C++ counterpart of .NET System.Collections.Specialized.OrderedDictionary.
 * Keys and values are std::string (non-generic, mirroring the .NET non-generic version).
 * Provides O(1) key lookup via an internal index map and O(1) index access via parallel vectors.
 */
class OrderedDictionary {
    std::vector<std::string>               keys_;
    std::vector<std::string>               values_;
    std::unordered_map<std::string, intcs> index_;
    bool                                   readOnly_ = false;

    void rebuildIndex() {
        index_.clear();
        for (intcs i = 0; i < static_cast<intcs>(keys_.size()); ++i)
            index_[keys_[static_cast<size_t>(i)]] = i;
    }

    void checkMutable() const {
        if (readOnly_) throw std::runtime_error("Collection is read-only.");
    }

public:
    /**
     * @brief Default-constructs an empty OrderedDictionary.
     *
     * C++ counterpart of .NET OrderedDictionary().
     */
    OrderedDictionary() = default;

    /**
     * @brief Constructs an empty OrderedDictionary with an optional comparer.
     *
     * C++ counterpart of .NET OrderedDictionary(IEqualityComparer).
     * The comparer is accepted for API compatibility but ignored.
     */
    explicit OrderedDictionary(std::nullptr_t /*comparer*/) {}

    /**
     * @brief Constructs an empty OrderedDictionary with an initial capacity hint.
     *
     * C++ counterpart of .NET OrderedDictionary(int).
     * @param capacity Initial capacity hint; ignored in this implementation.
     */
    explicit OrderedDictionary(int /*capacity*/) {}

    /**
     * @brief Constructs an empty OrderedDictionary with a capacity hint and optional comparer.
     *
     * C++ counterpart of .NET OrderedDictionary(int, IEqualityComparer).
     * @param capacity  Initial capacity hint; ignored.
     * @param comparer  Equality comparer; accepted for API compatibility but ignored.
     */
    OrderedDictionary(int /*capacity*/, std::nullptr_t /*comparer*/) {}

    /**
     * @brief Gets the number of key/value pairs in the dictionary.
     *
     * C++ counterpart of .NET OrderedDictionary.Count.
     * @return The number of entries.
     */
    [[nodiscard]] intcs getCountProperty() const { return static_cast<intcs>(keys_.size()); }

    /**
     * @brief Gets a value indicating whether the dictionary is read-only.
     *
     * C++ counterpart of .NET OrderedDictionary.IsReadOnly.
     * @return true after AsReadOnly() has been called; otherwise false.
     */
    [[nodiscard]] bool getIsReadOnlyProperty() const { return readOnly_; }

    /**
     * @brief Gets a value indicating whether the dictionary has a fixed size.
     *
     * C++ counterpart of .NET IDictionary.IsFixedSize (explicit implementation).
     * @return Always false — this dictionary can grow.
     */
    [[nodiscard]] bool getIsFixedSizeProperty() const { return false; }

    /**
     * @brief Gets a value indicating whether access to the dictionary is thread-safe.
     *
     * C++ counterpart of .NET ICollection.IsSynchronized (explicit implementation).
     * @return Always false — this implementation is not thread-safe.
     */
    [[nodiscard]] bool getIsSynchronizedProperty() const { return false; }

    /**
     * @brief Gets an object that can be used to synchronize access to the dictionary.
     *
     * C++ counterpart of .NET ICollection.SyncRoot (explicit implementation).
     * @return A pointer to this instance used as a synchronization token.
     */
    [[nodiscard]] const void* getSyncRootProperty() const { return this; }

    /**
     * @brief Gets a vector of all keys in insertion order.
     *
     * C++ counterpart of .NET OrderedDictionary.Keys.
     * @return A const reference to the ordered key vector.
     */
    [[nodiscard]] const std::vector<std::string>& getKeysProperty() const { return keys_; }

    /**
     * @brief Gets a vector of all values in insertion order.
     *
     * C++ counterpart of .NET OrderedDictionary.Values.
     * @return A const reference to the ordered value vector.
     */
    [[nodiscard]] const std::vector<std::string>& getValuesProperty() const { return values_; }

    /**
     * @brief Returns a read-only wrapper for this dictionary.
     *
     * C++ counterpart of .NET OrderedDictionary.AsReadOnly().
     * Marks this instance as read-only and returns a reference to it.
     * @return A reference to this instance in read-only mode.
     */
    OrderedDictionary& AsReadOnly() {
        readOnly_ = true;
        return *this;
    }

    /**
     * @brief Adds a key/value pair at the end of the dictionary.
     *
     * C++ counterpart of .NET OrderedDictionary.Add(object, object).
     * @param key   The key to add.
     * @param value The value to associate with @p key.
     * @throws std::runtime_error if the dictionary is read-only.
     * @throws std::invalid_argument if @p key already exists.
     */
    void Add(const std::string& key, const std::string& value) {
        checkMutable();
        if (index_.count(key)) throw std::invalid_argument("Key already exists: " + key);
        index_[key] = static_cast<intcs>(keys_.size());
        keys_.push_back(key);
        values_.push_back(value);
    }

    /**
     * @brief Inserts a key/value pair at the specified zero-based index.
     *
     * C++ counterpart of .NET OrderedDictionary.Insert(int, object, object).
     * @param index The position at which to insert.
     * @param key   The key to insert.
     * @param value The value to associate with @p key.
     * @throws std::runtime_error if the dictionary is read-only.
     * @throws std::invalid_argument if @p key already exists.
     */
    void Insert(intcs index, const std::string& key, const std::string& value) {
        checkMutable();
        if (index_.count(key)) throw std::invalid_argument("Key already exists: " + key);
        keys_.insert(keys_.begin() + index, key);
        values_.insert(values_.begin() + index, value);
        rebuildIndex();
    }

    /**
     * @brief Removes the entry with the given key.
     *
     * C++ counterpart of .NET OrderedDictionary.Remove(object).
     * Does nothing if the key is not found.
     * @param key The key to remove.
     * @throws std::runtime_error if the dictionary is read-only.
     */
    void Remove(const std::string& key) {
        checkMutable();
        auto it = index_.find(key);
        if (it == index_.end()) return;
        intcs i = it->second;
        keys_.erase(keys_.begin() + i);
        values_.erase(values_.begin() + i);
        rebuildIndex();
    }

    /**
     * @brief Removes the entry at the specified zero-based index.
     *
     * C++ counterpart of .NET OrderedDictionary.RemoveAt(int).
     * @param index The zero-based index of the entry to remove.
     * @throws std::runtime_error if the dictionary is read-only.
     * @throws std::out_of_range if @p index is out of range.
     */
    void RemoveAt(intcs index) {
        checkMutable();
        if (index < 0 || index >= getCountProperty()) throw std::out_of_range("index out of range");
        Remove(keys_[static_cast<size_t>(index)]);
    }

    /**
     * @brief Removes all entries from the dictionary.
     *
     * C++ counterpart of .NET OrderedDictionary.Clear().
     * @throws std::runtime_error if the dictionary is read-only.
     */
    void Clear() {
        checkMutable();
        keys_.clear();
        values_.clear();
        index_.clear();
    }

    /**
     * @brief Determines whether the dictionary contains the given key.
     *
     * C++ counterpart of .NET OrderedDictionary.Contains(object).
     * @param key The key to locate.
     * @return true if an entry with @p key exists; otherwise false.
     */
    [[nodiscard]] bool Contains(const std::string& key) const { return index_.count(key) > 0; }

    /**
     * @brief Returns a const reference to the value for the given key.
     *
     * C++ counterpart of .NET OrderedDictionary.Item[object] getter.
     * @param key The key whose value to retrieve.
     * @return A const reference to the value.
     * @throws std::out_of_range if @p key is not found.
     */
    [[nodiscard]] const std::string& operator[](const std::string& key) const {
        auto it = index_.find(key);
        if (it == index_.end()) throw std::out_of_range("Key not found: " + key);
        return values_[static_cast<size_t>(it->second)];
    }

    /**
     * @brief Returns a reference to the value for the given key.
     *
     * C++ counterpart of .NET OrderedDictionary.Item[object] setter.
     * @param key The key whose value to access.
     * @return A reference to the value.
     * @throws std::out_of_range if @p key is not found.
     */
    std::string& operator[](const std::string& key) {
        auto it = index_.find(key);
        if (it == index_.end()) throw std::out_of_range("Key not found: " + key);
        return values_[static_cast<size_t>(it->second)];
    }

    /**
     * @brief Returns the value at the given zero-based index.
     *
     * C++ counterpart of .NET OrderedDictionary.Item[int] getter.
     * @param i The zero-based index.
     * @return A const reference to the value.
     * @throws std::out_of_range if @p i is out of range.
     */
    [[nodiscard]] const std::string& GetByIndex(intcs i) const {
        return values_.at(static_cast<size_t>(i));
    }

    /**
     * @brief Returns the key at the given zero-based index.
     *
     * C++ counterpart of accessing the key by position in .NET OrderedDictionary.
     * @param i The zero-based index.
     * @return A const reference to the key string.
     * @throws std::out_of_range if @p i is out of range.
     */
    [[nodiscard]] const std::string& GetKey(intcs i) const {
        return keys_.at(static_cast<size_t>(i));
    }
};

} // namespace System::Collections::Specialized
