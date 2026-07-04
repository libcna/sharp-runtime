// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include <set>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::Collections::Immutable {

using SharpRuntime::intcs;

/**
 * @brief Represents an immutable sorted set with no duplicate elements.
 *
 * C++ counterpart of .NET System.Collections.Immutable.ImmutableSortedSet<T>.
 * Backed by std::set; provides O(log n) Add, Remove, and Contains with sorted iteration.
 * All mutating operations return a new instance; the original is unchanged.
 *
 * @tparam T The type of elements (must support operator< for ordering).
 */
template<typename T>
class ImmutableSortedSet {
    using SetT = std::set<T>;
    std::shared_ptr<const SetT> data_;

    explicit ImmutableSortedSet(std::shared_ptr<const SetT> data) : data_(std::move(data)) {}

public:
    /** @brief Default-constructs an empty ImmutableSortedSet. */
    ImmutableSortedSet() : data_(std::make_shared<SetT>()) {}

    /**
     * @brief Returns an empty ImmutableSortedSet.
     *
     * C++ counterpart of .NET ImmutableSortedSet<T>.Empty.
     * @return An empty ImmutableSortedSet<T>.
     */
    static ImmutableSortedSet<T> Empty() { return ImmutableSortedSet<T>(); }

    /**
     * @brief Creates an ImmutableSortedSet from an initializer list.
     *
     * C++ counterpart of .NET ImmutableSortedSet.Create<T>(params T[] items).
     * @param items The initial elements.
     * @return A new ImmutableSortedSet containing the given elements.
     */
    static ImmutableSortedSet<T> Create(std::initializer_list<T> items) {
        return ImmutableSortedSet<T>(std::make_shared<SetT>(items));
    }

    /**
     * @brief Creates an ImmutableSortedSet from an existing vector.
     *
     * C++ counterpart of .NET ImmutableSortedSet.CreateRange<T>(IEnumerable<T>).
     * @param items The initial elements.
     * @return A new ImmutableSortedSet containing the given elements.
     */
    static ImmutableSortedSet<T> Create(const std::vector<T>& items) {
        return ImmutableSortedSet<T>(std::make_shared<SetT>(items.begin(), items.end()));
    }

    /**
     * @brief Gets the number of elements in the set.
     *
     * C++ counterpart of .NET ImmutableSortedSet<T>.Count.
     * @return The number of elements.
     */
    [[nodiscard]] intcs getCountProperty() const { return static_cast<intcs>(data_->size()); }

    /**
     * @brief Gets a value indicating whether the set is empty.
     *
     * C++ counterpart of .NET ImmutableSortedSet<T>.IsEmpty.
     * @return true if the set contains no elements; otherwise false.
     */
    [[nodiscard]] bool getIsEmptyProperty() const { return data_->empty(); }

    /**
     * @brief Gets the smallest element in the set, or a default-constructed T if empty.
     *
     * C++ counterpart of .NET ImmutableSortedSet<T>.Min, which returns default(T) rather
     * than throwing when the set is empty.
     */
    [[nodiscard]] T getMinProperty() const { return data_->empty() ? T{} : *data_->begin(); }

    /**
     * @brief Gets the largest element in the set, or a default-constructed T if empty.
     *
     * C++ counterpart of .NET ImmutableSortedSet<T>.Max, which returns default(T) rather
     * than throwing when the set is empty.
     */
    [[nodiscard]] T getMaxProperty() const { return data_->empty() ? T{} : *data_->rbegin(); }

    /**
     * @brief Determines whether the set contains the specified element.
     *
     * C++ counterpart of .NET ImmutableSortedSet<T>.Contains(T).
     * @param item The element to locate.
     * @return true if found; otherwise false.
     */
    [[nodiscard]] bool Contains(const T& item) const {
        return data_->find(item) != data_->end();
    }

    /**
     * @brief Searches for a value equal to @p equalValue and returns the stored value.
     *
     * C++ counterpart of .NET ImmutableSortedSet<T>.TryGetValue(T, out T).
     * @param equalValue  The value to search for.
     * @param actualValue Receives the stored value if found.
     * @return true if a matching value was found; otherwise false.
     */
    bool TryGetValue(const T& equalValue, T& actualValue) const {
        auto it = data_->find(equalValue);
        if (it == data_->end()) return false;
        actualValue = *it;
        return true;
    }

    /**
     * @brief Returns a new set with @p item added.
     *
     * C++ counterpart of .NET ImmutableSortedSet<T>.Add(T).
     * @param item The element to add.
     * @return A new ImmutableSortedSet with the element added (duplicate is ignored).
     */
    [[nodiscard]] ImmutableSortedSet<T> Add(const T& item) const {
        auto s = std::make_shared<SetT>(*data_);
        s->insert(item);
        return ImmutableSortedSet<T>(std::move(s));
    }

    /**
     * @brief Returns a new set with @p item removed.
     *
     * C++ counterpart of .NET ImmutableSortedSet<T>.Remove(T).
     * @param item The element to remove.
     * @return A new ImmutableSortedSet without the specified element.
     */
    [[nodiscard]] ImmutableSortedSet<T> Remove(const T& item) const {
        auto s = std::make_shared<SetT>(*data_);
        s->erase(item);
        return ImmutableSortedSet<T>(std::move(s));
    }

    /**
     * @brief Returns an empty ImmutableSortedSet.
     *
     * C++ counterpart of .NET ImmutableSortedSet<T>.Clear().
     * @return An empty ImmutableSortedSet<T>.
     */
    [[nodiscard]] ImmutableSortedSet<T> Clear() const { return Empty(); }

    /**
     * @brief Returns a new set containing all elements from both this set and @p other.
     *
     * C++ counterpart of .NET ImmutableSortedSet<T>.Union(IEnumerable<T>).
     * @param other The set to union with.
     * @return A new ImmutableSortedSet containing all elements from both sets.
     */
    [[nodiscard]] ImmutableSortedSet<T> Union(const ImmutableSortedSet<T>& other) const {
        auto s = std::make_shared<SetT>(*data_);
        for (const auto& x : *other.data_) s->insert(x);
        return ImmutableSortedSet<T>(std::move(s));
    }

    /**
     * @brief Returns a new set containing only elements present in both sets.
     *
     * C++ counterpart of .NET ImmutableSortedSet<T>.Intersect(IEnumerable<T>).
     * @param other The set to intersect with.
     * @return A new ImmutableSortedSet containing only common elements.
     */
    [[nodiscard]] ImmutableSortedSet<T> Intersect(const ImmutableSortedSet<T>& other) const {
        auto s = std::make_shared<SetT>();
        for (const auto& x : *data_) if (other.Contains(x)) s->insert(x);
        return ImmutableSortedSet<T>(std::move(s));
    }

    /**
     * @brief Returns a new set containing elements in this set that are not in @p other.
     *
     * C++ counterpart of .NET ImmutableSortedSet<T>.Except(IEnumerable<T>).
     * @param other The set whose elements to exclude.
     * @return A new ImmutableSortedSet without the elements from @p other.
     */
    [[nodiscard]] ImmutableSortedSet<T> Except(const ImmutableSortedSet<T>& other) const {
        auto s = std::make_shared<SetT>();
        for (const auto& x : *data_) if (!other.Contains(x)) s->insert(x);
        return ImmutableSortedSet<T>(std::move(s));
    }

    /**
     * @brief Returns a new set containing only elements present in exactly one of the two sets.
     *
     * C++ counterpart of .NET ImmutableSortedSet<T>.SymmetricExcept(IEnumerable<T>).
     * @param other The other set.
     * @return A new ImmutableSortedSet with the symmetric difference.
     */
    [[nodiscard]] ImmutableSortedSet<T> SymmetricExcept(const ImmutableSortedSet<T>& other) const {
        auto s = std::make_shared<SetT>(*data_);
        for (const auto& x : *other.data_) {
            if (s->count(x)) s->erase(x);
            else s->insert(x);
        }
        return ImmutableSortedSet<T>(std::move(s));
    }

    /**
     * @brief Determines whether this set and @p other contain the same elements.
     *
     * C++ counterpart of .NET ImmutableSortedSet<T>.SetEquals(IEnumerable<T>).
     * @param other The set to compare to.
     * @return true if both sets are equal; otherwise false.
     */
    [[nodiscard]] bool SetEquals(const ImmutableSortedSet<T>& other) const {
        return *data_ == *other.data_;
    }

    /**
     * @brief Determines whether every element of this set is in @p other.
     *
     * C++ counterpart of .NET ImmutableSortedSet<T>.IsSubsetOf(IEnumerable<T>).
     * @param other The set to compare to.
     * @return true if every element of this set is in @p other; otherwise false.
     */
    [[nodiscard]] bool IsSubsetOf(const ImmutableSortedSet<T>& other) const {
        for (const auto& x : *data_) if (!other.Contains(x)) return false;
        return true;
    }

    /**
     * @brief Determines whether every element of @p other is in this set.
     *
     * C++ counterpart of .NET ImmutableSortedSet<T>.IsSupersetOf(IEnumerable<T>).
     * @param other The set to compare to.
     * @return true if every element of @p other is in this set; otherwise false.
     */
    [[nodiscard]] bool IsSupersetOf(const ImmutableSortedSet<T>& other) const {
        return other.IsSubsetOf(*this);
    }

    /**
     * @brief Determines whether this set is a proper (strict) subset of @p other.
     *
     * C++ counterpart of .NET ImmutableSortedSet<T>.IsProperSubsetOf(IEnumerable<T>).
     * @param other The set to compare to.
     * @return true if this set is a proper subset of @p other; otherwise false.
     */
    [[nodiscard]] bool IsProperSubsetOf(const ImmutableSortedSet<T>& other) const {
        return IsSubsetOf(other) && !SetEquals(other);
    }

    /**
     * @brief Determines whether this set is a proper (strict) superset of @p other.
     *
     * C++ counterpart of .NET ImmutableSortedSet<T>.IsProperSupersetOf(IEnumerable<T>).
     * @param other The set to compare to.
     * @return true if this set is a proper superset of @p other; otherwise false.
     */
    [[nodiscard]] bool IsProperSupersetOf(const ImmutableSortedSet<T>& other) const {
        return IsSupersetOf(other) && !SetEquals(other);
    }

    /**
     * @brief Determines whether this set and @p other share any common elements.
     *
     * C++ counterpart of .NET ImmutableSortedSet<T>.Overlaps(IEnumerable<T>).
     * @param other The set to compare to.
     * @return true if at least one element is common; otherwise false.
     */
    [[nodiscard]] bool Overlaps(const ImmutableSortedSet<T>& other) const {
        for (const auto& x : *other.data_) if (Contains(x)) return true;
        return false;
    }

    /** @brief Returns a const iterator to the beginning of the set (STL interop). */
    auto begin() const { return data_->begin(); }
    /** @brief Returns a const iterator past the end of the set (STL interop). */
    auto end()   const { return data_->end(); }
};

} // namespace System::Collections::Immutable
