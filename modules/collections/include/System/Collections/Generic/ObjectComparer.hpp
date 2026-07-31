// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Collections/Generic/Comparer.hpp"

namespace System::Collections::Generic {

/**
 * @brief A general-purpose comparer implementing .NET's default ordering.
 *
 * C++ counterpart of .NET System.Collections.Generic.ObjectComparer<T>.
 * Extends Comparer<T>; used as the fallback comparer when no IComparable<T>-specific
 * comparer is available. Its ordering is @c Comparer<T>.Default's, stated once by
 * @c System::detail::compareValues — the built-in relational operators for every
 * type but `float` and `double`, and `Single.CompareTo`/`Double.CompareTo`'s
 * NaN-orders-first rule for those two.
 *
 * @tparam T The type of objects to compare; must support operator<.
 */
template<typename T>
class ObjectComparer : public Comparer<T> {
public:
    /** @brief Constructs an ObjectComparer using the default ordering for T. */
    ObjectComparer() = default;

    /**
     * @brief Compares two objects of type T under .NET's default ordering.
     *
     * C++ counterpart of .NET ObjectComparer<T>.Compare(T, T).
     * @param x The first object to compare.
     * @param y The second object to compare.
     * @return Negative if @p x sorts before @p y, zero if equivalent, positive otherwise.
     *         NaN sorts before every value including negative infinity, and two NaNs
     *         are equivalent.
     */
    [[nodiscard]] intcs Compare(const T& x, const T& y) const override {
        return static_cast<intcs>(System::detail::compareValues(x, y));
    }
};

} // namespace System::Collections::Generic
