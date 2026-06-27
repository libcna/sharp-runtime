// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <any>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include "System/Collections/IDictionary.hpp"
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::Collections {

using SharpRuntime::bytecs;
using SharpRuntime::intcs;

/**
 * @brief Represents a collection of key/value pairs organized based on the hash code of the key.
 *
 * C++ counterpart of .NET System.Collections.Hashtable.
 * Keys are stored as std::string (address-stringified for void* keys, or direct for string overloads).
 * Values are stored as std::any.
 */
class Hashtable : public IDictionary {
public:
    /** @brief Constructs an empty Hashtable. */
    Hashtable() = default;
    /** @brief Constructs a Hashtable; the capacity hint is accepted but ignored in this port. */
    explicit Hashtable(int /*capacity*/) {}

    /**
     * @brief Returns the number of key/value pairs in the table.
     * C++ counterpart of .NET Hashtable.Count.
     */
    [[nodiscard]] int getCountProperty() const override {
        return static_cast<int>(_map.size());
    }

    /**
     * @brief Copies elements into a destination buffer; not fully implemented in this stub.
     * C++ counterpart of .NET Hashtable.CopyTo(Array, int).
     */
    void CopyTo(void* /*array*/, int /*index*/) override {}

    /** @brief Returns false; Hashtable is never read-only. */
    [[nodiscard]] bool getIsReadOnlyProperty()  const override { return false; }
    /** @brief Returns false; Hashtable is never fixed-size. */
    [[nodiscard]] bool getIsFixedSizeProperty() const override { return false; }
    /** @brief Returns false; Hashtable is not thread-safe. */
    [[nodiscard]] bool getIsSynchronizedProperty() const override { return false; }

    /**
     * @brief Gets the value associated with the specified key.
     * C++ counterpart of .NET Hashtable indexer getter (this[object key]).
     */
    [[nodiscard]] void* getItem(const void* key) const override {
        auto k = toKey(key);
        auto it = _map.find(k);
        if (it == _map.end()) return nullptr;
        return const_cast<std::any*>(&it->second);
    }

    /**
     * @brief Sets the value associated with the specified key.
     * C++ counterpart of .NET Hashtable indexer setter (this[object key] = value).
     */
    void setItem(const void* key, void* value) override {
        _map[toKey(key)] = value ? *static_cast<std::any*>(value) : std::any{};
    }

    /**
     * @brief Gets an ICollection containing the keys of the Hashtable.
     * C++ counterpart of .NET Hashtable.Keys.
     * Returns nullptr in this stub (full key collection not implemented).
     */
    [[nodiscard]] ICollection* getKeysProperty() const override { return nullptr; }

    /**
     * @brief Gets an ICollection containing the values of the Hashtable.
     * C++ counterpart of .NET Hashtable.Values.
     * Returns nullptr in this stub (full value collection not implemented).
     */
    [[nodiscard]] ICollection* getValuesProperty() const override { return nullptr; }

    /**
     * @brief Returns true if the Hashtable contains the specified key.
     * C++ counterpart of .NET Hashtable.Contains(object).
     */
    [[nodiscard]] bool Contains(const void* key) const override {
        return _map.count(toKey(key)) > 0;
    }

    /**
     * @brief Returns true if the table contains the specified string key.
     * @param key String key to look up.
     */
    bool ContainsKey(const std::string& key) const { return _map.count(key) > 0; }

    /**
     * @brief Returns true if any stored value has the same type as @p value.
     * C++ counterpart of .NET Hashtable.ContainsValue(object?).
     */
    bool ContainsValue(const std::any& value) const {
        for (const auto& [k, v] : _map)
            if (v.type() == value.type()) return true;
        return false;
    }

    /**
     * @brief Adds an element with the specified key and value.
     * C++ counterpart of .NET Hashtable.Add(object, object?).
     * @throws std::invalid_argument if the key already exists.
     */
    void Add(const void* key, void* value) override {
        std::string k = toKey(key);
        if (_map.count(k)) throw std::invalid_argument("key already exists");
        _map[k] = value ? *static_cast<std::any*>(value) : std::any{};
    }

    /**
     * @brief Adds a string key with a typed value.
     * @throws std::invalid_argument if the key already exists.
     */
    void Add(const std::string& key, const std::any& value) {
        if (_map.count(key)) throw std::invalid_argument("key already exists");
        _map[key] = value;
    }

    /**
     * @brief Removes all key/value pairs from the table.
     * C++ counterpart of .NET Hashtable.Clear().
     */
    void Clear() override { _map.clear(); }

    /**
     * @brief Removes the element with the specified key.
     * C++ counterpart of .NET Hashtable.Remove(object).
     */
    void Remove(const void* key) override { _map.erase(toKey(key)); }

    /**
     * @brief Removes the entry with the specified string key.
     * @param key String key to remove.
     */
    void Remove(const std::string& key) { _map.erase(key); }

    /**
     * @brief Removes the entry with the specified C-string key.
     * @param key C-string key to remove.
     */
    void Remove(const char* key) { _map.erase(key); }

    /**
     * @brief Returns a reference to the value for the given string key, inserting a default if absent.
     * @param key String key.
     */
    std::any& operator[](const std::string& key) { return _map[key]; }

    /**
     * @brief Returns a const reference to the value for the given string key.
     * @throws std::out_of_range if the key is absent.
     */
    const std::any& at(const std::string& key) const { return _map.at(key); }

    /** @brief Returns a vector of all string keys in the table. */
    std::vector<std::string> getKeys() const {
        std::vector<std::string> keys;
        keys.reserve(_map.size());
        for (const auto& [k, v] : _map) keys.push_back(k);
        return keys;
    }

    /** @brief Returns a vector of all values in the table. */
    std::vector<std::any> getValues() const {
        std::vector<std::any> vals;
        vals.reserve(_map.size());
        for (const auto& [k, v] : _map) vals.push_back(v);
        return vals;
    }

    /**
     * @brief Returns an enumerator for the Hashtable; stub returns nullptr.
     * C++ counterpart of .NET Hashtable.GetEnumerator().
     */
    IDictionaryEnumerator* GetEnumerator() override { return nullptr; }

private:
    std::unordered_map<std::string, std::any> _map;

    static std::string toKey(const void* key) {
        return std::to_string(reinterpret_cast<uintptr_t>(key));
    }
};

} // namespace System::Collections
