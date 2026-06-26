// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Collections/Generic/IReadOnlyCollection.hpp"

namespace System::Collections::Generic {

    /** Represents a read-only abstraction of a set. */
    template<typename T>
    class IReadOnlySet : public IReadOnlyCollection<T> {
    public:
        /** Destroys the set. */
        virtual ~IReadOnlySet() = default;
        /** Returns true if the set contains the specified element. */
        [[nodiscard]] virtual bool Contains(const T& item) const = 0;
        /** Returns true if the set is a subset of the specified collection. */
        [[nodiscard]] virtual bool IsSubsetOf(const IEnumerable<T>& other) const = 0;
        /** Returns true if the set is a superset of the specified collection. */
        [[nodiscard]] virtual bool IsSupersetOf(const IEnumerable<T>& other) const = 0;
        /** Returns true if the set is a proper (strict) subset of the specified collection. */
        [[nodiscard]] virtual bool IsProperSubsetOf(const IEnumerable<T>& other) const = 0;
        /** Returns true if the set is a proper (strict) superset of the specified collection. */
        [[nodiscard]] virtual bool IsProperSupersetOf(const IEnumerable<T>& other) const = 0;
        /** Returns true if the set and the specified collection share at least one common element. */
        [[nodiscard]] virtual bool Overlaps(const IEnumerable<T>& other) const = 0;
        /** Returns true if the set and the specified collection contain the same elements. */
        [[nodiscard]] virtual bool SetEquals(const IEnumerable<T>& other) const = 0;
    };

} // namespace System::Collections::Generic
