// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>
#include "System/Collections/Generic/EqualityComparer.hpp"

namespace System::Collections::Generic {

/**
 * @brief A general-purpose equality comparer implementing .NET's default equality.
 *
 * C++ counterpart of .NET System.Collections.Generic.ObjectEqualityComparer<T>.
 * Extends EqualityComparer<T>; used as the fallback equality comparer when no
 * IEquatable<T>-specific comparer is available. Its equality and hash are
 * @c EqualityComparer<T>.Default's, stated once by @c System::detail::equalValues
 * and @c System::detail::hashValue — the built-in `operator==` and `std::hash<T>`
 * for ordinary non-floating types, and `Single.Equals`/`Double.Equals`'s
 * NaN-equals-itself rule with the matching NaN-folding hash for floating
 * primitives and the three direct nullable-floating forms.
 *
 * @tparam T The type of objects to compare; must support operator== and std::hash<T>.
 */
template<typename T>
class ObjectEqualityComparer : public EqualityComparer<T> {
public:
    /** @brief Constructs an ObjectEqualityComparer using the default equality for T. */
    ObjectEqualityComparer() = default;

    /**
     * @brief Determines whether two objects of type T are equal under .NET's
     *        default equality contract.
     *
     * C++ counterpart of .NET ObjectEqualityComparer<T>.Equals(T, T).
     * @param x The first object to compare.
     * @param y The second object to compare.
     * @return true if the two are equal; a NaN is equal to another NaN, and
     *         `+0.0` is still equal to `-0.0`.
     */
    [[nodiscard]] bool Equals(const T& x, const T& y) const override {
        return System::detail::equalValues(x, y);
    }

    /**
     * @brief Returns a hash code for the specified object, consistent with Equals.
     *
     * C++ counterpart of .NET ObjectEqualityComparer<T>.GetHashCode(T).
     * @param obj The object for which to compute the hash code.
     * @return A hash code for @p obj. Every NaN hashes alike, so the
     *         equal-objects-equal-hashes invariant survives the NaN-reflexive
     *         Equals above.
     */
    [[nodiscard]] intcs GetHashCode(const T& obj) const override {
        return static_cast<intcs>(System::detail::hashValue(obj));
    }
};

} // namespace System::Collections::Generic
