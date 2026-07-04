// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <algorithm>
#include <functional>
#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/IndexOutOfRangeException.hpp"

namespace System::Collections::Immutable {

using SharpRuntime::intcs;

/**
 * @brief Represents an immutable array with value semantics and structural-sharing mutations.
 *
 * C++ counterpart of .NET System.Collections.Immutable.ImmutableArray<T>.
 * Internally shares the underlying std::vector via shared_ptr; mutations return new instances.
 *
 * @tparam T The type of elements stored in the array.
 */
template<typename T>
class ImmutableArray {
    std::shared_ptr<const std::vector<T>> data_;

    explicit ImmutableArray(std::shared_ptr<const std::vector<T>> data) : data_(std::move(data)) {}

public:
    /** @brief Default-constructs an empty ImmutableArray. */
    ImmutableArray() : data_(std::make_shared<std::vector<T>>()) {}

    /**
     * @brief Returns an empty ImmutableArray.
     *
     * C++ counterpart of .NET ImmutableArray<T>.Empty.
     * @return An empty ImmutableArray<T>.
     */
    static ImmutableArray<T> Empty() { return ImmutableArray<T>(); }

    /**
     * @brief Creates an ImmutableArray from an initializer list.
     *
     * C++ counterpart of .NET ImmutableArray.Create<T>(params T[] items).
     * @param items The elements to include.
     * @return A new ImmutableArray containing the given elements.
     */
    static ImmutableArray<T> Create(std::initializer_list<T> items) {
        return ImmutableArray<T>(std::make_shared<std::vector<T>>(items));
    }

    /**
     * @brief Creates an ImmutableArray from an existing vector.
     *
     * C++ counterpart of .NET ImmutableArray.CreateRange<T>(IEnumerable<T>).
     * @param items The elements to include.
     * @return A new ImmutableArray containing the given elements.
     */
    static ImmutableArray<T> Create(const std::vector<T>& items) {
        return ImmutableArray<T>(std::make_shared<std::vector<T>>(items));
    }

    /**
     * @brief Gets the number of elements in the array.
     *
     * C++ counterpart of .NET ImmutableArray<T>.Length.
     * @return The number of elements.
     */
    [[nodiscard]] intcs getLengthProperty() const { return static_cast<intcs>(data_->size()); }

    /**
     * @brief Gets a value indicating whether the array is empty.
     *
     * C++ counterpart of .NET ImmutableArray<T>.IsEmpty.
     * @return true if the array contains no elements; otherwise false.
     */
    [[nodiscard]] bool getIsEmptyProperty() const { return data_->empty(); }

    /**
     * @brief Gets a value indicating whether this array was created with the default constructor.
     *
     * C++ counterpart of .NET ImmutableArray<T>.IsDefault.
     * @return true if the array has not been initialized; otherwise false.
     */
    [[nodiscard]] bool getIsDefaultProperty() const { return !data_; }

    /**
     * @brief Returns a const reference to the element at the given zero-based index.
     *
     * C++ counterpart of .NET ImmutableArray<T>.Item[int].
     * @param index The zero-based index.
     * @return A const reference to the element.
     * @throws System::IndexOutOfRangeException if index is out of range (matches .NET's real
     *         ImmutableArray<T> indexer, which is a thin wrapper over the raw backing array).
     */
    const T& operator[](intcs index) const {
        if (index < 0 || index >= getLengthProperty())
            throw System::IndexOutOfRangeException();
        return (*data_)[static_cast<size_t>(index)];
    }

    /**
     * @brief Returns a new array with @p item appended at the end.
     *
     * C++ counterpart of .NET ImmutableArray<T>.Add(T).
     * @param item The element to append.
     * @return A new ImmutableArray with the element added.
     */
    [[nodiscard]] ImmutableArray<T> Add(const T& item) const {
        auto v = std::make_shared<std::vector<T>>(*data_);
        v->push_back(item);
        return ImmutableArray<T>(std::move(v));
    }

    /**
     * @brief Returns a new array with all elements from @p items appended.
     *
     * C++ counterpart of .NET ImmutableArray<T>.AddRange(IEnumerable<T>).
     * @param items The elements to append.
     * @return A new ImmutableArray with the elements added.
     */
    [[nodiscard]] ImmutableArray<T> AddRange(const std::vector<T>& items) const {
        auto v = std::make_shared<std::vector<T>>(*data_);
        for (const auto& i : items) v->push_back(i);
        return ImmutableArray<T>(std::move(v));
    }

    /**
     * @brief Returns a new array with @p item inserted at @p index.
     *
     * C++ counterpart of .NET ImmutableArray<T>.Insert(int, T).
     * @param index The zero-based index at which to insert.
     * @param item  The element to insert.
     * @return A new ImmutableArray with the element inserted.
     */
    [[nodiscard]] ImmutableArray<T> Insert(intcs index, const T& item) const {
        if (index < 0 || index > getLengthProperty())
            throw System::ArgumentOutOfRangeException("index", "Index must be within the bounds of the List.");
        auto v = std::make_shared<std::vector<T>>(*data_);
        v->insert(v->begin() + index, item);
        return ImmutableArray<T>(std::move(v));
    }

    /**
     * @brief Returns a new array with the elements from @p items inserted at @p index.
     *
     * C++ counterpart of .NET ImmutableArray<T>.InsertRange(int, IEnumerable<T>).
     * @param index The zero-based index at which to insert.
     * @param items The elements to insert.
     * @return A new ImmutableArray with the elements inserted.
     */
    [[nodiscard]] ImmutableArray<T> InsertRange(intcs index, const std::vector<T>& items) const {
        if (index < 0 || index > getLengthProperty())
            throw System::ArgumentOutOfRangeException("index", "Index must be within the bounds of the List.");
        auto v = std::make_shared<std::vector<T>>(*data_);
        v->insert(v->begin() + index, items.begin(), items.end());
        return ImmutableArray<T>(std::move(v));
    }

    /**
     * @brief Returns a new array with the element at @p index replaced by @p item.
     *
     * C++ counterpart of .NET ImmutableArray<T>.SetItem(int, T).
     * @param index The zero-based index of the element to replace.
     * @param item  The new value.
     * @return A new ImmutableArray with the replacement applied.
     */
    [[nodiscard]] ImmutableArray<T> SetItem(intcs index, const T& item) const {
        if (index < 0 || index >= getLengthProperty())
            throw System::IndexOutOfRangeException();
        auto v = std::make_shared<std::vector<T>>(*data_);
        (*v)[static_cast<size_t>(index)] = item;
        return ImmutableArray<T>(std::move(v));
    }

    /**
     * @brief Returns a new array with the first occurrence of @p oldValue replaced by @p newValue.
     *
     * C++ counterpart of .NET ImmutableArray<T>.Replace(T, T).
     * @param oldValue The value to replace.
     * @param newValue The replacement value.
     * @return A new ImmutableArray with the first occurrence replaced.
     */
    [[nodiscard]] ImmutableArray<T> Replace(const T& oldValue, const T& newValue) const {
        auto v = std::make_shared<std::vector<T>>(*data_);
        auto it = std::find(v->begin(), v->end(), oldValue);
        if (it != v->end()) *it = newValue;
        return ImmutableArray<T>(std::move(v));
    }

    /**
     * @brief Returns a new array with the first occurrence of @p item removed.
     *
     * C++ counterpart of .NET ImmutableArray<T>.Remove(T).
     * @param item The element to remove.
     * @return A new ImmutableArray with the first matching element removed.
     */
    [[nodiscard]] ImmutableArray<T> Remove(const T& item) const {
        auto v = std::make_shared<std::vector<T>>();
        bool removed = false;
        for (const auto& x : *data_) {
            if (!removed && x == item) { removed = true; continue; }
            v->push_back(x);
        }
        return ImmutableArray<T>(std::move(v));
    }

    /**
     * @brief Returns a new array with the element at @p index removed.
     *
     * C++ counterpart of .NET ImmutableArray<T>.RemoveAt(int).
     * @param index The zero-based index of the element to remove.
     * @return A new ImmutableArray with the element removed.
     */
    [[nodiscard]] ImmutableArray<T> RemoveAt(intcs index) const {
        if (index < 0 || index >= getLengthProperty())
            throw System::IndexOutOfRangeException();
        auto v = std::make_shared<std::vector<T>>(*data_);
        v->erase(v->begin() + index);
        return ImmutableArray<T>(std::move(v));
    }

    /**
     * @brief Returns a new array with all elements matching @p match removed.
     *
     * C++ counterpart of .NET ImmutableArray<T>.RemoveAll(Predicate<T>).
     * @param match The predicate that identifies elements to remove.
     * @return A new ImmutableArray with all matching elements removed.
     */
    [[nodiscard]] ImmutableArray<T> RemoveAll(std::function<bool(const T&)> match) const {
        auto v = std::make_shared<std::vector<T>>();
        for (const auto& x : *data_) if (!match(x)) v->push_back(x);
        return ImmutableArray<T>(std::move(v));
    }

    /**
     * @brief Returns a new array with elements sorted in ascending order.
     *
     * C++ counterpart of .NET ImmutableArray<T>.Sort().
     * @return A new ImmutableArray sorted in ascending order.
     */
    [[nodiscard]] ImmutableArray<T> Sort() const {
        auto v = std::make_shared<std::vector<T>>(*data_);
        std::sort(v->begin(), v->end());
        return ImmutableArray<T>(std::move(v));
    }

    /**
     * @brief Determines whether the array contains the specified element.
     *
     * C++ counterpart of .NET ImmutableArray<T>.Contains(T).
     * @param item The element to locate.
     * @return true if found; otherwise false.
     */
    [[nodiscard]] bool Contains(const T& item) const {
        return std::find(data_->begin(), data_->end(), item) != data_->end();
    }

    /**
     * @brief Returns the zero-based index of the first occurrence of @p item, or -1 if not found.
     *
     * C++ counterpart of .NET ImmutableArray<T>.IndexOf(T).
     * @param item The element to locate.
     * @return The zero-based index, or -1 if not found.
     */
    [[nodiscard]] intcs IndexOf(const T& item) const {
        auto it = std::find(data_->begin(), data_->end(), item);
        return it == data_->end() ? -1 : static_cast<intcs>(it - data_->begin());
    }

    /**
     * @brief Returns the zero-based index of the last occurrence of @p item, or -1 if not found.
     *
     * C++ counterpart of .NET ImmutableArray<T>.LastIndexOf(T).
     * @param item The element to locate.
     * @return The zero-based index of the last occurrence, or -1 if not found.
     */
    [[nodiscard]] intcs LastIndexOf(const T& item) const {
        for (intcs i = getLengthProperty() - 1; i >= 0; --i)
            if ((*data_)[static_cast<size_t>(i)] == item) return i;
        return -1;
    }

    /**
     * @brief Copies the array elements to a new std::vector.
     *
     * C++ counterpart of .NET ImmutableArray<T>.ToArray() (returns vector here).
     * @return A std::vector<T> containing all elements.
     */
    [[nodiscard]] std::vector<T> ToVector() const { return *data_; }

    /** @brief Returns a const iterator to the beginning of the array (STL interop). */
    auto begin() const { return data_->begin(); }
    /** @brief Returns a const iterator past the end of the array (STL interop). */
    auto end()   const { return data_->end(); }

    /**
     * @brief Returns true if both arrays contain the same elements in the same order.
     * @param o The other array to compare.
     * @return true if equal; otherwise false.
     */
    bool operator==(const ImmutableArray<T>& o) const {
        return data_ == o.data_ || *data_ == *o.data_;
    }
    /**
     * @brief Returns true if the arrays are not equal.
     * @param o The other array to compare.
     * @return true if not equal; otherwise false.
     */
    bool operator!=(const ImmutableArray<T>& o) const { return !(*this == o); }
};

} // namespace System::Collections::Immutable
