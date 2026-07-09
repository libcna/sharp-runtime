// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <set>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentException.hpp"

namespace System::Collections::Generic {

using SharpRuntime::intcs;

/**
 * @brief Represents a collection of objects maintained in sorted order with no duplicate elements.
 *
 * C++ counterpart of .NET System.Collections.Generic.SortedSet<T>.
 * Backed by std::set<T>; provides O(log n) Add, Remove, and Contains.
 *
 * @tparam T The type of elements in the set (must support operator< for ordering).
 */
template<typename T>
class SortedSet {
    std::set<T> data_;

public:
    /** @brief Initializes a new empty SortedSet. */
    SortedSet() = default;

    /**
     * @brief Initializes a SortedSet with elements from an initializer list.
     * @param items The initial elements.
     */
    explicit SortedSet(std::initializer_list<T> items) : data_(items) {}

    /**
     * @brief Gets the number of elements in the SortedSet.
     *
     * C++ counterpart of .NET SortedSet<T>.Count.
     * @return The number of elements.
     */
    [[nodiscard]] intcs getCountProperty() const { return static_cast<intcs>(data_.size()); }

    /**
     * @brief Gets a value indicating whether the SortedSet contains no elements.
     * @return true if the set is empty; otherwise false.
     */
    [[nodiscard]] bool getIsEmptyProperty() const { return data_.empty(); }

    /**
     * @brief Gets the minimum value in the SortedSet, or a default-constructed T if empty.
     *
     * C++ counterpart of .NET SortedSet<T>.Min, which returns default(T) (.NET's nullable
     * T?) rather than throwing when the set is empty.
     */
    [[nodiscard]] T getMinProperty() const { return data_.empty() ? T{} : *data_.begin(); }

    /**
     * @brief Gets the maximum value in the SortedSet, or a default-constructed T if empty.
     *
     * C++ counterpart of .NET SortedSet<T>.Max, which returns default(T) (.NET's nullable
     * T?) rather than throwing when the set is empty.
     */
    [[nodiscard]] T getMaxProperty() const { return data_.empty() ? T{} : *data_.rbegin(); }

    /**
     * @brief Adds the specified element to the set.
     *
     * C++ counterpart of .NET SortedSet<T>.Add(T).
     * @param item The element to add.
     * @return true if the element was added; false if it was already present.
     */
    bool Add(const T& item) { return data_.insert(item).second; }

    /**
     * @brief Removes the specified element from the set.
     *
     * C++ counterpart of .NET SortedSet<T>.Remove(T).
     * @param item The element to remove.
     * @return true if the element was found and removed; otherwise false.
     */
    bool Remove(const T& item) { return data_.erase(item) > 0; }

    /**
     * @brief Determines whether the set contains the specified element.
     *
     * C++ counterpart of .NET SortedSet<T>.Contains(T).
     * @param item The element to locate.
     * @return true if found; otherwise false.
     */
    [[nodiscard]] bool Contains(const T& item) const {
        return data_.find(item) != data_.end();
    }

    /**
     * @brief Removes all elements from the set.
     *
     * C++ counterpart of .NET SortedSet<T>.Clear().
     */
    void Clear() { data_.clear(); }

    /**
     * @brief Modifies the set to contain all elements in itself and the specified set.
     *
     * C++ counterpart of .NET SortedSet<T>.UnionWith(IEnumerable<T>).
     * @param other The set of elements to add.
     */
    void UnionWith(const SortedSet<T>& other) {
        for (const auto& x : other.data_) data_.insert(x);
    }

    /**
     * @brief Modifies the set to contain only elements also present in the specified set.
     *
     * C++ counterpart of .NET SortedSet<T>.IntersectWith(IEnumerable<T>).
     * @param other The set to intersect with.
     */
    void IntersectWith(const SortedSet<T>& other) {
        std::set<T> result;
        for (const auto& x : data_) if (other.Contains(x)) result.insert(x);
        data_ = std::move(result);
    }

    /**
     * @brief Removes all elements in the specified set from the current set.
     *
     * C++ counterpart of .NET SortedSet<T>.ExceptWith(IEnumerable<T>).
     * @param other The set of elements to remove.
     */
    void ExceptWith(const SortedSet<T>& other) {
        for (const auto& x : other.data_) data_.erase(x);
    }

    /**
     * @brief Modifies the set so it contains only elements present in one set but not both.
     *
     * C++ counterpart of .NET SortedSet<T>.SymmetricExceptWith(IEnumerable<T>).
     * @param other The set to compare to.
     */
    void SymmetricExceptWith(const SortedSet<T>& other) {
        for (const auto& x : other.data_) {
            auto it = data_.find(x);
            if (it != data_.end()) data_.erase(it);
            else data_.insert(x);
        }
    }

    /**
     * @brief Determines whether the set is a subset of the specified set.
     *
     * C++ counterpart of .NET SortedSet<T>.IsSubsetOf(IEnumerable<T>).
     * @param other The set to compare to.
     * @return true if every element of this set is in @p other; otherwise false.
     */
    [[nodiscard]] bool IsSubsetOf(const SortedSet<T>& other) const {
        for (const auto& x : data_) if (!other.Contains(x)) return false;
        return true;
    }

    /**
     * @brief Determines whether the set is a superset of the specified set.
     *
     * C++ counterpart of .NET SortedSet<T>.IsSupersetOf(IEnumerable<T>).
     * @param other The set to compare to.
     * @return true if every element of @p other is in this set; otherwise false.
     */
    [[nodiscard]] bool IsSupersetOf(const SortedSet<T>& other) const {
        return other.IsSubsetOf(*this);
    }

    /**
     * @brief Determines whether the set is a proper (strict) subset of the specified set.
     *
     * C++ counterpart of .NET SortedSet<T>.IsProperSubsetOf(IEnumerable<T>).
     * @param other The set to compare to.
     * @return true if this set is a subset of @p other and @p other has more elements.
     */
    [[nodiscard]] bool IsProperSubsetOf(const SortedSet<T>& other) const {
        return IsSubsetOf(other) && !SetEquals(other);
    }

    /**
     * @brief Determines whether the set is a proper (strict) superset of the specified set.
     *
     * C++ counterpart of .NET SortedSet<T>.IsProperSupersetOf(IEnumerable<T>).
     * @param other The set to compare to.
     * @return true if this set is a superset of @p other and has more elements.
     */
    [[nodiscard]] bool IsProperSupersetOf(const SortedSet<T>& other) const {
        return IsSupersetOf(other) && !SetEquals(other);
    }

    /**
     * @brief Determines whether the set and the specified set contain the same elements.
     *
     * C++ counterpart of .NET SortedSet<T>.SetEquals(IEnumerable<T>).
     * @param other The set to compare to.
     * @return true if the two sets are equal; otherwise false.
     */
    [[nodiscard]] bool SetEquals(const SortedSet<T>& other) const {
        return data_ == other.data_;
    }

    /**
     * @brief Determines whether the set and the specified set share any common elements.
     *
     * C++ counterpart of .NET SortedSet<T>.Overlaps(IEnumerable<T>).
     * @param other The set to compare to.
     * @return true if at least one element is common to both; otherwise false.
     */
    [[nodiscard]] bool Overlaps(const SortedSet<T>& other) const {
        for (const auto& x : other.data_)
            if (data_.find(x) != data_.end()) return true;
        return false;
    }

    /**
     * @brief Returns a view of elements in the inclusive range [@p lower, @p upper].
     *
     * C++ counterpart of .NET SortedSet<T>.GetViewBetween(T, T).
     * @param lower The minimum value (inclusive).
     * @param upper The maximum value (inclusive).
     * @return A new SortedSet<T> containing elements in the range.
     * @throws System::ArgumentException if @p lower is greater than @p upper.
     */
    [[nodiscard]] SortedSet<T> GetViewBetween(const T& lower, const T& upper) const {
        if (lower > upper)
            throw System::ArgumentException("lowerValue is greater than upperValue.", "lowerValue");
        SortedSet<T> view;
        for (auto it = data_.lower_bound(lower); it != data_.end() && !(*it > upper); ++it)
            view.Add(*it);
        return view;
    }

    /**
     * @brief Copies the set elements to a new vector in sorted order.
     *
     * C++ counterpart of .NET SortedSet<T>.CopyTo(T[]).
     * @return A std::vector<T> containing all elements in ascending order.
     */
    [[nodiscard]] std::vector<T> ToVector() const {
        return std::vector<T>(data_.begin(), data_.end());
    }

    /** @brief Returns a const iterator to the beginning of the SortedSet (STL interop). */
    auto begin() const { return data_.begin(); }
    /** @brief Returns a const iterator past the end of the SortedSet (STL interop). */
    auto end()   const { return data_.end(); }
};

} // namespace System::Collections::Generic
