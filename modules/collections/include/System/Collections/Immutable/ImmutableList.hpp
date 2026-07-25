// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <algorithm>
#include <functional>
#include <initializer_list>
#include <memory>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentException.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"

namespace System::Collections::Immutable {

using SharpRuntime::intcs;

/**
 * @brief Represents an immutable ordered list of elements.
 *
 * C++ counterpart of .NET System.Collections.Immutable.ImmutableList<T>.
 * Internally shares the underlying std::vector via shared_ptr; mutations return new instances.
 *
 * Covers the core mutation/lookup surface (Add/AddRange/Insert/InsertRange/SetItem/Replace/
 * Remove/RemoveAll/RemoveAt/RemoveRange(int,int)/Sort/Reverse/Contains/IndexOf/LastIndexOf/
 * BinarySearch).
 * Deliberately deferred relative to real .NET's ImmutableList<T> (a much larger surface backed
 * by an AVL tree, not a flat vector): range and custom-comparer Sort/Reverse overloads, the 3
 * CopyTo overloads, ConvertAll<TOutput>, ToBuilder/Builder,
 * RemoveRange(IEnumerable<T>), and every
 * IEqualityComparer<T>/IComparer<T>-taking overload of Remove/RemoveRange/Replace/IndexOf/
 * LastIndexOf/BinarySearch (this port always uses T::operator== / operator< instead). These are
 * real gaps, not incorrect behavior for the surface that does exist -- left undone here rather
 * than expanded ad hoc in a single audit pass; a full port would need an AVL/red-black backing
 * structure to match .NET's O(log n) persistent-update complexity (this port's vector-copy
 * approach is O(n) per mutation).
 *
 * @tparam T The type of elements stored in the list.
 */
template<typename T>
class ImmutableList {
    std::shared_ptr<const std::vector<T>> data_;

    explicit ImmutableList(std::shared_ptr<const std::vector<T>> data) : data_(std::move(data)) {}

    void requireIndexInRange(intcs index) const {
        if (index < 0 || index >= static_cast<intcs>(data_->size()))
            throw System::ArgumentOutOfRangeException("index", "Index was out of range. Must be non-negative and less than the size of the collection.");
    }

    void requireValidRange(intcs index, intcs count) const {
        // Matches real .NET's ImmutableList<T>.RemoveRange(int, int) (Requires.Range calls),
        // which throws ArgumentOutOfRangeException for every violation here -- not
        // ArgumentException for the bounds check, as this previously did.
        if (index < 0 || index > static_cast<intcs>(data_->size()))
            throw System::ArgumentOutOfRangeException("index", "Index was out of range. Must be non-negative and less than or equal to the size of the collection.");
        if (count < 0 || index > static_cast<intcs>(data_->size()) - count)
            throw System::ArgumentOutOfRangeException("count", "Count must refer to a location within the collection.");
    }

    static void requirePredicate(const std::function<bool(const T&)>& predicate) {
        if (!predicate) {
            throw System::ArgumentNullException("match");
        }
    }

public:
    /** @brief Default-constructs an empty ImmutableList. */
    ImmutableList() : data_(std::make_shared<std::vector<T>>()) {}

    /**
     * @brief Returns an empty ImmutableList.
     *
     * C++ counterpart of .NET ImmutableList<T>.Empty.
     * @return An empty ImmutableList<T>.
     */
    static ImmutableList<T> Empty() { return ImmutableList<T>(); }

    /**
     * @brief Creates an ImmutableList from an initializer list.
     *
     * C++ counterpart of .NET ImmutableList.Create<T>(params T[] items).
     * @param items The initial elements.
     * @return A new ImmutableList containing the given elements.
     */
    static ImmutableList<T> Create(std::initializer_list<T> items) {
        return ImmutableList<T>(std::make_shared<std::vector<T>>(items));
    }

    /**
     * @brief Creates an ImmutableList from an existing vector.
     *
     * C++ counterpart of .NET ImmutableList.CreateRange<T>(IEnumerable<T>).
     * @param items The initial elements.
     * @return A new ImmutableList containing the given elements.
     */
    static ImmutableList<T> Create(const std::vector<T>& items) {
        return ImmutableList<T>(std::make_shared<std::vector<T>>(items));
    }

    /**
     * @brief Gets the number of elements in the list.
     *
     * C++ counterpart of .NET ImmutableList<T>.Count.
     * @return The number of elements.
     */
    [[nodiscard]] intcs getCountProperty() const { return static_cast<intcs>(data_->size()); }

    /**
     * @brief Gets a value indicating whether the list is empty.
     *
     * C++ counterpart of .NET ImmutableList<T>.IsEmpty.
     * @return true if the list contains no elements; otherwise false.
     */
    [[nodiscard]] bool getIsEmptyProperty() const { return data_->empty(); }

    /**
     * @brief Returns a const reference to the element at the given zero-based index.
     *
     * C++ counterpart of .NET ImmutableList<T>.Item[int].
     * @param index The zero-based index.
     * @return A const reference to the element.
     */
    const T& operator[](intcs index) const {
        requireIndexInRange(index);
        return (*data_)[static_cast<size_t>(index)];
    }

    /**
     * @brief Returns a new list with @p item appended at the end.
     *
     * C++ counterpart of .NET ImmutableList<T>.Add(T).
     * @param item The element to append.
     * @return A new ImmutableList with the element added.
     */
    [[nodiscard]] ImmutableList<T> Add(const T& item) const {
        auto v = std::make_shared<std::vector<T>>(*data_);
        v->push_back(item);
        return ImmutableList<T>(std::move(v));
    }

    /**
     * @brief Returns a new list with all elements from @p items appended.
     *
     * C++ counterpart of .NET ImmutableList<T>.AddRange(IEnumerable<T>).
     * @param items The elements to append.
     * @return A new ImmutableList with the elements added.
     */
    [[nodiscard]] ImmutableList<T> AddRange(const std::vector<T>& items) const {
        auto v = std::make_shared<std::vector<T>>(*data_);
        for (const auto& i : items) v->push_back(i);
        return ImmutableList<T>(std::move(v));
    }

    /**
     * @brief Returns a new list with @p item inserted at @p index.
     *
     * C++ counterpart of .NET ImmutableList<T>.Insert(int, T).
     * @param index The zero-based index at which to insert.
     * @param item  The element to insert.
     * @return A new ImmutableList with the element inserted.
     */
    [[nodiscard]] ImmutableList<T> Insert(intcs index, const T& item) const {
        if (index < 0 || index > static_cast<intcs>(data_->size()))
            throw System::ArgumentOutOfRangeException("index", "Index must be within the bounds of the List.");
        auto v = std::make_shared<std::vector<T>>(*data_);
        v->insert(v->begin() + index, item);
        return ImmutableList<T>(std::move(v));
    }

    /**
     * @brief Returns a new list with the elements from @p items inserted at @p index.
     *
     * C++ counterpart of .NET ImmutableList<T>.InsertRange(int, IEnumerable<T>).
     * @param index The zero-based index at which to insert.
     * @param items The elements to insert.
     * @return A new ImmutableList with the elements inserted.
     */
    [[nodiscard]] ImmutableList<T> InsertRange(intcs index, const std::vector<T>& items) const {
        if (index < 0 || index > static_cast<intcs>(data_->size()))
            throw System::ArgumentOutOfRangeException("index", "Index must be within the bounds of the List.");
        auto v = std::make_shared<std::vector<T>>(*data_);
        v->insert(v->begin() + index, items.begin(), items.end());
        return ImmutableList<T>(std::move(v));
    }

    /**
     * @brief Returns a new list with the element at @p index replaced by @p item.
     *
     * C++ counterpart of .NET ImmutableList<T>.SetItem(int, T).
     * @param index The zero-based index of the element to replace.
     * @param item  The new value.
     * @return A new ImmutableList with the replacement applied.
     */
    [[nodiscard]] ImmutableList<T> SetItem(intcs index, const T& item) const {
        requireIndexInRange(index);
        auto v = std::make_shared<std::vector<T>>(*data_);
        (*v)[static_cast<size_t>(index)] = item;
        return ImmutableList<T>(std::move(v));
    }

    /**
     * @brief Returns a new list with the first occurrence of @p oldValue replaced by @p newValue.
     *
     * C++ counterpart of .NET ImmutableList<T>.Replace(T, T).
     * @param oldValue The value to replace.
     * @param newValue The replacement value.
     * @return A new ImmutableList with the first occurrence replaced.
     */
    [[nodiscard]] ImmutableList<T> Replace(const T& oldValue, const T& newValue) const {
        auto v = std::make_shared<std::vector<T>>(*data_);
        auto it = std::find(v->begin(), v->end(), oldValue);
        if (it != v->end()) *it = newValue;
        return ImmutableList<T>(std::move(v));
    }

    /**
     * @brief Returns a new list with the first occurrence of @p item removed.
     *
     * C++ counterpart of .NET ImmutableList<T>.Remove(T).
     * @param item The element to remove.
     * @return A new ImmutableList with the first matching element removed.
     */
    [[nodiscard]] ImmutableList<T> Remove(const T& item) const {
        auto v = std::make_shared<std::vector<T>>();
        bool removed = false;
        for (const auto& x : *data_) {
            if (!removed && x == item) { removed = true; continue; }
            v->push_back(x);
        }
        return ImmutableList<T>(std::move(v));
    }

    /**
     * @brief Returns a new list with all elements matching @p match removed.
     *
     * C++ counterpart of .NET ImmutableList<T>.RemoveAll(Predicate<T>).
     * @param match The predicate that identifies elements to remove.
     * @return A new ImmutableList with all matching elements removed.
     */
    [[nodiscard]] ImmutableList<T> RemoveAll(std::function<bool(const T&)> match) const {
        auto v = std::make_shared<std::vector<T>>();
        for (const auto& x : *data_) if (!match(x)) v->push_back(x);
        return ImmutableList<T>(std::move(v));
    }

    /**
     * @brief Returns a new list with the element at @p index removed.
     *
     * C++ counterpart of .NET ImmutableList<T>.RemoveAt(int).
     * @param index The zero-based index of the element to remove.
     * @return A new ImmutableList with the element removed.
     */
    [[nodiscard]] ImmutableList<T> RemoveAt(intcs index) const {
        requireIndexInRange(index);
        auto v = std::make_shared<std::vector<T>>(*data_);
        v->erase(v->begin() + index);
        return ImmutableList<T>(std::move(v));
    }

    /**
     * @brief Returns a new list with @p count elements removed starting at @p index.
     *
     * C++ counterpart of .NET ImmutableList<T>.RemoveRange(int, int).
     * @param index The zero-based starting index.
     * @param count The number of elements to remove.
     * @return A new ImmutableList with the range removed.
     */
    [[nodiscard]] ImmutableList<T> RemoveRange(intcs index, intcs count) const {
        requireValidRange(index, count);
        auto v = std::make_shared<std::vector<T>>(*data_);
        v->erase(v->begin() + index, v->begin() + index + count);
        return ImmutableList<T>(std::move(v));
    }

    /**
     * @brief Returns a new list containing the contiguous range at @p index.
     *
     * C++ counterpart of .NET ImmutableList<T>.GetRange(int, int).
     * @param index The zero-based first element in the range.
     * @param count The number of elements to copy into the result.
     * @return An immutable list containing the requested range.
     * @throws System::ArgumentOutOfRangeException if the range is outside this list.
     */
    [[nodiscard]] ImmutableList<T> GetRange(intcs index, intcs count) const {
        requireValidRange(index, count);
        auto values = std::make_shared<std::vector<T>>(
            data_->begin() + index, data_->begin() + index + count);
        return ImmutableList<T>(std::move(values));
    }

    /**
     * @brief Returns a new list with all elements sorted using T::operator<.
     *
     * C++ counterpart of .NET ImmutableList<T>.Sort(). Range and custom-comparer
     * overloads remain deliberately unimplemented.
     * @return A sorted immutable list; the source list is unchanged.
     */
    [[nodiscard]] ImmutableList<T> Sort() const {
        auto values = std::make_shared<std::vector<T>>(*data_);
        std::sort(values->begin(), values->end());
        return ImmutableList<T>(std::move(values));
    }

    /**
     * @brief Returns a new list whose elements are in reverse order.
     *
     * C++ counterpart of .NET ImmutableList<T>.Reverse(). Range overloads remain
     * deliberately unimplemented.
     * @return A reversed immutable list; the source list is unchanged.
     */
    [[nodiscard]] ImmutableList<T> Reverse() const {
        auto values = std::make_shared<std::vector<T>>(*data_);
        std::reverse(values->begin(), values->end());
        return ImmutableList<T>(std::move(values));
    }

    /**
     * @brief Performs @p action on each element in list order.
     *
     * C++ counterpart of .NET ImmutableList<T>.ForEach(Action<T>).
     * @param action The action to perform.
     * @throws System::ArgumentNullException if @p action is empty.
     */
    void ForEach(std::function<void(const T&)> action) const {
        if (!action) {
            throw System::ArgumentNullException("action");
        }
        for (const auto& item : *data_) {
            action(item);
        }
    }

    /**
     * @brief Determines whether any element matches @p match.
     *
     * C++ counterpart of .NET ImmutableList<T>.Exists(Predicate<T>).
     * @param match The predicate to test.
     * @return true if at least one element matches; otherwise false.
     * @throws System::ArgumentNullException if @p match is empty.
     */
    [[nodiscard]] bool Exists(std::function<bool(const T&)> match) const {
        requirePredicate(match);
        return std::any_of(data_->begin(), data_->end(), match);
    }

    /**
     * @brief Returns the first element that matches @p match, or default T{} if none does.
     *
     * C++ counterpart of .NET ImmutableList<T>.Find(Predicate<T>).
     * @param match The predicate to test.
     * @return The first matching element, or default T{}.
     * @throws System::ArgumentNullException if @p match is empty.
     */
    [[nodiscard]] T Find(std::function<bool(const T&)> match) const {
        requirePredicate(match);
        const auto found = std::find_if(data_->begin(), data_->end(), match);
        return found == data_->end() ? T{} : *found;
    }

    /**
     * @brief Returns a new list containing every element that matches @p match.
     *
     * C++ counterpart of .NET ImmutableList<T>.FindAll(Predicate<T>).
     * @param match The predicate to test.
     * @return An immutable list of the matching elements, in list order.
     * @throws System::ArgumentNullException if @p match is empty.
     */
    [[nodiscard]] ImmutableList<T> FindAll(std::function<bool(const T&)> match) const {
        requirePredicate(match);
        auto values = std::make_shared<std::vector<T>>();
        values->reserve(data_->size());
        for (const auto& item : *data_) {
            if (match(item)) {
                values->push_back(item);
            }
        }
        return ImmutableList<T>(std::move(values));
    }

    /**
     * @brief Returns the index of the first element that matches @p match, or -1.
     *
     * C++ counterpart of .NET ImmutableList<T>.FindIndex(Predicate<T>).
     * @param match The predicate to test.
     * @return The first matching index, or -1 if no element matches.
     * @throws System::ArgumentNullException if @p match is empty.
     */
    [[nodiscard]] intcs FindIndex(std::function<bool(const T&)> match) const {
        requirePredicate(match);
        for (intcs index = 0; index < getCountProperty(); ++index) {
            if (match((*data_)[static_cast<size_t>(index)])) {
                return index;
            }
        }
        return -1;
    }

    /**
     * @brief Returns the last element that matches @p match, or default T{} if none does.
     *
     * C++ counterpart of .NET ImmutableList<T>.FindLast(Predicate<T>).
     * @param match The predicate to test.
     * @return The last matching element, or default T{}.
     * @throws System::ArgumentNullException if @p match is empty.
     */
    [[nodiscard]] T FindLast(std::function<bool(const T&)> match) const {
        const intcs index = FindLastIndex(std::move(match));
        return index == -1 ? T{} : (*data_)[static_cast<size_t>(index)];
    }

    /**
     * @brief Returns the index of the last element that matches @p match, or -1.
     *
     * C++ counterpart of .NET ImmutableList<T>.FindLastIndex(Predicate<T>).
     * @param match The predicate to test.
     * @return The last matching index, or -1 if no element matches.
     * @throws System::ArgumentNullException if @p match is empty.
     */
    [[nodiscard]] intcs FindLastIndex(std::function<bool(const T&)> match) const {
        requirePredicate(match);
        for (intcs index = getCountProperty() - 1; index >= 0; --index) {
            if (match((*data_)[static_cast<size_t>(index)])) {
                return index;
            }
        }
        return -1;
    }

    /**
     * @brief Determines whether every element matches @p match.
     *
     * C++ counterpart of .NET ImmutableList<T>.TrueForAll(Predicate<T>).
     * @param match The predicate to test.
     * @return true if all elements match, including when the list is empty.
     * @throws System::ArgumentNullException if @p match is empty.
     */
    [[nodiscard]] bool TrueForAll(std::function<bool(const T&)> match) const {
        requirePredicate(match);
        return std::all_of(data_->begin(), data_->end(), match);
    }

    /**
     * @brief Determines whether the list contains the specified element.
     *
     * C++ counterpart of .NET ImmutableList<T>.Contains(T).
     * @param item The element to locate.
     * @return true if found; otherwise false.
     */
    [[nodiscard]] bool Contains(const T& item) const {
        return std::find(data_->begin(), data_->end(), item) != data_->end();
    }

    /**
     * @brief Returns the zero-based index of the first occurrence of @p item, or -1 if not found.
     *
     * C++ counterpart of .NET ImmutableList<T>.IndexOf(T).
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
     * C++ counterpart of .NET ImmutableList<T>.LastIndexOf(T).
     * @param item The element to locate.
     * @return The zero-based index of the last occurrence, or -1 if not found.
     */
    [[nodiscard]] intcs LastIndexOf(const T& item) const {
        for (intcs i = getCountProperty() - 1; i >= 0; --i)
            if ((*data_)[static_cast<size_t>(i)] == item) return i;
        return -1;
    }

    /**
     * @brief Searches a sorted list for @p item using binary search.
     *
     * C++ counterpart of .NET ImmutableList<T>.BinarySearch(T).
     * The list must be sorted. Returns a negative number (bitwise complement of
     * insertion point) if not found, matching .NET semantics.
     * @param item The element to search for.
     * @return The zero-based index if found; otherwise ~insertionPoint.
     */
    [[nodiscard]] intcs BinarySearch(const T& item) const {
        intcs lo = 0, hi = getCountProperty() - 1;
        while (lo <= hi) {
            intcs mid = lo + (hi - lo) / 2;
            const T& mid_val = (*data_)[static_cast<size_t>(mid)];
            if (mid_val == item) return mid;
            if (mid_val < item) lo = mid + 1;
            else hi = mid - 1;
        }
        return ~lo;
    }

    /** @brief Returns a const iterator to the beginning of the list (STL interop). */
    auto begin() const { return data_->begin(); }
    /** @brief Returns a const iterator past the end of the list (STL interop). */
    auto end()   const { return data_->end(); }
};

} // namespace System::Collections::Immutable
