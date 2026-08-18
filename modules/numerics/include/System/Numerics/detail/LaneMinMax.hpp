// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cmath>

namespace System::Numerics::detail {

    /**
     * @brief One lane of .NET's vector `Max`, transcribed.
     *
     * Ticket #2174 (2026-08-18). The three vector types used `std::max`, whose NaN behaviour is
     * asymmetric: `std::max(a, b)` returns `a < b ? b : a`, so a NaN in `a` propagates and a NaN
     * in `b` is **silently discarded**. Measured before the repair,
     * `Vector3::Max({0,1,1}, {NaN,1,1})` was `{0,1,1}` — the NaN vanished.
     *
     * .NET's is
     * @code
     * ConditionalSelect(LessThan(y, x) | IsNaN(x) | (Equals(x, y) & IsNegative(y)), x, y)
     * @endcode
     * (`VectorMath.cs:1512-1524`). Read it for each operand in turn:
     *
     *   - `x` is NaN → `IsNaN(x)` is true → `x` is selected, so NaN is returned;
     *   - `y` is NaN → `LessThan(y, x)` is false, `IsNaN(x)` is false, `Equals(x, y)` is false →
     *     `y` is selected, so NaN is returned.
     *
     * **NaN propagates from either side**, and the second row is the one that is easy to miss,
     * because nothing in .NET's expression mentions `IsNaN(y)`.
     *
     * The third disjunct is the signed-zero rule: `Max(+0, -0)` is `+0` and `Max(-0, +0)` is `+0`,
     * which `std::max` also gets wrong (it returns whichever operand it was handed second).
     */
    [[nodiscard]] inline float LaneMax(float x, float y) noexcept {
        const bool selectX = (y < x) || std::isnan(x) || (x == y && std::signbit(y));
        return selectX ? x : y;
    }

    /**
     * @brief One lane of .NET's vector `Min`, transcribed.
     *
     * The mirror of LaneMax: `LessThan(x, y) | IsNaN(x) | (Equals(x, y) & IsNegative(x))`
     * (`VectorMath.cs:1598-1610`). Note the signed-zero disjunct tests `IsNegative(x)` here where
     * `Max` tests `IsNegative(y)`, so `Min(+0, -0)` is `-0` — the mirror is not a copy.
     */
    [[nodiscard]] inline float LaneMin(float x, float y) noexcept {
        const bool selectX = (x < y) || std::isnan(x) || (x == y && std::signbit(x));
        return selectX ? x : y;
    }

} // namespace System::Numerics::detail
