// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <unordered_set>
#include <vector>

namespace System::Collections::Generic {

/**
 * @brief Represents a set of values with no duplicate elements.
 *
 * C++ counterpart of .NET System.Collections.Generic.HashSet<T>.
 * Backed by std::unordered_set; provides O(1) average-case Add, Remove, and Contains.
 *
 * @tparam T The type of elements in the set.
 */
template<typename T>
class HashSet {
    std::unordered_set<T> set_;

public:
    /**
     * @brief Initializes a new empty HashSet.
     *
     * C++ counterpart of .NET HashSet<T>().
     */
    HashSet() = default;

    /**
     * @brief Adds the specified element to the set.
     *
     * C++ counterpart of .NET HashSet<T>.Add(T).
     * @param item The element to add.
     * @return true if the element was added; false if it was already present.
     */
    bool Add(const T& item) { return set_.insert(item).second; }

    /**
     * @brief Adds the specified element to the set (move overload).
     *
     * C++ counterpart of .NET HashSet<T>.Add(T).
     * @param item The element to add (moved).
     * @return true if the element was added; false if it was already present.
     */
    bool Add(T&& item) { return set_.insert(std::move(item)).second; }

    /**
     * @brief Removes the specified element from the set.
     *
     * C++ counterpart of .NET HashSet<T>.Remove(T).
     * @param item The element to remove.
     * @return true if the element was found and removed; otherwise false.
     */
    bool Remove(const T& item) { return set_.erase(item) > 0; }

    /**
     * @brief Determines whether the set contains the specified element.
     *
     * C++ counterpart of .NET HashSet<T>.Contains(T).
     * @param item The element to locate.
     * @return true if found; otherwise false.
     */
    [[nodiscard]] bool Contains(const T& item) const { return set_.count(item) > 0; }

    /**
     * @brief Gets the number of elements contained in the set.
     *
     * C++ counterpart of .NET HashSet<T>.Count.
     */
    [[nodiscard]] int getCountProperty() const { return static_cast<int>(set_.size()); }

    /**
     * @brief Removes all elements from the set.
     *
     * C++ counterpart of .NET HashSet<T>.Clear().
     */
    void Clear() { set_.clear(); }

    /**
     * @brief Modifies the set to contain all elements present in itself or the specified collection.
     *
     * C++ counterpart of .NET HashSet<T>.UnionWith(IEnumerable<T>).
     * @param other The collection to union with.
     */
    void UnionWith(const HashSet<T>& other) {
        for (const auto& item : other.set_) set_.insert(item);
    }

    /**
     * @brief Modifies the set to contain only elements also present in the specified collection.
     *
     * C++ counterpart of .NET HashSet<T>.IntersectWith(IEnumerable<T>).
     * @param other The collection to intersect with.
     */
    void IntersectWith(const HashSet<T>& other) {
        for (auto it = set_.begin(); it != set_.end(); ) {
            it = other.Contains(*it) ? ++it : set_.erase(it);
        }
    }

    /**
     * @brief Removes all elements that are present in the specified collection.
     *
     * C++ counterpart of .NET HashSet<T>.ExceptWith(IEnumerable<T>).
     * @param other The collection of elements to remove.
     */
    void ExceptWith(const HashSet<T>& other) {
        for (const auto& item : other.set_) set_.erase(item);
    }

    /**
     * @brief Modifies the set to contain only elements present in exactly one of the two sets.
     *
     * C++ counterpart of .NET HashSet<T>.SymmetricExceptWith(IEnumerable<T>).
     * @param other The other set.
     */
    void SymmetricExceptWith(const HashSet<T>& other) {
        std::vector<T> toAdd;
        for (const auto& item : other.set_) {
            if (!Contains(item)) toAdd.push_back(item);
            else set_.erase(item);
        }
        for (const auto& item : toAdd) set_.insert(item);
    }

    /**
     * @brief Determines whether this set and the specified collection contain exactly the same elements.
     *
     * C++ counterpart of .NET HashSet<T>.SetEquals(IEnumerable<T>).
     * @param other The collection to compare to.
     * @return true if both sets contain the same elements; otherwise false.
     */
    [[nodiscard]] bool SetEquals(const HashSet<T>& other) const {
        if (set_.size() != other.set_.size()) return false;
        for (const auto& item : other.set_)
            if (!Contains(item)) return false;
        return true;
    }

    /**
     * @brief Determines whether the set is a subset of the specified collection.
     *
     * C++ counterpart of .NET HashSet<T>.IsSubsetOf(IEnumerable<T>).
     * @param other The collection to check against.
     * @return true if every element of this set is in other; otherwise false.
     */
    [[nodiscard]] bool IsSubsetOf(const HashSet<T>& other) const {
        for (const auto& item : set_)
            if (!other.Contains(item)) return false;
        return true;
    }

    /**
     * @brief Determines whether the set is a superset of the specified collection.
     *
     * C++ counterpart of .NET HashSet<T>.IsSupersetOf(IEnumerable<T>).
     * @param other The collection to check against.
     * @return true if every element of other is in this set; otherwise false.
     */
    [[nodiscard]] bool IsSupersetOf(const HashSet<T>& other) const {
        return other.IsSubsetOf(*this);
    }

    /**
     * @brief Determines whether the set is a proper (strict) subset of the specified collection.
     *
     * C++ counterpart of .NET HashSet<T>.IsProperSubsetOf(IEnumerable<T>).
     * @param other The collection to check against.
     * @return true if this set is a subset of other and not equal to it; otherwise false.
     */
    [[nodiscard]] bool IsProperSubsetOf(const HashSet<T>& other) const {
        return IsSubsetOf(other) && !SetEquals(other);
    }

    /**
     * @brief Determines whether the set is a proper (strict) superset of the specified collection.
     *
     * C++ counterpart of .NET HashSet<T>.IsProperSupersetOf(IEnumerable<T>).
     * @param other The collection to check against.
     * @return true if this set is a superset of other and not equal to it; otherwise false.
     */
    [[nodiscard]] bool IsProperSupersetOf(const HashSet<T>& other) const {
        return IsSupersetOf(other) && !SetEquals(other);
    }

    /**
     * @brief Copies all elements to a new vector.
     *
     * C++ counterpart of .NET HashSet<T>.CopyTo(T[]).
     * @return A new vector containing all elements.
     */
    [[nodiscard]] std::vector<T> ToArray() const {
        return std::vector<T>(set_.begin(), set_.end());
    }

    /**
     * @brief Ensures the bucket count supports at least capacity elements without rehashing.
     *
     * C++ counterpart of .NET HashSet<T>.EnsureCapacity(int).
     * @param capacity The minimum number of elements the set should support.
     */
    void EnsureCapacity(int capacity) { set_.reserve(static_cast<std::size_t>(capacity)); }

    /**
     * @brief Reduces memory usage by shrinking the bucket array to fit the current element count.
     *
     * C++ counterpart of .NET HashSet<T>.TrimExcess().
     */
    void TrimExcess() { set_.rehash(set_.size()); }

    /** @brief Returns an iterator to the first element (for range-based for). */
    auto begin()       { return set_.begin(); }
    /** @brief Returns an iterator past the last element (for range-based for). */
    auto end()         { return set_.end(); }
    /** @brief Returns a const iterator to the first element (for range-based for). */
    [[nodiscard]] auto begin() const { return set_.begin(); }
    /** @brief Returns a const iterator past the last element (for range-based for). */
    [[nodiscard]] auto end()   const { return set_.end(); }
};

} // namespace System::Collections::Generic
