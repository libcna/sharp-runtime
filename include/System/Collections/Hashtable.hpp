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
    Hashtable() = default;
    explicit Hashtable(int /*capacity*/) {}

    [[nodiscard]] int getCountProperty() const override {
        return static_cast<int>(_map.size());
    }
    [[nodiscard]] bool getIsReadOnlyProperty() const override { return false; }
    [[nodiscard]] bool getIsFixedSizeProperty() const override { return false; }
    [[nodiscard]] bool getIsSynchronizedProperty() const override { return false; }

    void Add(void* key, void* value) override {
        std::string k = std::to_string(reinterpret_cast<uintptr_t>(key));
        if (_map.count(k)) throw std::invalid_argument("key already exists");
        _map[k] = std::any(value);
    }

    void Add(const std::string& key, const std::any& value) {
        if (_map.count(key)) throw std::invalid_argument("key already exists");
        _map[key] = value;
    }

    void Clear() override { _map.clear(); }

    bool Contains(void* /*key*/) const override { return false; }
    bool ContainsKey(const std::string& key) const { return _map.count(key) > 0; }
    bool ContainsValue(const std::any& value) const {
        for (const auto& [k, v] : _map)
            if (v.type() == value.type()) return true;
        return false;
    }

    std::any& operator[](const std::string& key) { return _map[key]; }
    const std::any& at(const std::string& key) const { return _map.at(key); }

    void Remove(void* /*key*/) override {}
    void Remove(const std::string& key) { _map.erase(key); }

    std::vector<std::string> getKeys() const {
        std::vector<std::string> keys;
        keys.reserve(_map.size());
        for (const auto& [k, v] : _map) keys.push_back(k);
        return keys;
    }

    std::vector<std::any> getValues() const {
        std::vector<std::any> vals;
        vals.reserve(_map.size());
        for (const auto& [k, v] : _map) vals.push_back(v);
        return vals;
    }

    IEnumerator* GetEnumerator() override { return nullptr; }

private:
    std::unordered_map<std::string, std::any> _map;
};

} // namespace System::Collections
