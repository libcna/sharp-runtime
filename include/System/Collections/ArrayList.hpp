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
    /// Constructs an empty ArrayList with default capacity.
    ArrayList() = default;

    /// Constructs an ArrayList with the specified initial capacity.
    /// @param capacity Initial reserved capacity.
    explicit ArrayList(int capacity) { _items.reserve(static_cast<size_t>(capacity)); }

    /// Returns the number of elements in the list.
    [[nodiscard]] int getCountProperty() const override { return static_cast<int>(_items.size()); }
    /// Returns the currently allocated capacity of the internal storage.
    [[nodiscard]] int getCapacityProperty() const { return static_cast<int>(_items.capacity()); }
    /// Sets the capacity, reserving at least @p value elements in the internal storage.
    void setCapacityProperty(int value) { _items.reserve(static_cast<size_t>(value)); }
    /// Returns false; ArrayList is never read-only.
    [[nodiscard]] bool getIsReadOnlyProperty() const override { return false; }
    /// Returns false; ArrayList is never fixed-size.
    [[nodiscard]] bool getIsFixedSizeProperty() const override { return false; }
    /// Returns false; ArrayList is not thread-safe.
    [[nodiscard]] bool getIsSynchronizedProperty() const override { return false; }

    /// Returns a reference to the element at the given index.
    /// @param index Zero-based index.
    std::any& operator[](int index) { return _items.at(static_cast<size_t>(index)); }
    /// Returns a const reference to the element at the given index.
    /// @param index Zero-based index.
    const std::any& operator[](int index) const { return _items.at(static_cast<size_t>(index)); }

    /// Adds a raw pointer value to the end of the list; returns its new index.
    SharpRuntime::intcs Add(void* value) override {
        _items.emplace_back(value);
        return static_cast<SharpRuntime::intcs>(_items.size()) - 1;
    }
    /// Adds a typed value to the end of the list and returns its new index.
    /// @param value Value to add.
    /// @return Zero-based index at which the value was inserted.
    int Add(const std::any& value) { _items.push_back(value); return static_cast<int>(_items.size()) - 1; }

    /// Appends all elements from @p c to the end of the list.
    /// @param c Source collection.
    void AddRange(const std::vector<std::any>& c) {
        _items.insert(_items.end(), c.begin(), c.end());
    }

    /// Removes all elements from the list.
    void Clear() override { _items.clear(); }

    /// Returns true if the list contains a raw pointer equal to @p value.
    bool Contains(void* value) const override {
        for (const auto& item : _items)
            if (std::any_cast<void*>(&item) && std::any_cast<void*>(item) == value) return true;
        return false;
    }
    /// Returns true if the list contains an element whose type matches @p value.
    bool Contains(const std::any& value) const {
        for (const auto& item : _items)
            if (item.type() == value.type()) return true;
        return false;
    }

    /// Returns the zero-based index of the first occurrence of the raw pointer, or -1.
    SharpRuntime::intcs IndexOf(void* value) const override {
        for (int i = 0; i < static_cast<int>(_items.size()); ++i)
            if (std::any_cast<void*>(&_items[i]) && std::any_cast<void*>(_items[i]) == value) return i;
        return -1;
    }

    /// Returns the zero-based index of the last element whose type matches @p value, or -1.
    int LastIndexOf(const std::any& value) const {
        for (int i = static_cast<int>(_items.size()) - 1; i >= 0; --i)
            if (_items[i].type() == value.type()) return i;
        return -1;
    }

    /// Inserts a raw pointer at the specified index.
    /// @param index Zero-based insertion position.
    void Insert(SharpRuntime::intcs index, void* value) override {
        _items.insert(_items.begin() + index, value);
    }
    /// Inserts a typed value at the specified index.
    /// @param index Zero-based insertion position.
    /// @param value Value to insert.
    void Insert(int index, const std::any& value) {
        _items.insert(_items.begin() + index, value);
    }

    /// Inserts all elements of @p c starting at the specified index.
    /// @param index Zero-based insertion position.
    /// @param c Source collection.
    void InsertRange(int index, const std::vector<std::any>& c) {
        _items.insert(_items.begin() + index, c.begin(), c.end());
    }

    /// Removes the first occurrence of the raw pointer from the list.
    void Remove(void* value) override {
        int idx = IndexOf(value);
        if (idx >= 0) RemoveAt(idx);
    }

    /// Removes the element at the specified index.
    /// @param index Zero-based index of the element to remove.
    void RemoveAt(SharpRuntime::intcs index) override {
        if (index < 0 || index >= static_cast<int>(_items.size())) throw std::out_of_range("index");
        _items.erase(_items.begin() + index);
    }

    /// Removes @p count elements starting at @p index.
    /// @param index Zero-based start index.
    /// @param count Number of elements to remove.
    void RemoveRange(int index, int count) {
        _items.erase(_items.begin() + index, _items.begin() + index + count);
    }

    /// Reverses the order of all elements in the list.
    void Reverse() { std::reverse(_items.begin(), _items.end()); }
    /// Reverses the order of @p count elements starting at @p index.
    /// @param index Zero-based start index.
    /// @param count Number of elements to reverse.
    void Reverse(int index, int count) {
        std::reverse(_items.begin() + index, _items.begin() + index + count);
    }

    /// Copies elements from @p c over the list starting at @p index.
    /// @param index Zero-based start position.
    /// @param c Source collection whose elements replace existing ones.
    void SetRange(int index, const std::vector<std::any>& c) {
        for (size_t i = 0; i < c.size(); ++i)
            _items[static_cast<size_t>(index) + i] = c[i];
    }

    /// Returns a copy of the internal items as a std::vector.
    std::vector<std::any> ToArray() const { return _items; }

    /// Releases any excess capacity, shrinking internal storage to fit the current count.
    void TrimToSize() { _items.shrink_to_fit(); }

    /// Returns a const reference to the underlying item vector.
    [[nodiscard]] const std::vector<std::any>& getItems() const { return _items; }

    /// Returns an enumerator; not implemented — returns nullptr.
    IEnumerator* GetEnumerator() override { return nullptr; }

private:
    std::vector<std::any> _items;
};

} // namespace System::Collections
