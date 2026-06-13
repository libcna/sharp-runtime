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

/// <summary>Represents a collection of key/value pairs organized based on the hash code of the key.</summary>
class Hashtable : public IDictionary {
public:
    /// Constructs an empty Hashtable.
    Hashtable() = default;
    /// Constructs a Hashtable; the capacity hint is accepted but ignored in this port.
    explicit Hashtable(int /*capacity*/) {}

    /// Returns the number of key/value pairs in the table.
    [[nodiscard]] int getCountProperty() const override {
        return static_cast<int>(_map.size());
    }
    /// Returns false; Hashtable is never read-only.
    [[nodiscard]] bool getIsReadOnlyProperty() const override { return false; }
    /// Returns false; Hashtable is never fixed-size.
    [[nodiscard]] bool getIsFixedSizeProperty() const override { return false; }
    /// Returns false; Hashtable is not thread-safe.
    [[nodiscard]] bool getIsSynchronizedProperty() const override { return false; }

    /// Adds a raw-pointer key/value pair; throws if the key already exists.
    void Add(void* key, void* value) override {
        std::string k = std::to_string(reinterpret_cast<uintptr_t>(key));
        if (_map.count(k)) throw std::invalid_argument("key already exists");
        _map[k] = std::any(value);
    }

    /// Adds a string key with a typed value; throws if the key already exists.
    /// @param key String key.
    /// @param value Value to store.
    void Add(const std::string& key, const std::any& value) {
        if (_map.count(key)) throw std::invalid_argument("key already exists");
        _map[key] = value;
    }

    /// Removes all key/value pairs from the table.
    void Clear() override { _map.clear(); }

    /// Raw-pointer key lookup — not implemented, always returns false.
    bool Contains(void* /*key*/) const override { return false; }
    /// Returns true if the table contains the specified string key.
    /// @param key String key to look up.
    bool ContainsKey(const std::string& key) const { return _map.count(key) > 0; }
    /// Returns true if any stored value has the same type as @p value.
    bool ContainsValue(const std::any& value) const {
        for (const auto& [k, v] : _map)
            if (v.type() == value.type()) return true;
        return false;
    }

    /// Returns a reference to the value for the given key, inserting a default if absent.
    /// @param key String key.
    std::any& operator[](const std::string& key) { return _map[key]; }
    /// Returns a const reference to the value for the given key; throws if absent.
    /// @param key String key.
    const std::any& at(const std::string& key) const { return _map.at(key); }

    /// Raw-pointer key removal — not implemented (no-op).
    void Remove(void* /*key*/) override {}
    /// Removes the entry with the specified string key.
    /// @param key String key to remove.
    void Remove(const std::string& key) { _map.erase(key); }

    /// Returns a vector of all keys in the table.
    std::vector<std::string> getKeys() const {
        std::vector<std::string> keys;
        keys.reserve(_map.size());
        for (const auto& [k, v] : _map) keys.push_back(k);
        return keys;
    }

    /// Returns a vector of all values in the table.
    std::vector<std::any> getValues() const {
        std::vector<std::any> vals;
        vals.reserve(_map.size());
        for (const auto& [k, v] : _map) vals.push_back(v);
        return vals;
    }

    /// Returns an enumerator; not implemented — returns nullptr.
    IEnumerator* GetEnumerator() override { return nullptr; }

private:
    std::unordered_map<std::string, std::any> _map;
};

} // namespace System::Collections
