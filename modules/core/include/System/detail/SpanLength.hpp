// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstddef>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentOutOfRangeException.hpp"

namespace System::detail {

using SharpRuntime::intcs;

/**
 * @brief Narrows a container element count to the signed @c intcs length used by
 * Span, ReadOnlySpan, Memory, and ArraySegment, throwing if it exceeds INT32_MAX.
 *
 * A .NET span/memory/segment length is a signed 32-bit @c int, but a @c std::vector
 * can in principle hold more than @c INT32_MAX elements. Without this guard the
 * narrowing @c static_cast<intcs> in the vector constructors would silently produce a
 * negative length — the same defect (from the opposite direction) that a negative
 * pointer-constructor length caused (SR-AUD-043a). Centralizing the check gives the
 * four vector constructors one checked conversion and lets it be unit-tested without
 * allocating an INT32_MAX-element container.
 *
 * @param n         The source element count.
 * @param paramName The name of the offending constructor parameter, for the exception.
 * @return @p n narrowed to @c intcs.
 * @throws System::ArgumentOutOfRangeException if @p n exceeds INT32_MAX.
 */
[[nodiscard]] inline intcs checkedSpanLength(std::size_t n, const char* paramName) {
    if (n > static_cast<std::size_t>(SharpRuntime::INTCS_MAX))
        throw System::ArgumentOutOfRangeException(
            paramName,
            "The source has more elements than the maximum span length (INT32_MAX).");
    return static_cast<intcs>(n);
}

} // namespace System::detail
