// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Collections/Generic/ICollection.hpp"

namespace System::Collections::Generic {

    /// Provides the base interface for the abstraction of sets.
    template<typename T>
    class ISet : public ICollection<T> {
    public:
        /// Destroys the set.
        virtual ~ISet() = default;
        /// Adds an element to the set; returns true if the element was added.
        virtual bool Add(const T& item) = 0;
        /// Modifies the set to contain all elements in itself and the specified collection.
        virtual void UnionWith(const IEnumerable<T>& other) = 0;
        /// Modifies the set to contain only elements also present in the specified collection.
        virtual void IntersectWith(const IEnumerable<T>& other) = 0;
        /// Removes all elements in the specified collection from the set.
        virtual void ExceptWith(const IEnumerable<T>& other) = 0;
        /// Modifies the set to contain only elements present in either itself or the specified collection, but not both.
        virtual void SymmetricExceptWith(const IEnumerable<T>& other) = 0;
        /// Returns true if the set is a subset of the specified collection.
        [[nodiscard]] virtual bool IsSubsetOf(const IEnumerable<T>& other) const = 0;
        /// Returns true if the set is a superset of the specified collection.
        [[nodiscard]] virtual bool IsSupersetOf(const IEnumerable<T>& other) const = 0;
        /// Returns true if the set is a proper (strict) subset of the specified collection.
        [[nodiscard]] virtual bool IsProperSubsetOf(const IEnumerable<T>& other) const = 0;
        /// Returns true if the set is a proper (strict) superset of the specified collection.
        [[nodiscard]] virtual bool IsProperSupersetOf(const IEnumerable<T>& other) const = 0;
        /// Returns true if the set and the specified collection share at least one common element.
        [[nodiscard]] virtual bool Overlaps(const IEnumerable<T>& other) const = 0;
        /// Returns true if the set contains exactly the same elements as the specified collection.
        [[nodiscard]] virtual bool SetEquals(const IEnumerable<T>& other) const = 0;
    };

} // namespace System::Collections::Generic
