// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

namespace System::Collections::Specialized {

/**
 * @brief A dictionary that maps case-insensitive string keys to string values.
 *
 * C++ counterpart of .NET System.Collections.Specialized.StringDictionary.
 * All keys are lowercased on insertion and lookup, mirroring .NET's case-insensitive
 * key comparison. Values are case-sensitive.
 */
class StringDictionary {
    std::unordered_map<std::string, std::string> data_;

    static std::string lower(const std::string& s) {
        std::string out = s;
        std::transform(out.begin(), out.end(), out.begin(), ::tolower);
        return out;
    }

public:
    /** @brief Default-constructs an empty StringDictionary. */
    StringDictionary() = default;

    /**
     * @brief Gets the number of key/value pairs in the dictionary.
     *
     * C++ counterpart of .NET StringDictionary.Count.
     * @return The number of entries.
     */
    [[nodiscard]] int getCountProperty() const { return static_cast<int>(data_.size()); }

    /**
     * @brief Gets a value indicating whether access to the dictionary is thread-safe.
     *
     * C++ counterpart of .NET StringDictionary.IsSynchronized.
     * @return Always false — this implementation is not thread-safe.
     */
    [[nodiscard]] bool getIsSynchronizedProperty() const { return false; }

    /**
     * @brief Gets an object that can be used to synchronize access to the dictionary.
     *
     * C++ counterpart of .NET StringDictionary.SyncRoot.
     * @return A pointer to this instance used as a synchronization token.
     */
    [[nodiscard]] const void* getSyncRootProperty() const { return this; }

    /**
     * @brief Gets a vector of all (lowercased) keys in the dictionary.
     *
     * C++ counterpart of .NET StringDictionary.Keys.
     * @return A vector containing all keys.
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
     * C++ counterpart of .NET StringDictionary.Values.
     * @return A vector containing all values.
     */
    [[nodiscard]] std::vector<std::string> getValuesProperty() const {
        std::vector<std::string> vals;
        vals.reserve(data_.size());
        for (const auto& p : data_) vals.push_back(p.second);
        return vals;
    }

    /**
     * @brief Returns a reference to the value for the given key (key is lowercased).
     *
     * C++ counterpart of .NET StringDictionary.Item[string] setter.
     * @param key The key (case-insensitive) whose value to access.
     * @return A reference to the associated value.
     */
    std::string& operator[](const std::string& key) { return data_[lower(key)]; }

    /**
     * @brief Adds or replaces the value for the given key.
     *
     * C++ counterpart of .NET StringDictionary.Add(string, string).
     * Keys are stored and looked up in lowercase.
     * @param key   The key to add (case-insensitive).
     * @param value The value to associate with @p key.
     */
    void Add(const std::string& key, const std::string& value) {
        data_[lower(key)] = value;
    }

    /**
     * @brief Removes the entry with the given key.
     *
     * C++ counterpart of .NET StringDictionary.Remove(string).
     * @param key The key to remove (case-insensitive).
     */
    void Remove(const std::string& key) { data_.erase(lower(key)); }

    /**
     * @brief Removes all entries from the dictionary.
     *
     * C++ counterpart of .NET StringDictionary.Clear().
     */
    void Clear() { data_.clear(); }

    /**
     * @brief Determines whether the dictionary contains the given key.
     *
     * C++ counterpart of .NET StringDictionary.ContainsKey(string).
     * @param key The key to locate (case-insensitive).
     * @return true if the key is found; otherwise false.
     */
    [[nodiscard]] bool ContainsKey(const std::string& key) const {
        return data_.find(lower(key)) != data_.end();
    }

    /**
     * @brief Determines whether any entry has the specified value.
     *
     * C++ counterpart of .NET StringDictionary.ContainsValue(string).
     * @param value The value to locate (case-sensitive).
     * @return true if any entry has @p value; otherwise false.
     */
    [[nodiscard]] bool ContainsValue(const std::string& value) const {
        for (const auto& [k, v] : data_) if (v == value) return true;
        return false;
    }

    /**
     * @brief Returns the value for the given key, or an empty string if not found.
     *
     * C++ counterpart of reading .NET StringDictionary.Item[string] when key is absent.
     * @param key The key to look up (case-insensitive).
     * @return The associated value, or "" if not found.
     */
    [[nodiscard]] std::string GetValue(const std::string& key) const {
        auto it = data_.find(lower(key));
        return it != data_.end() ? it->second : std::string{};
    }

    /** @brief Returns a const iterator to the beginning of the dictionary (STL interop). */
    auto begin() const { return data_.begin(); }
    /** @brief Returns a const iterator past the end of the dictionary (STL interop). */
    auto end()   const { return data_.end(); }
};

} // namespace System::Collections::Specialized
