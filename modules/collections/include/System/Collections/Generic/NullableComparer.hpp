// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <optional>
#include "System/Collections/Generic/Comparer.hpp"
#include "System/detail/ComparisonPolicy.hpp"

namespace System::Collections::Generic {

/**
 * @brief Provides comparison for nullable value types (std::optional<T>).
 *
 * C++ counterpart of .NET System.Collections.Generic.NullableComparer<T>.
 * Extends Comparer<std::optional<T>>: a null value (std::nullopt) is considered
 * less than any non-null value; two null values are considered equal; and two
 * present values are ordered by @c Comparer<T>.Default, which .NET's
 * NullableComparer<T> also delegates to. That default is stated once by
 * @c System::detail::compareValues, so a `std::optional<float>` holding NaN
 * orders before every other value instead of comparing equivalent to all of
 * them.
 *
 * @tparam T The underlying value type (must be comparable via operator<).
 */
template<typename T>
class NullableComparer : public Comparer<std::optional<T>> {
public:
    /** @brief Constructs a NullableComparer using the default ordering for T. */
    NullableComparer() = default;

    /**
     * @brief Compares two nullable values.
     *
     * C++ counterpart of .NET NullableComparer<T>.Compare(T?, T?).
     * Null sorts before any non-null value; two present values are ordered by
     * Comparer<T>.Default.
     * @param x The first nullable value.
     * @param y The second nullable value.
     * @return Negative if @p x sorts before @p y, zero if equivalent, positive otherwise.
     */
    [[nodiscard]] intcs Compare(const std::optional<T>& x, const std::optional<T>& y) const override {
        if (!x.has_value() && !y.has_value()) return 0;
        if (!x.has_value()) return -1;
        if (!y.has_value()) return  1;
        return static_cast<intcs>(System::detail::compareValues(*x, *y));
    }
};

} // namespace System::Collections::Generic
