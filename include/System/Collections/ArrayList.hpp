// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <algorithm>
#include <any>
#include <stdexcept>
#include <vector>
#include "System/Collections/IList.hpp"
#include "System/Collections/IComparer.hpp"
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::Collections {

using SharpRuntime::bytecs;
using SharpRuntime::intcs;

/**
 * @brief Implements the IList interface using an array whose size is dynamically increased.
 *
 * C++ counterpart of .NET System.Collections.ArrayList.
 * Elements are stored as std::any to approximate .NET's object storage.
 * IComparer-based methods pass const std::any* pointers to the comparer.
 */
class ArrayList : public IList {
public:
    // -----------------------------------------------------------------------
    // Constructors
    // -----------------------------------------------------------------------

    /** @brief Constructs an empty ArrayList with default capacity. */
    ArrayList() = default;

    /**
     * @brief Constructs an ArrayList with the specified initial capacity.
     * @param capacity Initial reserved capacity.
     */
    explicit ArrayList(int capacity) { _items.reserve(static_cast<size_t>(capacity)); }

    /**
     * @brief Constructs an ArrayList by copying all elements from @p c via GetEnumerator().
     *
     * C++ counterpart of .NET ArrayList(ICollection).
     * Each element is stored as void* (the raw pointer returned by IEnumerator::getCurrent()).
     */
    explicit ArrayList(ICollection& c) {
        IEnumerator* e = c.GetEnumerator();
        if (e) {
            while (e->MoveNext())
                _items.emplace_back(e->getCurrent());
            delete e;
        }
    }

    // -----------------------------------------------------------------------
    // Properties
    // -----------------------------------------------------------------------

    /**
     * @brief Returns the number of elements in the list.
     *
     * C++ counterpart of .NET ArrayList.Count.
     */
    [[nodiscard]] int getCountProperty() const override { return static_cast<int>(_items.size()); }

    /**
     * @brief Copies the elements of the list into the given buffer starting at @p index.
     *
     * C++ counterpart of .NET ArrayList.CopyTo(Array, int).
     * @param array Pointer to a std::any[] destination buffer.
     * @param index Zero-based index at which copying begins.
     */
    void CopyTo(void* array, int index) override {
        auto* dest = static_cast<std::any*>(array);
        for (size_t i = 0; i < _items.size(); ++i)
            dest[static_cast<size_t>(index) + i] = _items[i];
    }

    /**
     * @brief Returns the currently allocated capacity of the internal storage.
     *
     * C++ counterpart of .NET ArrayList.Capacity.
     */
    [[nodiscard]] int getCapacityProperty() const { return static_cast<int>(_items.capacity()); }

    /**
     * @brief Sets the capacity, reserving at least @p value elements in the internal storage.
     *
     * C++ counterpart of .NET ArrayList.Capacity setter.
     */
    void setCapacityProperty(int value) { _items.reserve(static_cast<size_t>(value)); }

    /**
     * @brief Returns false; ArrayList is never read-only.
     *
     * C++ counterpart of .NET ArrayList.IsReadOnly.
     */
    [[nodiscard]] bool getIsReadOnlyProperty() const override { return false; }

    /**
     * @brief Returns false; ArrayList is never fixed-size.
     *
     * C++ counterpart of .NET ArrayList.IsFixedSize.
     */
    [[nodiscard]] bool getIsFixedSizeProperty() const override { return false; }

    /**
     * @brief Returns false; ArrayList is not thread-safe.
     *
     * C++ counterpart of .NET ArrayList.IsSynchronized.
     */
    [[nodiscard]] bool getIsSynchronizedProperty() const override { return false; }

    /**
     * @brief Gets an object that can be used to synchronize access to the ArrayList.
     *
     * C++ counterpart of .NET ArrayList.SyncRoot.
     */
    [[nodiscard]] const void* getSyncRootProperty() const override { return this; }

    // -----------------------------------------------------------------------
    // Element access
    // -----------------------------------------------------------------------

    /**
     * @brief Returns a pointer to the element at the given index.
     *
     * C++ counterpart of .NET IList indexer getter (this[int index]).
     * @param index Zero-based index.
     * @return Pointer to the std::any element at @p index.
     */
    [[nodiscard]] void* getItem(intcs index) const override {
        return const_cast<std::any*>(&_items.at(static_cast<size_t>(index)));
    }

    /**
     * @brief Sets the element at the given index.
     *
     * C++ counterpart of .NET IList indexer setter (this[int index] = value).
     * @param index Zero-based index.
     * @param value Pointer to a std::any to store; if null, stores an empty std::any.
     */
    void setItem(intcs index, void* value) override {
        _items.at(static_cast<size_t>(index)) = value ? *static_cast<std::any*>(value) : std::any{};
    }

    /**
     * @brief Returns a reference to the element at the given index.
     * @param index Zero-based index.
     */
    std::any& operator[](int index) { return _items.at(static_cast<size_t>(index)); }

    /**
     * @brief Returns a const reference to the element at the given index.
     * @param index Zero-based index.
     */
    const std::any& operator[](int index) const { return _items.at(static_cast<size_t>(index)); }

    // -----------------------------------------------------------------------
    // Add
    // -----------------------------------------------------------------------

    /**
     * @brief Adds a raw pointer value to the end of the list and returns its new index.
     *
     * C++ counterpart of .NET ArrayList.Add(object).
     */
    SharpRuntime::intcs Add(void* value) override {
        _items.emplace_back(value);
        return static_cast<SharpRuntime::intcs>(_items.size()) - 1;
    }

    /**
     * @brief Adds a typed value to the end of the list and returns its new index.
     * @param value Value to add.
     * @return Zero-based index at which the value was inserted.
     */
    int Add(const std::any& value) { _items.push_back(value); return static_cast<int>(_items.size()) - 1; }

    /**
     * @brief Appends all elements from @p c to the end of the list.
     *
     * C++ counterpart of .NET ArrayList.AddRange(ICollection).
     */
    void AddRange(const std::vector<std::any>& c) {
        _items.insert(_items.end(), c.begin(), c.end());
    }

    // -----------------------------------------------------------------------
    // Clear / Remove
    // -----------------------------------------------------------------------

    /**
     * @brief Removes all elements from the list.
     *
     * C++ counterpart of .NET ArrayList.Clear().
     */
    void Clear() override { _items.clear(); }

    /**
     * @brief Removes the first occurrence of the raw pointer from the list.
     *
     * C++ counterpart of .NET ArrayList.Remove(object).
     */
    void Remove(void* value) override {
        int idx = IndexOf(value);
        if (idx >= 0) RemoveAt(idx);
    }

    /**
     * @brief Removes the element at the specified index.
     *
     * C++ counterpart of .NET ArrayList.RemoveAt(int).
     * @param index Zero-based index of the element to remove.
     */
    void RemoveAt(SharpRuntime::intcs index) override {
        if (index < 0 || index >= static_cast<int>(_items.size())) throw std::out_of_range("index");
        _items.erase(_items.begin() + index);
    }

    /**
     * @brief Removes @p count elements starting at @p index.
     *
     * C++ counterpart of .NET ArrayList.RemoveRange(int, int).
     */
    void RemoveRange(int index, int count) {
        _items.erase(_items.begin() + index, _items.begin() + index + count);
    }

    // -----------------------------------------------------------------------
    // Contains / IndexOf / LastIndexOf
    // -----------------------------------------------------------------------

    /**
     * @brief Returns true if the list contains a raw pointer equal to @p value.
     *
     * C++ counterpart of .NET ArrayList.Contains(object) for void* elements.
     */
    bool Contains(void* value) const override {
        for (const auto& item : _items)
            if (std::any_cast<void*>(&item) && std::any_cast<void*>(item) == value) return true;
        return false;
    }

    /**
     * @brief Returns true if the list contains an element whose type matches @p value.
     */
    bool Contains(const std::any& value) const {
        for (const auto& item : _items)
            if (item.type() == value.type()) return true;
        return false;
    }

    /**
     * @brief Returns the zero-based index of the first occurrence of the raw pointer, or -1.
     *
     * C++ counterpart of .NET ArrayList.IndexOf(object).
     */
    SharpRuntime::intcs IndexOf(void* value) const override {
        for (int i = 0; i < static_cast<int>(_items.size()); ++i)
            if (std::any_cast<void*>(&_items[i]) && std::any_cast<void*>(_items[i]) == value) return i;
        return -1;
    }

    /**
     * @brief Returns the zero-based index of the first type-matching element, or -1.
     *
     * C++ counterpart of .NET ArrayList.IndexOf(object).
     */
    int IndexOf(const std::any& value) const {
        for (int i = 0; i < static_cast<int>(_items.size()); ++i)
            if (_items[i].type() == value.type()) return i;
        return -1;
    }

    /**
     * @brief Returns the first type-matching element at or after @p startIndex, or -1.
     *
     * C++ counterpart of .NET ArrayList.IndexOf(object, int).
     */
    int IndexOf(const std::any& value, int startIndex) const {
        for (int i = startIndex; i < static_cast<int>(_items.size()); ++i)
            if (_items[i].type() == value.type()) return i;
        return -1;
    }

    /**
     * @brief Returns the first type-matching element in [@p startIndex, @p startIndex+@p count), or -1.
     *
     * C++ counterpart of .NET ArrayList.IndexOf(object, int, int).
     */
    int IndexOf(const std::any& value, int startIndex, int count) const {
        int end = startIndex + count;
        for (int i = startIndex; i < end && i < static_cast<int>(_items.size()); ++i)
            if (_items[i].type() == value.type()) return i;
        return -1;
    }

    /**
     * @brief Returns the zero-based index of the last type-matching element, or -1.
     *
     * C++ counterpart of .NET ArrayList.LastIndexOf(object).
     */
    int LastIndexOf(const std::any& value) const {
        for (int i = static_cast<int>(_items.size()) - 1; i >= 0; --i)
            if (_items[i].type() == value.type()) return i;
        return -1;
    }

    /**
     * @brief Searches backward from @p startIndex for the last type-matching element, or -1.
     *
     * C++ counterpart of .NET ArrayList.LastIndexOf(object, int).
     */
    int LastIndexOf(const std::any& value, int startIndex) const {
        for (int i = startIndex; i >= 0; --i)
            if (_items[i].type() == value.type()) return i;
        return -1;
    }

    /**
     * @brief Searches backward in [@p startIndex-@p count+1, @p startIndex] for the last type-matching element.
     *
     * C++ counterpart of .NET ArrayList.LastIndexOf(object, int, int).
     */
    int LastIndexOf(const std::any& value, int startIndex, int count) const {
        int lo = startIndex - count + 1;
        for (int i = startIndex; i >= lo && i >= 0; --i)
            if (_items[i].type() == value.type()) return i;
        return -1;
    }

    // -----------------------------------------------------------------------
    // Insert
    // -----------------------------------------------------------------------

    /**
     * @brief Inserts a raw pointer at the specified index.
     *
     * C++ counterpart of .NET ArrayList.Insert(int, object).
     */
    void Insert(SharpRuntime::intcs index, void* value) override {
        _items.insert(_items.begin() + index, value);
    }

    /**
     * @brief Inserts a typed value at the specified index.
     */
    void Insert(int index, const std::any& value) {
        _items.insert(_items.begin() + index, value);
    }

    /**
     * @brief Inserts all elements of @p c starting at the specified index.
     *
     * C++ counterpart of .NET ArrayList.InsertRange(int, ICollection).
     */
    void InsertRange(int index, const std::vector<std::any>& c) {
        _items.insert(_items.begin() + index, c.begin(), c.end());
    }

    // -----------------------------------------------------------------------
    // Reverse / SetRange / GetRange
    // -----------------------------------------------------------------------

    /**
     * @brief Reverses the order of all elements in the list.
     *
     * C++ counterpart of .NET ArrayList.Reverse().
     */
    void Reverse() { std::reverse(_items.begin(), _items.end()); }

    /**
     * @brief Reverses the order of @p count elements starting at @p index.
     *
     * C++ counterpart of .NET ArrayList.Reverse(int, int).
     */
    void Reverse(int index, int count) {
        std::reverse(_items.begin() + index, _items.begin() + index + count);
    }

    /**
     * @brief Copies elements from @p c over the list starting at @p index.
     *
     * C++ counterpart of .NET ArrayList.SetRange(int, ICollection).
     */
    void SetRange(int index, const std::vector<std::any>& c) {
        for (size_t i = 0; i < c.size(); ++i)
            _items[static_cast<size_t>(index) + i] = c[i];
    }

    /**
     * @brief Returns a new ArrayList containing @p count elements starting at @p index.
     *
     * C++ counterpart of .NET ArrayList.GetRange(int, int).
     */
    [[nodiscard]] ArrayList GetRange(int index, int count) const {
        ArrayList result;
        result._items.insert(result._items.end(),
                             _items.begin() + index,
                             _items.begin() + index + count);
        return result;
    }

    // -----------------------------------------------------------------------
    // Sort
    // -----------------------------------------------------------------------

    /**
     * @brief Sorts all elements using the specified comparer.
     *
     * C++ counterpart of .NET ArrayList.Sort(IComparer).
     * The comparer receives const void* pointing to std::any elements.
     */
    void Sort(const IComparer& comparer) {
        std::stable_sort(_items.begin(), _items.end(),
            [&comparer](const std::any& a, const std::any& b) {
                return comparer.Compare(&a, &b) < 0;
            });
    }

    /**
     * @brief Sorts @p count elements starting at @p index using the specified comparer.
     *
     * C++ counterpart of .NET ArrayList.Sort(int, int, IComparer).
     */
    void Sort(int index, int count, const IComparer& comparer) {
        std::stable_sort(_items.begin() + index, _items.begin() + index + count,
            [&comparer](const std::any& a, const std::any& b) {
                return comparer.Compare(&a, &b) < 0;
            });
    }

    // -----------------------------------------------------------------------
    // BinarySearch
    // -----------------------------------------------------------------------

    /**
     * @brief Binary-searches the entire list for @p value using the specified comparer.
     *
     * C++ counterpart of .NET ArrayList.BinarySearch(object, IComparer).
     * @return Index of the found element, or bitwise complement of insertion point if not found.
     */
    [[nodiscard]] int BinarySearch(const std::any& value, const IComparer& comparer) const {
        return BinarySearch(0, static_cast<int>(_items.size()), value, comparer);
    }

    /**
     * @brief Binary-searches [@p index, @p index+@p count) for @p value using @p comparer.
     *
     * C++ counterpart of .NET ArrayList.BinarySearch(int, int, object, IComparer).
     * @return Index of the found element, or bitwise complement of insertion point if not found.
     */
    [[nodiscard]] int BinarySearch(int index, int count, const std::any& value, const IComparer& comparer) const {
        int lo = index, hi = index + count - 1;
        while (lo <= hi) {
            int mid = lo + (hi - lo) / 2;
            int cmp = comparer.Compare(&_items[static_cast<size_t>(mid)], &value);
            if (cmp == 0) return mid;
            if (cmp < 0) lo = mid + 1;
            else         hi = mid - 1;
        }
        return ~lo;
    }

    // -----------------------------------------------------------------------
    // Clone / ToArray / TrimToSize
    // -----------------------------------------------------------------------

    /**
     * @brief Returns a shallow copy of this ArrayList.
     *
     * C++ counterpart of .NET ArrayList.Clone().
     */
    [[nodiscard]] ArrayList Clone() const {
        ArrayList copy;
        copy._items = _items;
        return copy;
    }

    /**
     * @brief Returns a copy of the internal items as a std::vector.
     *
     * C++ counterpart of .NET ArrayList.ToArray().
     */
    std::vector<std::any> ToArray() const { return _items; }

    /**
     * @brief Releases any excess capacity, shrinking internal storage to fit the current count.
     *
     * C++ counterpart of .NET ArrayList.TrimToSize().
     */
    void TrimToSize() { _items.shrink_to_fit(); }

    // -----------------------------------------------------------------------
    // Static factories
    // -----------------------------------------------------------------------

    /**
     * @brief Returns an ArrayList with @p count copies of @p value.
     *
     * C++ counterpart of .NET ArrayList.Repeat(object, int).
     */
    [[nodiscard]] static ArrayList Repeat(const std::any& value, int count) {
        ArrayList result(count);
        for (int i = 0; i < count; ++i) result.Add(value);
        return result;
    }

    // -----------------------------------------------------------------------
    // Misc
    // -----------------------------------------------------------------------

    /** @brief Returns a const reference to the underlying item vector. */
    [[nodiscard]] const std::vector<std::any>& getItems() const { return _items; }

    /**
     * @brief Returns an enumerator over the full list; not implemented — returns nullptr.
     *
     * C++ counterpart of .NET ArrayList.GetEnumerator().
     */
    IEnumerator* GetEnumerator() override { return nullptr; }

    /**
     * @brief Returns an enumerator over a range; not implemented — returns nullptr.
     *
     * C++ counterpart of .NET ArrayList.GetEnumerator(int, int).
     */
    IEnumerator* GetEnumerator(int /*index*/, int /*count*/) { return nullptr; }

private:
    std::vector<std::any> _items;
};

} // namespace System::Collections
