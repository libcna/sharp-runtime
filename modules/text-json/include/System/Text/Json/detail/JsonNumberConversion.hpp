// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstdint>
#include <limits>
#include <type_traits>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "nlohmann/json.hpp"

namespace System::Text::Json::detail {

/**
 * @brief The single rule this module uses to convert a JSON number to an integral type.
 *
 * Ticket #2114 (SR-AUD-328, cause TJ-F, `docs/SystemTextJsonNamespaceReviewPlan.md` §6.3).
 * `JsonElement` already implemented this rule correctly and carried a comment explaining why;
 * `JsonValue` and `JsonSerializer::Deserialize<T>` each reached `nlohmann`'s `get<T>()` directly
 * and therefore
 *
 *   - **silently truncated** a JSON float literal (`1.5` → `1`, `2e1` → `20`, `1.0` → `1`),
 *   - **silently wrapped** an out-of-`T`-range integer (`2147483648` → `-2147483648`), and
 *   - executed **undefined behaviour** for a value outside the destination's range, because
 *     `get<int>()` on a `number_float` is a bare `static_cast<int>(double)`. UBSan's
 *     `float-cast-overflow` reports it at `vendor/nlohmann/json.hpp:4279`
 *     (`build-probe/2114_probe1_fcorec.log`) for `99999999999999999999`, `1e100` and NaN.
 *
 * Extracting the rule here — rather than copying `JsonElement`'s body a second and third time —
 * is deliberate: a duplicated rule that drifts apart is exactly cause **TJ-E**, the defect
 * #2113 had just repaired one file away.
 *
 * **A JSON float literal never converts, regardless of its numeric value.** That is not an
 * arithmetic decision but a grammar one, and it is the behaviour `JsonElement` already recorded
 * as verified against .NET's `JsonDocument.cs`: real .NET parses the original number *text* with
 * `Utf8Parser.TryParse<int>`, which fails outright on `1.0` or `2e1` and never round-trips
 * through a floating-point intermediate.
 *
 * @param number A JSON value; a non-number always fails rather than being coerced.
 * @param value Receives the converted value on success, and is zeroed on failure.
 * @return true if @p number is an integer literal exactly representable in @p TInt.
 */
template <typename TInt>
inline bool TryConvertJsonNumberToIntegral(const nlohmann::ordered_json& number, TInt& value) {
    static_assert(std::is_integral_v<TInt> && !std::is_same_v<TInt, bool>,
                  "TryConvertJsonNumberToIntegral converts to a non-bool integral type");
    value = 0;
    if (!number.is_number()) return false;
    if (number.is_number_float()) return false;

    if (number.is_number_unsigned()) {
        const auto u = number.template get<std::uint64_t>();
        if (u > static_cast<std::uint64_t>(std::numeric_limits<TInt>::max())) return false;
        value = static_cast<TInt>(u);
        return true;
    }

    const auto i = number.template get<std::int64_t>();
    if constexpr (std::is_signed_v<TInt>) {
        if (i < static_cast<std::int64_t>(std::numeric_limits<TInt>::min()) ||
            i > static_cast<std::int64_t>(std::numeric_limits<TInt>::max()))
            return false;
    } else {
        if (i < 0) return false;
        if (static_cast<std::uint64_t>(i) > static_cast<std::uint64_t>(std::numeric_limits<TInt>::max()))
            return false;
    }
    value = static_cast<TInt>(i);
    return true;
}

} // namespace System::Text::Json::detail
