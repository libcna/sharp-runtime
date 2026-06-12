// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <algorithm>
#include <any>
#include <stdexcept>
#include <vector>
#include "System/Collections/IList.hpp"
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::Collections {

using SharpRuntime::bytecs;
using SharpRuntime::intcs;

/// <summary>Implements the IList interface using an array whose size is dynamically increased.</summary>
class ArrayList : public IList {
public:
    ArrayList() = default;

    explicit ArrayList(int capacity) { _items.reserve(static_cast<size_t>(capacity)); }

    [[nodiscard]] int getCountProperty() const override { return static_cast<int>(_items.size()); }
    [[nodiscard]] int getCapacityProperty() const { return static_cast<int>(_items.capacity()); }
    void setCapacityProperty(int value) { _items.reserve(static_cast<size_t>(value)); }
    [[nodiscard]] bool getIsReadOnlyProperty() const override { return false; }
    [[nodiscard]] bool getIsFixedSizeProperty() const override { return false; }
    [[nodiscard]] bool getIsSynchronizedProperty() const override { return false; }

    std::any& operator[](int index) { return _items.at(static_cast<size_t>(index)); }
    const std::any& operator[](int index) const { return _items.at(static_cast<size_t>(index)); }

    void Add(void* value) override { _items.emplace_back(value); }
    int Add(const std::any& value) { _items.push_back(value); return static_cast<int>(_items.size()) - 1; }

    void AddRange(const std::vector<std::any>& c) {
        _items.insert(_items.end(), c.begin(), c.end());
    }

    void Clear() override { _items.clear(); }

    bool Contains(void* value) const override {
        for (const auto& item : _items)
            if (std::any_cast<void*>(&item) && std::any_cast<void*>(item) == value) return true;
        return false;
    }
    bool Contains(const std::any& value) const {
        for (const auto& item : _items)
            if (item.type() == value.type()) return true;
        return false;
    }

    int IndexOf(void* value) const override {
        for (int i = 0; i < static_cast<int>(_items.size()); ++i)
            if (std::any_cast<void*>(&_items[i]) && std::any_cast<void*>(_items[i]) == value) return i;
        return -1;
    }

    int LastIndexOf(const std::any& value) const {
        for (int i = static_cast<int>(_items.size()) - 1; i >= 0; --i)
            if (_items[i].type() == value.type()) return i;
        return -1;
    }

    void Insert(int index, void* value) override {
        _items.insert(_items.begin() + index, value);
    }
    void Insert(int index, const std::any& value) {
        _items.insert(_items.begin() + index, value);
    }

    void InsertRange(int index, const std::vector<std::any>& c) {
        _items.insert(_items.begin() + index, c.begin(), c.end());
    }

    void Remove(void* value) override {
        int idx = IndexOf(value);
        if (idx >= 0) RemoveAt(idx);
    }

    void RemoveAt(int index) override {
        if (index < 0 || index >= static_cast<int>(_items.size())) throw std::out_of_range("index");
        _items.erase(_items.begin() + index);
    }

    void RemoveRange(int index, int count) {
        _items.erase(_items.begin() + index, _items.begin() + index + count);
    }

    void Reverse() { std::reverse(_items.begin(), _items.end()); }
    void Reverse(int index, int count) {
        std::reverse(_items.begin() + index, _items.begin() + index + count);
    }

    void SetRange(int index, const std::vector<std::any>& c) {
        for (size_t i = 0; i < c.size(); ++i)
            _items[static_cast<size_t>(index) + i] = c[i];
    }

    std::vector<std::any> ToArray() const { return _items; }

    void TrimToSize() { _items.shrink_to_fit(); }

    [[nodiscard]] const std::vector<std::any>& getItems() const { return _items; }

    IEnumerator* GetEnumerator() override { return nullptr; }

private:
    std::vector<std::any> _items;
};

} // namespace System::Collections
