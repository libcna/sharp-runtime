// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

//! @file
//! @brief The 11:5:4 trie lookup over the generated Unicode numeric tables (ticket #2336).

#include <cstdint>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/Globalization/detail/UnicodeNumericTable.hpp"

namespace System::Globalization::detail {

    /**
     * @brief The offset into the numeric value tables for @p codePoint.
     *
     * Transcribed from `CharUnicodeInfo.GetNumericGraphemeTableOffsetNoBoundsChecks`
     * (`CharUnicodeInfo.cs:510-538`). The arithmetic is **identical** to the category trie's,
     * over a **different set of tables** — .NET keeps two 11:5:4 tries because the two properties
     * cluster differently, and this port keeps both for the same reason rather than merging them.
     *
     * @pre @p codePoint is a valid Unicode code point. The callers check.
     */
    [[nodiscard]] inline uint16_t NumericGraphemeTableOffset(uint32_t codePoint) noexcept {
        uint32_t index = kNumericGraphemeLevel1Index[codePoint >> 9];

        const uint32_t level2ByteOffset = (index << 6) + ((codePoint >> 3) & 0b0011'1110u);
        index = static_cast<uint32_t>(kNumericGraphemeLevel2Index[level2ByteOffset])
              | (static_cast<uint32_t>(kNumericGraphemeLevel2Index[level2ByteOffset + 1]) << 8);

        return kNumericGraphemeLevel3Index[(index << 4) + (codePoint & 0x0Fu)];
    }

    /**
     * @brief `Numeric_Type = Decimal` value, or −1.
     *
     * `CharUnicodeInfo.cs:148-153`: the **high** nibble of the `DigitValues` entry, minus one, so
     * that "not a decimal digit" normalises to −1. The minus-one is not a tidy-up — it is how the
     * table encodes absence, and dropping it turns every non-digit into a `0`.
     */
    [[nodiscard]] inline SharpRuntime::intcs LookupDecimalDigitValue(uint32_t codePoint) noexcept {
        return static_cast<SharpRuntime::intcs>(
                   kDigitValues[NumericGraphemeTableOffset(codePoint)] >> 4) - 1;
    }

    /**
     * @brief `Numeric_Type = Decimal or Digit` value, or −1.
     *
     * `CharUnicodeInfo.cs:182-187`: the **low** nibble, minus one. The two nibbles are different
     * properties in the same byte, which is why swapping them is a mutation and not a refactor.
     */
    [[nodiscard]] inline SharpRuntime::intcs LookupDigitValue(uint32_t codePoint) noexcept {
        return static_cast<SharpRuntime::intcs>(
                   kDigitValues[NumericGraphemeTableOffset(codePoint)] & 0x0Fu) - 1;
    }

    /**
     * @brief `Numeric_Value` as a `double`, or −1.
     *
     * `CharUnicodeInfo.cs:260-276`. The rationals really are rationals: U+2153 VULGAR FRACTION ONE
     * THIRD is `0.333…`, which is why the table is doubles and not a digit lookup. The generator
     * decodes .NET's little-endian bytes into `double` literals that round-trip, so no byte
     * assembly or aliasing cast is needed here and the values are bit-identical to .NET's.
     */
    [[nodiscard]] inline double LookupNumericValue(uint32_t codePoint) noexcept {
        return kNumericValues[NumericGraphemeTableOffset(codePoint)];
    }

}  // namespace System::Globalization::detail
