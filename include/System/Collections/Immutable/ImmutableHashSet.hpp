// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include <unordered_set>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::Collections::Immutable {

using SharpRuntime::intcs;

/**
 * @brief Represents an immutable unordered set with no duplicate elements.
 *
 * C++ counterpart of .NET System.Collections.Immutable.ImmutableHashSet<T>.
 * Internally shares the underlying unordered_set via shared_ptr; mutations return new instances.
 *
 * @tparam T The type of elements in the set.
 */
template<typename T>
class ImmutableHashSet {
    using SetT = std::unordered_set<T>;
    std::shared_ptr<const SetT> data_;

    explicit ImmutableHashSet(std::shared_ptr<const SetT> data) : data_(std::move(data)) {}

public:
    /** @brief Default-constructs an empty ImmutableHashSet. */
    ImmutableHashSet() : data_(std::make_shared<SetT>()) {}

    /**
     * @brief Returns an empty ImmutableHashSet.
     *
     * C++ counterpart of .NET ImmutableHashSet<T>.Empty.
     * @return An empty ImmutableHashSet<T>.
     */
    static ImmutableHashSet<T> Empty() { return ImmutableHashSet<T>(); }

    /**
     * @brief Creates an ImmutableHashSet from an initializer list.
     *
     * C++ counterpart of .NET ImmutableHashSet.Create<T>(params T[] items).
     * @param items The initial elements.
     * @return A new ImmutableHashSet containing the given elements.
     */
    static ImmutableHashSet<T> Create(std::initializer_list<T> items) {
        return ImmutableHashSet<T>(std::make_shared<SetT>(items));
    }

    /**
     * @brief Creates an ImmutableHashSet from an existing vector.
     *
     * C++ counterpart of .NET ImmutableHashSet.CreateRange<T>(IEnumerable<T>).
     * @param items The initial elements.
     * @return A new ImmutableHashSet containing the given elements.
     */
    static ImmutableHashSet<T> Create(const std::vector<T>& items) {
        return ImmutableHashSet<T>(std::make_shared<SetT>(items.begin(), items.end()));
    }

    /**
     * @brief Gets the number of elements in the set.
     *
     * C++ counterpart of .NET ImmutableHashSet<T>.Count.
     * @return The number of elements.
     */
    [[nodiscard]] intcs getCountProperty() const { return static_cast<intcs>(data_->size()); }

    /**
     * @brief Gets a value indicating whether the set is empty.
     *
     * C++ counterpart of .NET ImmutableHashSet<T>.IsEmpty.
     * @return true if the set contains no elements; otherwise false.
     */
    [[nodiscard]] bool getIsEmptyProperty() const { return data_->empty(); }

    /**
     * @brief Determines whether the set contains the specified element.
     *
     * C++ counterpart of .NET ImmutableHashSet<T>.Contains(T).
     * @param item The element to locate.
     * @return true if found; otherwise false.
     */
    [[nodiscard]] bool Contains(const T& item) const {
        return data_->find(item) != data_->end();
    }

    /**
     * @brief Returns a new set with @p item added.
     *
     * C++ counterpart of .NET ImmutableHashSet<T>.Add(T).
     * @param item The element to add.
     * @return A new ImmutableHashSet with the element added (duplicate is ignored).
     */
    [[nodiscard]] ImmutableHashSet<T> Add(const T& item) const {
        auto s = std::make_shared<SetT>(*data_);
        s->insert(item);
        return ImmutableHashSet<T>(std::move(s));
    }

    /**
     * @brief Returns a new set with @p item removed.
     *
     * C++ counterpart of .NET ImmutableHashSet<T>.Remove(T).
     * @param item The element to remove.
     * @return A new ImmutableHashSet without the specified element.
     */
    [[nodiscard]] ImmutableHashSet<T> Remove(const T& item) const {
        auto s = std::make_shared<SetT>(*data_);
        s->erase(item);
        return ImmutableHashSet<T>(std::move(s));
    }

    /**
     * @brief Returns an empty ImmutableHashSet.
     *
     * C++ counterpart of .NET ImmutableHashSet<T>.Clear().
     * @return An empty ImmutableHashSet<T>.
     */
    [[nodiscard]] ImmutableHashSet<T> Clear() const { return Empty(); }

    /**
     * @brief Returns a new set containing all elements from both this set and @p other.
     *
     * C++ counterpart of .NET ImmutableHashSet<T>.Union(IEnumerable<T>).
     * @param other The set to union with.
     * @return A new ImmutableHashSet containing all elements from both sets.
     */
    [[nodiscard]] ImmutableHashSet<T> Union(const ImmutableHashSet<T>& other) const {
        auto s = std::make_shared<SetT>(*data_);
        for (const auto& x : *other.data_) s->insert(x);
        return ImmutableHashSet<T>(std::move(s));
    }

    /**
     * @brief Returns a new set containing only elements present in both sets.
     *
     * C++ counterpart of .NET ImmutableHashSet<T>.Intersect(IEnumerable<T>).
     * @param other The set to intersect with.
     * @return A new ImmutableHashSet containing only common elements.
     */
    [[nodiscard]] ImmutableHashSet<T> Intersect(const ImmutableHashSet<T>& other) const {
        auto s = std::make_shared<SetT>();
        for (const auto& x : *data_) if (other.Contains(x)) s->insert(x);
        return ImmutableHashSet<T>(std::move(s));
    }

    /**
     * @brief Returns a new set containing elements in this set that are not in @p other.
     *
     * C++ counterpart of .NET ImmutableHashSet<T>.Except(IEnumerable<T>).
     * @param other The set whose elements to exclude.
     * @return A new ImmutableHashSet without the elements from @p other.
     */
    [[nodiscard]] ImmutableHashSet<T> Except(const ImmutableHashSet<T>& other) const {
        auto s = std::make_shared<SetT>();
        for (const auto& x : *data_) if (!other.Contains(x)) s->insert(x);
        return ImmutableHashSet<T>(std::move(s));
    }

    /**
     * @brief Returns a new set containing only elements present in exactly one of the two sets.
     *
     * C++ counterpart of .NET ImmutableHashSet<T>.SymmetricExcept(IEnumerable<T>).
     * @param other The other set.
     * @return A new ImmutableHashSet with the symmetric difference.
     */
    [[nodiscard]] ImmutableHashSet<T> SymmetricExcept(const ImmutableHashSet<T>& other) const {
        auto s = std::make_shared<SetT>(*data_);
        for (const auto& x : *other.data_) {
            if (s->count(x)) s->erase(x);
            else s->insert(x);
        }
        return ImmutableHashSet<T>(std::move(s));
    }

    /**
     * @brief Determines whether this set and @p other contain the same elements.
     *
     * C++ counterpart of .NET ImmutableHashSet<T>.SetEquals(IEnumerable<T>).
     * @param other The set to compare to.
     * @return true if both sets are equal; otherwise false.
     */
    [[nodiscard]] bool SetEquals(const ImmutableHashSet<T>& other) const {
        if (data_->size() != other.data_->size()) return false;
        for (const auto& x : *data_) if (!other.Contains(x)) return false;
        return true;
    }

    /**
     * @brief Determines whether every element of this set is in @p other.
     *
     * C++ counterpart of .NET ImmutableHashSet<T>.IsSubsetOf(IEnumerable<T>).
     * @param other The set to compare to.
     * @return true if every element of this set is in @p other; otherwise false.
     */
    [[nodiscard]] bool IsSubsetOf(const ImmutableHashSet<T>& other) const {
        for (const auto& x : *data_) if (!other.Contains(x)) return false;
        return true;
    }

    /**
     * @brief Determines whether every element of @p other is in this set.
     *
     * C++ counterpart of .NET ImmutableHashSet<T>.IsSupersetOf(IEnumerable<T>).
     * @param other The set to compare to.
     * @return true if every element of @p other is in this set; otherwise false.
     */
    [[nodiscard]] bool IsSupersetOf(const ImmutableHashSet<T>& other) const {
        return other.IsSubsetOf(*this);
    }

    /**
     * @brief Determines whether this set is a proper (strict) subset of @p other.
     *
     * C++ counterpart of .NET ImmutableHashSet<T>.IsProperSubsetOf(IEnumerable<T>).
     * @param other The set to compare to.
     * @return true if this set is a proper subset of @p other; otherwise false.
     */
    [[nodiscard]] bool IsProperSubsetOf(const ImmutableHashSet<T>& other) const {
        return IsSubsetOf(other) && !SetEquals(other);
    }

    /**
     * @brief Determines whether this set is a proper (strict) superset of @p other.
     *
     * C++ counterpart of .NET ImmutableHashSet<T>.IsProperSupersetOf(IEnumerable<T>).
     * @param other The set to compare to.
     * @return true if this set is a proper superset of @p other; otherwise false.
     */
    [[nodiscard]] bool IsProperSupersetOf(const ImmutableHashSet<T>& other) const {
        return IsSupersetOf(other) && !SetEquals(other);
    }

    /**
     * @brief Determines whether this set and @p other share any common elements.
     *
     * C++ counterpart of .NET ImmutableHashSet<T>.Overlaps(IEnumerable<T>).
     * @param other The set to compare to.
     * @return true if at least one element is common; otherwise false.
     */
    [[nodiscard]] bool Overlaps(const ImmutableHashSet<T>& other) const {
        for (const auto& x : *other.data_) if (Contains(x)) return true;
        return false;
    }

    /** @brief Returns a const iterator to the beginning of the set (STL interop). */
    auto begin() const { return data_->begin(); }
    /** @brief Returns a const iterator past the end of the set (STL interop). */
    auto end()   const { return data_->end(); }
};

} // namespace System::Collections::Immutable
