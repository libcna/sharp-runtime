// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

//! @file
//! @brief The 11:5:4 trie lookup over the generated Unicode table (ticket #2315).

#include <cstdint>

#include "System/Globalization/UnicodeCategory.hpp"
#include "System/Globalization/detail/UnicodeCategoryTable.hpp"

namespace System::Globalization::detail {

    /**
     * @brief The byte offset into `kCategoriesValues` for @p codePoint.
     *
     * Transcribed from `CharUnicodeInfo.GetCategoryCasingTableOffsetNoBoundsChecks`
     * (`CharUnicodeInfo.cs:437-467`). The three shifts and masks are .NET's exactly, including
     * that the level-2 index is a **byte** offset into a table read as `uint16_t` — hence
     * `(index << 6) + ((codePoint >> 3) & 0b0011'1110)` rather than an element index.
     *
     * The layout is .NET's rather than something better packed, deliberately: a different
     * packing would be a second table to keep correct, and SA-4's whole point is that there is
     * one source of record.
     *
     * @pre @p codePoint is a valid Unicode code point (`<= 0x10FFFF`). The callers check.
     */
    [[nodiscard]] inline uint8_t CategoryCasingTableOffset(uint32_t codePoint) noexcept {
        uint32_t index = kCategoryCasingLevel1Index[codePoint >> 9];

        const uint32_t level2ByteOffset = (index << 6) + ((codePoint >> 3) & 0b0011'1110u);
        // Read the little-endian uint16 the generator wrote as two bytes. Assembling it from
        // its bytes rather than reinterpret_cast'ing keeps this correct on a big-endian host,
        // where .NET calls ReverseEndianness for the same reason.
        index = static_cast<uint32_t>(kCategoryCasingLevel2Index[level2ByteOffset])
              | (static_cast<uint32_t>(kCategoryCasingLevel2Index[level2ByteOffset + 1]) << 8);

        return kCategoryCasingLevel3Index[(index << 4) + (codePoint & 0x0Fu)];
    }

    /**
     * @brief The Unicode general category of @p codePoint, at UCD 16.0.
     *
     * `CharUnicodeInfo.cs:421`: the low five bits of the values entry. Bits 5-6 hold the strong
     * bidi class and bit 7 marks white space; both are masked off here, and neither is exposed —
     * this port has no `GetBidiCategory` and its `IsWhiteSpace` predates the table.
     */
    [[nodiscard]] inline UnicodeCategory LookupUnicodeCategory(uint32_t codePoint) noexcept {
        return static_cast<UnicodeCategory>(kCategoriesValues[CategoryCasingTableOffset(codePoint)]
                                            & 0x1Fu);
    }

}  // namespace System::Globalization::detail
