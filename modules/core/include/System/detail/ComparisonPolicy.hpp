// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <iterator>
#include <limits>
#include <type_traits>
#include <utility>

namespace System::detail {

/**
 * @file ComparisonPolicy.hpp
 * @brief The port's single statement of .NET's default comparison and equality
 *        contract — the C++ counterpart of @c Comparer<T>.Default and
 *        @c EqualityComparer<T>.Default.
 *
 * A .NET expression such as `Array.Sort(array)` or `list.Contains(value)` does
 * not use the operand type's `<` or `==`; it uses `Comparer<T>.Default` /
 * `EqualityComparer<T>.Default`, which dispatch to `IComparable<T>.CompareTo`
 * and `IEquatable<T>.Equals`. For every type in this port except the two IEEE
 * binary floating types those agree exactly with the built-in C++ operators,
 * which is why porting them as `<` and `==` was invisible for the whole life of
 * these files. For `float` and `double` they differ in exactly two places:
 *
 *  - **Ordering.** `Single.CompareTo` / `Double.CompareTo` place NaN *below*
 *    every value including negative infinity, and treat two NaNs as equal
 *    (`Single.cs:274-296`). Raw `<` makes NaN incomparable with everything.
 *  - **Equality.** `Single.Equals` / `Double.Equals` are
 *    `obj == m_value || (IsNaN(obj) && IsNaN(m_value))` (`Single.cs:329-336`),
 *    so NaN equals itself. Raw `==` says it does not.
 *
 * Raw `<` over a range that contains NaN is not merely a different answer: it
 * is **not a strict weak ordering**, because NaN is "equivalent" to every
 * finite value while finite values order among themselves, so equivalence is
 * not transitive. Passing such a predicate to `std::sort` violates
 * `[alg.sort]`'s precondition. Measured on 2026-07-31 against the then-shipped
 * `Array::Sort`, that left the **non-NaN** elements unsorted in 64 of 196
 * size/density/placement shapes, worst case 3,874 inversions in 65,536
 * elements — with no diagnostic from AddressSanitizer, UndefinedBehaviorSanitizer
 * or `_GLIBCXX_DEBUG` (libstdc++'s debug mode checks irreflexivity, which NaN
 * satisfies, and nothing about transitivity of equivalence). See
 * `docs/ComparisonContractPlan.md`.
 *
 * **`operator==` on a raw primitive is deliberately not covered here.** C#'s
 * *lifted* `==` on `T?` is the underlying type's `operator ==`, so
 * `(double?)double.NaN == (double?)double.NaN` is `false` in .NET too. Only
 * the `Equals`-shaped surfaces are NaN-reflexive; see `Nullable<T>::operator==`.
 *
 * Every entry point below is `if constexpr`-gated on
 * `std::is_floating_point_v<T>`, so for every non-floating instantiation it
 * generates the built-in operation and nothing else.
 */

/**
 * @brief Counterpart of .NET @c Comparer<T>.Default.Compare.
 *
 * For IEEE binary floating types this is @c Single.CompareTo /
 * @c Double.CompareTo: NaN orders before every value including negative
 * infinity, and two NaNs compare equal. For every other @p T it is the
 * built-in relational operators, unchanged.
 *
 * @return A negative value if @p a sorts before @p b, zero if they are
 *         equivalent, a positive value if @p a sorts after @p b.
 */
template<typename T>
[[nodiscard]] constexpr int compareValues(const T& a, const T& b) {
    if constexpr (std::is_floating_point_v<T>) {
        // Mirrors Single.cs:274-296 / Double.cs's CompareTo exactly, including
        // its ordering of the three tests.
        if (a < b) return -1;
        if (a > b) return  1;
        if (a == b) return 0;
        // At least one operand is NaN.
        if (std::isnan(a)) return std::isnan(b) ? 0 : -1;
        return 1;
    } else {
        if (a < b) return -1;
        if (b < a) return  1;
        return 0;
    }
}

/**
 * @brief Counterpart of .NET @c EqualityComparer<T>.Default.Equals.
 *
 * For IEEE binary floating types this is @c Single.Equals / @c Double.Equals:
 * `a == b || (isnan(a) && isnan(b))`, so NaN equals itself and `+0.0` still
 * equals `-0.0`. For every other @p T it is the built-in `==`, unchanged.
 */
template<typename T>
[[nodiscard]] constexpr bool equalValues(const T& a, const T& b)
        noexcept(noexcept(a == b)) {
    if constexpr (std::is_floating_point_v<T>) {
        return a == b || (std::isnan(a) && std::isnan(b));
    } else {
        return a == b;
    }
}

/**
 * @brief Counterpart of .NET @c Single.GetHashCode / @c Double.GetHashCode's
 *        NaN and signed-zero normalisation.
 *
 * Once @ref equalValues treats NaN as equal to itself, a hash that depends on
 * NaN's payload bits would break the equal-objects-equal-hashes invariant:
 * libstdc++'s `std::hash<double>` already folds `+0.0` and `-0.0` to `0` but
 * hashes NaN's bit pattern, so two NaNs with different payloads hash
 * differently. .NET folds both cases together
 * (`Single.GetHashCode`: `bits &= 0x7F800000` when the value is NaN or zero).
 * This helper reproduces that by mapping every NaN to one canonical value
 * before hashing. For every other @p T it is `std::hash<T>`, unchanged.
 */
template<typename T>
[[nodiscard]] std::size_t hashValue(const T& v) {
    if constexpr (std::is_floating_point_v<T>) {
        // One canonical NaN, so every NaN hashes alike; std::hash already maps
        // -0.0 to the same value as +0.0.
        if (std::isnan(v)) return std::hash<T>{}(std::numeric_limits<T>::quiet_NaN());
        return std::hash<T>{}(v);
    } else {
        return std::hash<T>{}(v);
    }
}

/**
 * @brief A strict weak ordering built from @ref compareValues.
 *
 * Safe to hand to `std::sort` / `std::stable_sort` / `std::lower_bound` even
 * when the range contains NaN, because the induced equivalence is transitive:
 * every NaN is equivalent only to another NaN, and orders below every number.
 * For non-floating @p T this is exactly `a < b`.
 *
 * Use this where the ordered values are not the ones being permuted — a
 * `keySelector`'s results, for example. Where the range itself is the
 * floating data, prefer @ref moveNaNsToFront followed by an ordinary sort of
 * the remainder, which is what .NET's own `ArraySortHelper` does.
 */
template<typename T>
struct DefaultLess {
    [[nodiscard]] constexpr bool operator()(const T& a, const T& b) const {
        if constexpr (std::is_floating_point_v<T>) {
            return compareValues(a, b) < 0;
        } else {
            return a < b;
        }
    }
};

/** @brief The reverse of @ref DefaultLess, for descending orderings. */
template<typename T>
struct DefaultGreater {
    [[nodiscard]] constexpr bool operator()(const T& a, const T& b) const {
        if constexpr (std::is_floating_point_v<T>) {
            return compareValues(a, b) > 0;
        } else {
            return b < a;
        }
    }
};

/**
 * @brief Counterpart of .NET's @c SortUtils.MoveNansToFront
 *        (`ArraySortHelper.cs:1111-1139`).
 *
 * Moves every NaN in `[first, last)` to the front of the range in one forward
 * pass and returns how many were moved, so a caller can sort only
 * `[first + moved, last)` — a range that provably contains no NaN and over
 * which `operator<` therefore *is* a strict weak ordering.
 *
 * .NET does this for performance ("so that we can do an optimized comparison
 * as part of the actual sort on the remainder of the values"); this port does
 * it for the same reason *and* because it makes the precondition violation
 * structurally unreachable rather than repaired by a hand-written predicate.
 *
 * For a non-floating value type this is a no-op that returns 0, and the
 * compiler removes it entirely.
 *
 * @return The number of NaN elements moved to the front.
 */
template<typename It>
[[nodiscard]] std::size_t moveNaNsToFront(It first, It last) {
    using Value = typename std::iterator_traits<It>::value_type;
    if constexpr (std::is_floating_point_v<Value>) {
        std::size_t moved = 0;
        It left = first;
        for (It i = first; i != last; ++i) {
            if (std::isnan(*i)) {
                std::iter_swap(left, i);
                ++left;
                ++moved;
            }
        }
        return moved;
    } else {
        (void)first;
        (void)last;
        return 0;
    }
}

/**
 * @brief Sorts `[first, last)` under .NET's default comparison contract.
 *
 * The NaN pre-pass of @ref moveNaNsToFront followed by an ordinary
 * `std::sort` of the NaN-free remainder — the algorithm
 * `ArraySortHelper<T>.Sort` uses for `float`, `double` and `Half`
 * (`ArraySortHelper.cs:285-305`). For a non-floating value type it is exactly
 * `std::sort(first, last)`.
 */
template<typename It>
void defaultSort(It first, It last) {
    using Value = typename std::iterator_traits<It>::value_type;
    if constexpr (std::is_floating_point_v<Value>) {
        const std::size_t nanCount = moveNaNsToFront(first, last);
        std::sort(std::next(first, static_cast<typename std::iterator_traits<It>::difference_type>(nanCount)),
                  last);
    } else {
        std::sort(first, last);
    }
}

} // namespace System::detail
