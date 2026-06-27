// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <any>
#include <string>
#include <unordered_map>
#include <vector>

namespace System::Collections::Specialized {

/**
 * @brief A dictionary that uses a list for small collections and a hash table for larger ones.
 *
 * C++ counterpart of .NET System.Collections.Specialized.HybridDictionary.
 * This implementation uses std::unordered_map<std::string, std::any> throughout
 * (the adaptive list/hash strategy is a performance detail not reproduced here).
 * Keys are always strings; values are type-erased via std::any.
 */
class HybridDictionary {
    std::unordered_map<std::string, std::any> data_;

public:
    /**
     * @brief Default-constructs an empty HybridDictionary.
     *
     * C++ counterpart of .NET HybridDictionary().
     */
    HybridDictionary() = default;

    /**
     * @brief Constructs an empty HybridDictionary with optional case-insensitive key comparison.
     *
     * C++ counterpart of .NET HybridDictionary(bool).
     * The caseInsensitive parameter is accepted for API compatibility but not enforced.
     * @param caseInsensitive If true, keys are treated case-insensitively (not enforced).
     */
    explicit HybridDictionary(bool /*caseInsensitive*/) {}

    /**
     * @brief Constructs an empty HybridDictionary with an initial size hint.
     *
     * C++ counterpart of .NET HybridDictionary(int).
     * @param initialSize A hint for the initial capacity; ignored in this implementation.
     */
    explicit HybridDictionary(int /*initialSize*/) {}

    /**
     * @brief Constructs an empty HybridDictionary with an initial size hint and optional case-insensitivity.
     *
     * C++ counterpart of .NET HybridDictionary(int, bool).
     * @param initialSize     A hint for the initial capacity; ignored in this implementation.
     * @param caseInsensitive If true, keys are treated case-insensitively (not enforced).
     */
    HybridDictionary(int /*initialSize*/, bool /*caseInsensitive*/) {}

    /**
     * @brief Gets the number of key/value pairs in the dictionary.
     *
     * C++ counterpart of .NET HybridDictionary.Count.
     * @return The number of entries.
     */
    [[nodiscard]] int getCountProperty() const { return static_cast<int>(data_.size()); }

    /**
     * @brief Gets a value indicating whether the dictionary has a fixed size.
     *
     * C++ counterpart of .NET HybridDictionary.IsFixedSize.
     * @return Always false — this dictionary can grow.
     */
    [[nodiscard]] bool getIsFixedSizeProperty() const { return false; }

    /**
     * @brief Gets a value indicating whether the dictionary is read-only.
     *
     * C++ counterpart of .NET HybridDictionary.IsReadOnly.
     * @return Always false — this dictionary allows modifications.
     */
    [[nodiscard]] bool getIsReadOnlyProperty() const { return false; }

    /**
     * @brief Gets a value indicating whether access to the dictionary is thread-safe.
     *
     * C++ counterpart of .NET HybridDictionary.IsSynchronized.
     * @return Always false — this implementation is not thread-safe.
     */
    [[nodiscard]] bool getIsSynchronizedProperty() const { return false; }

    /**
     * @brief Gets an object that can be used to synchronize access to the dictionary.
     *
     * C++ counterpart of .NET HybridDictionary.SyncRoot.
     * @return A pointer to this instance (used as a synchronization token).
     */
    [[nodiscard]] const void* getSyncRootProperty() const { return this; }

    /**
     * @brief Gets a vector of all keys in the dictionary.
     *
     * C++ counterpart of .NET HybridDictionary.Keys.
     * @return A vector containing all string keys.
     */
    [[nodiscard]] std::vector<std::string> getKeysProperty() const {
        std::vector<std::string> keys;
        keys.reserve(data_.size());
        for (const auto& p : data_) keys.push_back(p.first);
        return keys;
    }

    /**
     * @brief Gets a vector of all values in the dictionary.
     *
     * C++ counterpart of .NET HybridDictionary.Values.
     * @return A vector containing all values as std::any.
     */
    [[nodiscard]] std::vector<std::any> getValuesProperty() const {
        std::vector<std::any> vals;
        vals.reserve(data_.size());
        for (const auto& p : data_) vals.push_back(p.second);
        return vals;
    }

    /**
     * @brief Returns the value for the given key, or an empty std::any if not found.
     *
     * C++ counterpart of .NET HybridDictionary.Item[object] getter.
     * @param key The key whose value to retrieve.
     * @return The value associated with @p key, or an empty std::any.
     */
    [[nodiscard]] std::any operator[](const std::string& key) const {
        auto it = data_.find(key);
        return (it != data_.end()) ? it->second : std::any{};
    }

    /**
     * @brief Sets the value for the given key, inserting or replacing as needed.
     *
     * C++ counterpart of .NET HybridDictionary.Item[object] setter.
     * @param key   The key to set.
     * @param value The value to associate with @p key.
     */
    void set(const std::string& key, const std::any& value) { data_[key] = value; }

    /**
     * @brief Adds the specified key/value pair to the dictionary.
     *
     * C++ counterpart of .NET HybridDictionary.Add(object, object).
     * If the key already exists, the value is replaced.
     * @param key   The key to add.
     * @param value The value to associate with @p key.
     */
    void Add(const std::string& key, const std::any& value) { data_[key] = value; }

    /**
     * @brief Removes the entry with the given key.
     *
     * C++ counterpart of .NET HybridDictionary.Remove(object).
     * @param key The key of the entry to remove.
     */
    void Remove(const std::string& key) { data_.erase(key); }

    /**
     * @brief Removes all entries from the dictionary.
     *
     * C++ counterpart of .NET HybridDictionary.Clear().
     */
    void Clear() { data_.clear(); }

    /**
     * @brief Determines whether the dictionary contains the given key.
     *
     * C++ counterpart of .NET HybridDictionary.Contains(object).
     * @param key The key to locate.
     * @return true if an entry with @p key exists; otherwise false.
     */
    [[nodiscard]] bool Contains(const std::string& key) const {
        return data_.find(key) != data_.end();
    }

    /** @brief Returns a const iterator to the beginning of the dictionary (STL interop). */
    auto begin() const { return data_.begin(); }
    /** @brief Returns a const iterator past the end of the dictionary (STL interop). */
    auto end()   const { return data_.end(); }
};

} // namespace System::Collections::Specialized
