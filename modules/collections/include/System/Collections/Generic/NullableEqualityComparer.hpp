// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <optional>
#include <functional>
#include "System/Collections/Generic/EqualityComparer.hpp"
#include "System/detail/ComparisonPolicy.hpp"

namespace System::Collections::Generic {

/**
 * @brief Provides equality comparison for nullable value types (std::optional<T>).
 *
 * C++ counterpart of .NET System.Collections.Generic.NullableEqualityComparer<T>.
 * Extends EqualityComparer<std::optional<T>>: two null values are equal;
 * a null and a non-null value are not equal; two present values are compared by
 * @c EqualityComparer<T>.Default, which .NET's NullableEqualityComparer<T> also
 * delegates to. GetHashCode returns 0 for null, and otherwise the underlying
 * value's default hash.
 *
 * @note This is deliberately NOT the same rule as C#'s *lifted* `==` on `T?`,
 * which is the underlying type's `operator==` and therefore says
 * `(double?)double.NaN == (double?)double.NaN` is false — in .NET as well as
 * here. `System::Nullable<T>::operator==` reproduces the lifted rule and must
 * not change; this comparer is the `Equals`-shaped surface and is
 * NaN-reflexive. See `docs/CollectionsComparisonContractPlan.md` §5.4.
 *
 * @tparam T The underlying value type (must support operator== and std::hash<T>).
 */
template<typename T>
class NullableEqualityComparer : public EqualityComparer<std::optional<T>> {
public:
    /** @brief Constructs a NullableEqualityComparer using EqualityComparer<T>.Default. */
    NullableEqualityComparer() = default;

    /**
     * @brief Determines whether two nullable values are equal.
     *
     * C++ counterpart of .NET NullableEqualityComparer<T>.Equals(T?, T?).
     * Two nullopt values are equal; a nullopt and a value are not equal; two
     * present values are compared by EqualityComparer<T>.Default, so two NaNs
     * are equal.
     * @param x The first nullable value.
     * @param y The second nullable value.
     * @return true if both are null, or both have equal values; otherwise false.
     */
    [[nodiscard]] bool Equals(const std::optional<T>& x, const std::optional<T>& y) const override {
        if (!x.has_value() && !y.has_value()) return true;
        if (!x.has_value() || !y.has_value()) return false;
        return System::detail::equalValues(*x, *y);
    }

    /**
     * @brief Returns a hash code for the specified nullable value.
     *
     * C++ counterpart of .NET NullableEqualityComparer<T>.GetHashCode(T?).
     * Returns 0 for null; otherwise the underlying value's default hash, which
     * folds every NaN to one value so the NaN-reflexive Equals above keeps the
     * equal-objects-equal-hashes invariant.
     * @param obj The nullable value for which to compute the hash code.
     * @return A hash code, or 0 if @p obj is null.
     */
    [[nodiscard]] intcs GetHashCode(const std::optional<T>& obj) const override {
        if (!obj.has_value()) return 0;
        return static_cast<intcs>(System::detail::hashValue(*obj));
    }
};

} // namespace System::Collections::Generic
