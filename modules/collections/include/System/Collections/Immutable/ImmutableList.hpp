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
#include "System/Collections/Generic/IComparer.hpp"
#include "System/Collections/Generic/IEqualityComparer.hpp"
#include "System/detail/ComparisonPolicy.hpp"

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
 * BinarySearch) and the core mutable Builder workflow.
 * Deliberately deferred relative to real .NET's ImmutableList<T> (a much larger surface backed
 * by an AVL tree, not a flat vector): advanced Builder query, sorting, and copy operations. The
 * implemented Builder makes independent vector-backed immutable snapshots, so it does not claim
 * the .NET tree implementation's O(1) ToBuilder or near-O(1) ToImmutable characteristics.
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
    /**
     * @brief Mutable staging collection for building an ImmutableList snapshot.
     *
     * C++ counterpart of .NET ImmutableList<T>.Builder. It uses the current
     * vector-backed representation, so ToImmutable copies the current contents
     * and later Builder changes cannot mutate an already returned list.
     */
    class Builder {
        std::vector<T> data_;

        void requireIndexInRange(intcs index) const {
            if (index < 0 || index >= static_cast<intcs>(data_.size())) {
                throw System::ArgumentOutOfRangeException(
                    "index", "Index was out of range. Must be non-negative and less than the size of the collection.");
            }
        }

        void requireValidRange(intcs index, intcs count) const {
            if (index < 0 || index > static_cast<intcs>(data_.size())) {
                throw System::ArgumentOutOfRangeException(
                    "index", "Index was out of range. Must be non-negative and less than or equal to the size of the collection.");
            }
            if (count < 0 || index > static_cast<intcs>(data_.size()) - count) {
                throw System::ArgumentOutOfRangeException(
                    "count", "Count must refer to a location within the collection.");
            }
        }

    public:
        /** @brief Creates an empty Builder. Prefer ImmutableList<T>::CreateBuilder(). */
        Builder() = default;

        /** @brief Creates a Builder initialized from the supplied vector. */
        explicit Builder(const std::vector<T>& items) : data_(items) {}

        /** @brief Gets the number of elements in this Builder. */
        [[nodiscard]] intcs getCountProperty() const { return static_cast<intcs>(data_.size()); }

        /** @brief Gets a mutable reference to an element after range validation. */
        T& operator[](intcs index) {
            requireIndexInRange(index);
            return data_[static_cast<size_t>(index)];
        }

        /** @brief Gets a const reference to an element after range validation. */
        const T& operator[](intcs index) const {
            requireIndexInRange(index);
            return data_[static_cast<size_t>(index)];
        }

        /** @brief Appends an item to this Builder. */
        void Add(const T& item) { data_.push_back(item); }

        /** @brief Appends all items in @p items to this Builder. */
        void AddRange(const std::vector<T>& items) {
            data_.insert(data_.end(), items.begin(), items.end());
        }

        /** @brief Inserts an item at @p index. */
        void Insert(intcs index, const T& item) {
            if (index < 0 || index > static_cast<intcs>(data_.size())) {
                throw System::ArgumentOutOfRangeException("index");
            }
            data_.insert(data_.begin() + index, item);
        }

        /** @brief Inserts all @p items at @p index. */
        void InsertRange(intcs index, const std::vector<T>& items) {
            if (index < 0 || index > static_cast<intcs>(data_.size())) {
                throw System::ArgumentOutOfRangeException("index");
            }
            data_.insert(data_.begin() + index, items.begin(), items.end());
        }

        /** @brief Replaces the element at @p index. */
        void SetItem(intcs index, const T& item) {
            requireIndexInRange(index);
            data_[static_cast<size_t>(index)] = item;
        }

        /** @brief Removes the first matching item and reports whether one was removed. */
        bool Remove(const T& item) {
            const auto found = System::detail::findValue(data_.begin(), data_.end(), item);
            if (found == data_.end()) {
                return false;
            }
            data_.erase(found);
            return true;
        }

        /** @brief Removes the element at @p index. */
        void RemoveAt(intcs index) {
            requireIndexInRange(index);
            data_.erase(data_.begin() + index);
        }

        /** @brief Removes @p count elements starting at @p index. */
        void RemoveRange(intcs index, intcs count) {
            requireValidRange(index, count);
            data_.erase(data_.begin() + index, data_.begin() + index + count);
        }

        /** @brief Removes all elements from this Builder. */
        void Clear() { data_.clear(); }

        /** @brief Determines whether this Builder contains @p item. */
        [[nodiscard]] bool Contains(const T& item) const {
            return System::detail::findValue(data_.begin(), data_.end(), item)
                   != data_.end();
        }

        /** @brief Returns the first index of @p item, or -1 if it is absent. */
        [[nodiscard]] intcs IndexOf(const T& item) const {
            const auto found = System::detail::findValue(data_.begin(), data_.end(), item);
            return found == data_.end() ? -1 : static_cast<intcs>(found - data_.begin());
        }

        /** @brief Returns an immutable snapshot unaffected by later Builder mutations. */
        [[nodiscard]] ImmutableList<T> ToImmutable() const {
            return ImmutableList<T>(std::make_shared<std::vector<T>>(data_));
        }
    };

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
     * @brief Creates an empty mutable Builder.
     *
     * C++ counterpart of .NET ImmutableList.CreateBuilder<T>().
     */
    [[nodiscard]] static Builder CreateBuilder() { return Builder(); }

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
     * @brief Creates a mutable Builder with a copy of this list's contents.
     *
     * C++ counterpart of .NET ImmutableList<T>.ToBuilder(). The vector-backed
     * representation copies contents so the source remains immutable.
     */
    [[nodiscard]] Builder ToBuilder() const { return Builder(*data_); }

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
        auto it = System::detail::findValue(v->begin(), v->end(), oldValue);
        if (it == v->end()) {
            throw System::ArgumentException("Cannot find the old value in the list.", "oldValue");
        }
        *it = newValue;
        return ImmutableList<T>(std::move(v));
    }

    /**
     * @brief Returns a new list with the first comparer-equal occurrence replaced.
     *
     * C++ counterpart of .NET ImmutableList<T>.Replace(T, T, IEqualityComparer<T>).
     * @param oldValue The value to replace.
     * @param newValue The replacement value.
     * @param equalityComparer The equality semantics used to find @p oldValue.
     * @return A new ImmutableList with the first matching occurrence replaced.
     * @throws System::ArgumentException if no matching occurrence exists.
     */
    [[nodiscard]] ImmutableList<T> Replace(
        const T& oldValue,
        const T& newValue,
        const System::Collections::Generic::IEqualityComparer<T>& equalityComparer) const {
        auto v = std::make_shared<std::vector<T>>(*data_);
        auto it = std::find_if(v->begin(), v->end(), [&oldValue, &equalityComparer](const T& value) {
            return equalityComparer.Equals(oldValue, value);
        });
        if (it == v->end()) {
            throw System::ArgumentException("Cannot find the old value in the list.", "oldValue");
        }
        *it = newValue;
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
            if (!removed && System::detail::equalValues(x, item)) { removed = true; continue; }
            v->push_back(x);
        }
        return ImmutableList<T>(std::move(v));
    }

    /**
     * @brief Returns a new list with the first comparer-equal occurrence removed.
     *
     * C++ counterpart of .NET ImmutableList<T>.Remove(T, IEqualityComparer<T>).
     * @param item The value to remove.
     * @param equalityComparer The equality semantics used to find @p item.
     * @return A new ImmutableList with the first matching element removed.
     */
    [[nodiscard]] ImmutableList<T> Remove(
        const T& item,
        const System::Collections::Generic::IEqualityComparer<T>& equalityComparer) const {
        auto v = std::make_shared<std::vector<T>>(*data_);
        auto it = std::find_if(v->begin(), v->end(), [&item, &equalityComparer](const T& value) {
            return equalityComparer.Equals(item, value);
        });
        if (it != v->end()) {
            v->erase(it);
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
     * @brief Returns a new list after removing one occurrence for each requested value.
     *
     * C++ counterpart of .NET ImmutableList<T>.RemoveRange(IEnumerable<T>).
     * Values are processed in input order, so repeated values can remove repeated occurrences.
     * @param items The values to remove.
     * @return A new ImmutableList with matching values removed.
     */
    [[nodiscard]] ImmutableList<T> RemoveRange(const std::vector<T>& items) const {
        auto v = std::make_shared<std::vector<T>>(*data_);
        for (const auto& item : items) {
            auto it = System::detail::findValue(v->begin(), v->end(), item);
            if (it != v->end()) {
                v->erase(it);
            }
        }
        return ImmutableList<T>(std::move(v));
    }

    /**
     * @brief Returns a new list after removing one comparer-equal occurrence for each value.
     *
     * C++ counterpart of .NET ImmutableList<T>.RemoveRange(IEnumerable<T>, IEqualityComparer<T>).
     * Values are processed in input order, so repeated values can remove repeated occurrences.
     * @param items The values to remove.
     * @param equalityComparer The equality semantics used to find values.
     * @return A new ImmutableList with matching values removed.
     */
    [[nodiscard]] ImmutableList<T> RemoveRange(
        const std::vector<T>& items,
        const System::Collections::Generic::IEqualityComparer<T>& equalityComparer) const {
        auto v = std::make_shared<std::vector<T>>(*data_);
        for (const auto& item : items) {
            auto it = std::find_if(v->begin(), v->end(), [&item, &equalityComparer](const T& value) {
                return equalityComparer.Equals(item, value);
            });
            if (it != v->end()) {
                v->erase(it);
            }
        }
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
     * @brief Converts every element and returns an immutable list of the converted values.
     *
     * C++ counterpart of .NET ImmutableList<T>.ConvertAll<TOutput>(Converter<T, TOutput>).
     * @tparam TOutput The target element type.
     * @param converter The conversion function.
     * @return A new immutable list containing converted values in source order.
     * @throws System::ArgumentNullException if @p converter is empty.
     */
    template<typename TOutput>
    [[nodiscard]] ImmutableList<TOutput> ConvertAll(
        std::function<TOutput(const T&)> converter) const {
        if (!converter) {
            throw System::ArgumentNullException("converter");
        }

        std::vector<TOutput> values;
        values.reserve(data_->size());
        for (const auto& item : *data_) {
            values.push_back(converter(item));
        }
        return ImmutableList<TOutput>::Create(values);
    }

    /**
     * @brief Copies every element to a compatible destination vector.
     *
     * C++ counterpart of .NET ImmutableList<T>.CopyTo(T[]). The vector is not
     * resized: callers retain the same fixed-destination-array contract as .NET.
     * @throws System::ArgumentException if @p destination is too small.
     */
    void CopyTo(std::vector<T>& destination) const {
        CopyTo(0, destination, 0, getCountProperty());
    }

    /**
     * @brief Copies every element to @p destination starting at @p arrayIndex.
     *
     * C++ counterpart of .NET ImmutableList<T>.CopyTo(T[], int).
     * @throws System::ArgumentOutOfRangeException if @p arrayIndex is negative.
     * @throws System::ArgumentException if @p destination cannot hold all elements.
     */
    void CopyTo(std::vector<T>& destination, intcs arrayIndex) const {
        CopyTo(0, destination, arrayIndex, getCountProperty());
    }

    /**
     * @brief Copies a source range to a compatible destination vector.
     *
     * C++ counterpart of .NET ImmutableList<T>.CopyTo(int, T[], int, int).
     * @param index The zero-based source index.
     * @param destination The fixed-size destination vector.
     * @param arrayIndex The zero-based destination index.
     * @param count The number of elements to copy.
     * @throws System::ArgumentOutOfRangeException if a source index, destination index, or
     *         count is negative, or the source range is invalid.
     * @throws System::ArgumentException if @p destination is too small.
     */
    void CopyTo(intcs index, std::vector<T>& destination, intcs arrayIndex, intcs count) const {
        const intcs sourceCount = getCountProperty();
        if (index < 0 || index > sourceCount) {
            throw System::ArgumentOutOfRangeException("index", "Index was out of range. Must be non-negative and within the collection.");
        }
        if (count < 0 || count > sourceCount - index) {
            throw System::ArgumentOutOfRangeException("count", "Count must refer to a location within the collection.");
        }
        if (arrayIndex < 0) {
            throw System::ArgumentOutOfRangeException("arrayIndex", "Non-negative number required.");
        }
        // Use subtraction after validating the non-negative destination index. This avoids the
        // signed-overflow hazard in an `arrayIndex + count > destination.size()` check.
        if (static_cast<intcs>(destination.size()) - arrayIndex < count) {
            throw System::ArgumentException(
                "Destination array was not long enough. Check the destination index, length, and the array's lower bounds.");
        }

        std::copy(data_->begin() + index, data_->begin() + index + count,
                  destination.begin() + arrayIndex);
    }

    /**
     * @brief Returns a new list with all elements sorted under .NET's default
     *        comparison contract.
     *
     * C++ counterpart of .NET ImmutableList<T>.Sort(), which uses
     * `Comparer<T>.Default` rather than the element type's `<`. NaN orders before every
     * value including negative infinity, and is moved out of the comparator's input by a
     * pre-pass first — see List<T>::Sort() for why that is a correctness requirement
     * rather than an optimisation.
     * @return A sorted immutable list; the source list is unchanged.
     */
    [[nodiscard]] ImmutableList<T> Sort() const {
        auto values = std::make_shared<std::vector<T>>(*data_);
        System::detail::defaultSort(values->begin(), values->end());
        return ImmutableList<T>(std::move(values));
    }

    /**
     * @brief Returns a new list with only the requested range sorted under .NET's
     *        default comparison contract.
     *
     * C++ counterpart of .NET ImmutableList<T>.Sort(int, int, IComparer<T>) using the
     * default comparison. Same NaN-before-everything rule and same NaN pre-pass as
     * Sort(); the pre-pass is confined to the requested range, so elements outside it
     * keep their positions.
     * @param index The zero-based first element in the range.
     * @param count The number of elements to sort.
     * @return A list with only the requested range sorted; the source is unchanged.
     * @throws System::ArgumentOutOfRangeException if the range is outside this list.
     */
    [[nodiscard]] ImmutableList<T> Sort(intcs index, intcs count) const {
        requireValidRange(index, count);
        auto values = std::make_shared<std::vector<T>>(*data_);
        System::detail::defaultSort(values->begin() + index, values->begin() + index + count);
        return ImmutableList<T>(std::move(values));
    }

    /**
     * @brief Returns a new list sorted by a custom comparison delegate.
     *
     * C++ counterpart of .NET ImmutableList<T>.Sort(Comparison<T>). The comparison follows
     * the established project convention: negative means before, zero equivalent, and positive
     * means after.
     * @param comparison The comparison delegate.
     * @return A sorted immutable list; the source list is unchanged.
     * @throws System::ArgumentNullException if @p comparison is empty.
     */
    [[nodiscard]] ImmutableList<T> Sort(std::function<intcs(const T&, const T&)> comparison) const {
        if (!comparison) {
            throw System::ArgumentNullException("comparison");
        }

        auto values = std::make_shared<std::vector<T>>(*data_);
        std::sort(values->begin(), values->end(),
                  [&comparison](const T& left, const T& right) {
                      return comparison(left, right) < 0;
                  });
        return ImmutableList<T>(std::move(values));
    }

    /**
     * @brief Returns a new list sorted by an `IComparer<T>`.
     *
     * C++ counterpart of .NET ImmutableList<T>.Sort(IComparer<T>). A C++
     * reference cannot be null; use the parameterless overload for the default
     * comparison.
     * @param comparer The comparer used to order elements.
     * @return A sorted immutable list; the source list is unchanged.
     */
    [[nodiscard]] ImmutableList<T> Sort(
        const System::Collections::Generic::IComparer<T>& comparer) const {
        return Sort(0, getCountProperty(), comparer);
    }

    /**
     * @brief Returns a new list with a range sorted by an `IComparer<T>`.
     *
     * C++ counterpart of .NET ImmutableList<T>.Sort(int, int, IComparer<T>).
     * @param index The zero-based first source element in the range.
     * @param count The number of elements to sort.
     * @param comparer The comparer used to order the range.
     * @return A list with only the requested range sorted; the source is unchanged.
     * @throws System::ArgumentOutOfRangeException if the range is outside this list.
     */
    [[nodiscard]] ImmutableList<T> Sort(
        intcs index,
        intcs count,
        const System::Collections::Generic::IComparer<T>& comparer) const {
        requireValidRange(index, count);
        auto values = std::make_shared<std::vector<T>>(*data_);
        std::sort(values->begin() + index, values->begin() + index + count,
                  [&comparer](const T& left, const T& right) {
                      return comparer.Compare(left, right) < 0;
                  });
        return ImmutableList<T>(std::move(values));
    }

    /**
     * @brief Returns a new list whose elements are in reverse order.
     *
     * C++ counterpart of .NET ImmutableList<T>.Reverse().
     * @return A reversed immutable list; the source list is unchanged.
     */
    [[nodiscard]] ImmutableList<T> Reverse() const {
        auto values = std::make_shared<std::vector<T>>(*data_);
        std::reverse(values->begin(), values->end());
        return ImmutableList<T>(std::move(values));
    }

    /**
     * @brief Returns a new list with the specified range in reverse order.
     *
     * C++ counterpart of .NET ImmutableList<T>.Reverse(int, int).
     * @param index The zero-based first element in the range.
     * @param count The number of elements to reverse.
     * @return A list with only the requested range reversed; the source is unchanged.
     * @throws System::ArgumentOutOfRangeException if the range is outside this list.
     */
    [[nodiscard]] ImmutableList<T> Reverse(intcs index, intcs count) const {
        requireValidRange(index, count);
        auto values = std::make_shared<std::vector<T>>(*data_);
        std::reverse(values->begin() + index, values->begin() + index + count);
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
        return System::detail::findValue(data_->begin(), data_->end(), item)
               != data_->end();
    }

    /**
     * @brief Returns the zero-based index of the first occurrence of @p item, or -1 if not found.
     *
     * C++ counterpart of .NET ImmutableList<T>.IndexOf(T).
     * @param item The element to locate.
     * @return The zero-based index, or -1 if not found.
     */
    [[nodiscard]] intcs IndexOf(const T& item) const {
        auto it = System::detail::findValue(data_->begin(), data_->end(), item);
        return it == data_->end() ? -1 : static_cast<intcs>(it - data_->begin());
    }

    /**
     * @brief Returns the first index of @p item within the requested range, or -1.
     *
     * C++ counterpart of .NET ImmutableList<T>.IndexOf(T, int, int, IEqualityComparer<T>)
     * using the default equality operator.
     * @param item The element to locate.
     * @param index The zero-based first index to search.
     * @param count The number of elements to search.
     * @return The first matching index in the range, or -1 if not found.
     * @throws System::ArgumentOutOfRangeException if the range is invalid.
     */
    [[nodiscard]] intcs IndexOf(const T& item, intcs index, intcs count) const {
        requireValidRange(index, count);
        const intcs end = index + count;
        for (intcs current = index; current < end; ++current) {
            if (System::detail::equalValues((*data_)[static_cast<size_t>(current)], item)) {
                return current;
            }
        }
        return -1;
    }

    /**
     * @brief Returns the first comparer-equal index within the requested range, or -1.
     *
     * C++ counterpart of .NET ImmutableList<T>.IndexOf(T, int, int, IEqualityComparer<T>).
     * @param item The element to locate.
     * @param index The zero-based first index to search.
     * @param count The number of elements to search.
     * @param equalityComparer The equality semantics used to compare values.
     * @return The first matching index in the range, or -1 if not found.
     * @throws System::ArgumentOutOfRangeException if the range is invalid.
     */
    [[nodiscard]] intcs IndexOf(
        const T& item,
        intcs index,
        intcs count,
        const System::Collections::Generic::IEqualityComparer<T>& equalityComparer) const {
        requireValidRange(index, count);
        const intcs end = index + count;
        for (intcs current = index; current < end; ++current) {
            if (equalityComparer.Equals(item, (*data_)[static_cast<size_t>(current)])) {
                return current;
            }
        }
        return -1;
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
            if (System::detail::equalValues((*data_)[static_cast<size_t>(i)], item)) return i;
        return -1;
    }

    /**
     * @brief Returns the last index of @p item within the requested backward range, or -1.
     *
     * C++ counterpart of .NET ImmutableList<T>.LastIndexOf(T, int, int,
     * IEqualityComparer<T>) using the default equality operator.
     * @param item The element to locate.
     * @param index The zero-based index at which to begin searching backward.
     * @param count The number of elements to search.
     * @return The last matching index in the range, or -1 if not found.
     * @throws System::ArgumentOutOfRangeException if the range is invalid.
     */
    [[nodiscard]] intcs LastIndexOf(const T& item, intcs index, intcs count) const {
        if (index == 0 && count == 0) {
            return -1;
        }

        const intcs size = getCountProperty();
        if (index < 0 || index >= size) {
            throw System::ArgumentOutOfRangeException("index");
        }
        if (count < 0 || count > size || index - count + 1 < 0) {
            throw System::ArgumentOutOfRangeException("count");
        }

        const intcs first = index - count + 1;
        for (intcs current = index; current >= first; --current) {
            if (System::detail::equalValues((*data_)[static_cast<size_t>(current)], item)) {
                return current;
            }
        }
        return -1;
    }

    /**
     * @brief Returns the last comparer-equal index within the requested backward range, or -1.
     *
     * C++ counterpart of .NET ImmutableList<T>.LastIndexOf(T, int, int,
     * IEqualityComparer<T>).
     * @param item The element to locate.
     * @param index The zero-based index at which to begin searching backward.
     * @param count The number of elements to search.
     * @param equalityComparer The equality semantics used to compare values.
     * @return The last matching index in the range, or -1 if not found.
     * @throws System::ArgumentOutOfRangeException if the range is invalid.
     */
    [[nodiscard]] intcs LastIndexOf(
        const T& item,
        intcs index,
        intcs count,
        const System::Collections::Generic::IEqualityComparer<T>& equalityComparer) const {
        if (index == 0 && count == 0) {
            return -1;
        }

        const intcs size = getCountProperty();
        if (index < 0 || index >= size) {
            throw System::ArgumentOutOfRangeException("index");
        }
        if (count < 0 || count > size || index - count + 1 < 0) {
            throw System::ArgumentOutOfRangeException("count");
        }

        const intcs first = index - count + 1;
        for (intcs current = index; current >= first; --current) {
            if (equalityComparer.Equals(item, (*data_)[static_cast<size_t>(current)])) {
                return current;
            }
        }
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
            const int cmp = System::detail::compareValues(
                (*data_)[static_cast<size_t>(mid)], item);
            if (cmp == 0) return mid;
            if (cmp < 0) lo = mid + 1;
            else         hi = mid - 1;
        }
        return ~lo;
    }

    /**
     * @brief Searches the entire sorted list using @p comparer.
     *
     * C++ counterpart of .NET ImmutableList<T>.BinarySearch(T, IComparer<T>).
     * @param item The element to search for.
     * @param comparer The comparison semantics used by the sorted list.
     * @return The zero-based index if found; otherwise ~insertionPoint.
     */
    [[nodiscard]] intcs BinarySearch(
        const T& item,
        const System::Collections::Generic::IComparer<T>& comparer) const {
        return BinarySearch(0, getCountProperty(), item, comparer);
    }

    /**
     * @brief Searches a sorted range using @p comparer.
     *
     * C++ counterpart of .NET ImmutableList<T>.BinarySearch(int, int, T, IComparer<T>).
     * @param index The zero-based first index of the range to search.
     * @param count The number of elements to search.
     * @param item The element to search for.
     * @param comparer The comparison semantics used by the sorted range.
     * @return The zero-based index if found; otherwise the complement of the absolute insertion
     *         point.
     * @throws System::ArgumentOutOfRangeException if the range is invalid.
     */
    [[nodiscard]] intcs BinarySearch(
        intcs index,
        intcs count,
        const T& item,
        const System::Collections::Generic::IComparer<T>& comparer) const {
        requireValidRange(index, count);
        intcs lo = index;
        intcs hi = index + count - 1;
        while (lo <= hi) {
            const intcs mid = lo + (hi - lo) / 2;
            const intcs comparison = comparer.Compare(item, (*data_)[static_cast<size_t>(mid)]);
            if (comparison == 0) {
                return mid;
            }
            if (comparison > 0) {
                lo = mid + 1;
            } else {
                hi = mid - 1;
            }
        }
        return ~lo;
    }

    /** @brief Returns a const iterator to the beginning of the list (STL interop). */
    auto begin() const { return data_->begin(); }
    /** @brief Returns a const iterator past the end of the list (STL interop). */
    auto end()   const { return data_->end(); }
};

} // namespace System::Collections::Immutable
