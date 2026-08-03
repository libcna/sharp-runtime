// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstddef>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"

/**
 * @file
 * @brief The one argument policy every raw `(data, index, count)` decode entry in
 *        `System::Text` validates with (ticket #2007, SR-AUD-286, cause T-A of
 *        `docs/SystemTextNamespaceReviewPlan.md`).
 *
 * Seven public decode entries take a bare pointer plus two **signed** lengths, and before
 * #2007 exactly one of them — `UTF7Encoding::GetString` — validated all three. The other six
 * failed in four different ways for the same malformed metadata: an out-of-bounds read
 * (`ASCIIEncoding`, `Latin1Encoding`, the `Encoding` base), a null dereference
 * (`Latin1Encoding`, `UnicodeEncoding`, `UTF32Encoding`), a `std::length_error` escaping a
 * `System`-shaped API (`Latin1Encoding`), and a signed-overflow `index + count`
 * (`UnicodeEncoding`). They failed differently because there was no policy, not because six
 * authors made the same mistake.
 *
 * The policy below is **transcribed from this repository**, not recalled from .NET: it is
 * `UTF7Encoding::GetString`'s own check order, exception types and parameter names, which is
 * why #2007 needed no reference tree (`docs/SystemTextNamespaceReviewPlan.md` §7). Note the
 * ordering in particular — `count == 0` is answered **before** `data` is examined, so
 * `GetString(nullptr, 0, 0)` returns empty rather than throwing, exactly as every entry did
 * before.
 *
 * What this policy deliberately **cannot** check: a raw pointer carries no length, so an
 * `index`/`count` inside the signed domain but outside the caller's buffer is undetectable
 * here. That is inherent to the signature, is stated in each entry's doc-comment, and is not
 * something a validator can fix.
 */
namespace System::Text::detail {

/**
 * @brief Validates the raw decode triple shared by every `Encoding::GetString` override.
 *
 * @param data      The caller's byte buffer; may be null only when @p count is zero.
 * @param index     Zero-based start offset within @p data.
 * @param count     Number of bytes to read.
 * @param dataParamName Parameter name reported for a null buffer.
 * @return `true` when there is something to decode, `false` when @p count is zero.
 * @throws System::ArgumentOutOfRangeException if @p index or @p count is negative.
 * @throws System::ArgumentNullException if @p data is null and @p count is positive.
 */
[[nodiscard]] inline bool validateRawDecodeRange(const SharpRuntime::bytecs* data,
                                                 SharpRuntime::intcs index,
                                                 SharpRuntime::intcs count,
                                                 const char* dataParamName = "data") {
    if (index < 0) {
        throw System::ArgumentOutOfRangeException("index");
    }
    if (count < 0) {
        throw System::ArgumentOutOfRangeException("count");
    }
    if (count == 0) {
        return false;
    }
    if (data == nullptr) {
        throw System::ArgumentNullException(dataParamName);
    }
    return true;
}

/**
 * @brief The validated range as unsigned offsets.
 *
 * `begin + length` is computed in `std::size_t` on purpose: after validation both operands
 * are non-negative, but `index + count` on `SharpRuntime::intcs` is still signed overflow at
 * `INTCS_MAX` — the defect `UnicodeEncoding::GetString` carried, reproduced as a
 * segmentation fault in `build-probe/2006_probe1_before.log`. Widening removes it by
 * construction on every 64-bit `std::size_t` target.
 */
struct RawDecodeRange {
    std::size_t begin = 0;  ///< First byte offset to read.
    std::size_t end = 0;    ///< One past the last byte offset to read.

    /** @return true when the range is non-empty. */
    [[nodiscard]] constexpr bool any() const { return begin < end; }
};

/**
 * @brief Validates the triple and returns it as an unsigned, overflow-free range.
 * @see validateRawDecodeRange for the exceptions raised.
 */
[[nodiscard]] inline RawDecodeRange checkedRawDecodeRange(const SharpRuntime::bytecs* data,
                                                          SharpRuntime::intcs index,
                                                          SharpRuntime::intcs count,
                                                          const char* dataParamName = "data") {
    if (!validateRawDecodeRange(data, index, count, dataParamName)) {
        return {};
    }
    RawDecodeRange range;
    range.begin = static_cast<std::size_t>(index);
    range.end = range.begin + static_cast<std::size_t>(count);
    return range;
}

} // namespace System::Text::detail
