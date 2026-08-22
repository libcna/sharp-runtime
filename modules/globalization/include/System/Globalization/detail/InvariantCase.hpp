// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "System/Globalization/detail/UnicodeCategoryLookup.hpp"
#include "System/detail/Utf8Scalar.hpp"

namespace System::Globalization::detail {

enum class InvariantCaseMapping { Lower, Upper };

/** A decoded/folded scalar stream plus each scalar's byte offset in the original UTF-8 text. */
struct InvariantScalarSequence {
    /** Decoded scalars after invariant simple-case folding. */
    std::vector<std::uint32_t> scalars;
    /** Byte offset of each scalar in the original UTF-8 input. */
    std::vector<std::size_t> byteOffsets;
};

[[nodiscard]] inline std::uint32_t MapInvariantCase(std::uint32_t codePoint,
                                                    InvariantCaseMapping mapping) noexcept {
    return mapping == InvariantCaseMapping::Upper
        ? LookupToUpperInvariant(codePoint)
        : LookupToLowerInvariant(codePoint);
}

/**
 * Decodes UTF-8 and applies invariant simple uppercase, the scalar folding used by this port's
 * OrdinalIgnoreCase subset. Ill-formed bytes are deterministic values above Unicode's range, so
 * they compare only with the identical ill-formed byte and can never alias a valid scalar.
 */
[[nodiscard]] inline InvariantScalarSequence FoldUtf8OrdinalIgnoreCase(const std::string& text) {
    constexpr std::uint32_t InvalidByteBase = 0x110000u;
    InvariantScalarSequence result;
    result.scalars.reserve(text.size());
    result.byteOffsets.reserve(text.size());

    std::size_t offset = 0;
    while (offset < text.size()) {
        result.byteOffsets.push_back(offset);
        std::uint32_t codePoint = 0;
        std::size_t length = 0;
        if (System::detail::TryDecodeUtf8Scalar(text, offset, codePoint, length)) {
            result.scalars.push_back(LookupToUpperInvariant(codePoint));
            offset += length;
        } else {
            result.scalars.push_back(
                InvalidByteBase + static_cast<unsigned char>(text[offset]));
            ++offset;
        }
    }
    return result;
}

/** Applies invariant Unicode simple casing to valid scalars and preserves malformed bytes. */
[[nodiscard]] inline std::string MapUtf8Invariant(const std::string& text,
                                                  InvariantCaseMapping mapping) {
    std::string result;
    result.reserve(text.size());
    std::size_t offset = 0;
    while (offset < text.size()) {
        std::uint32_t codePoint = 0;
        std::size_t length = 0;
        if (System::detail::TryDecodeUtf8Scalar(text, offset, codePoint, length)) {
            System::detail::AppendUtf8Scalar(result, MapInvariantCase(codePoint, mapping));
            offset += length;
        } else {
            result.push_back(text[offset++]);
        }
    }
    return result;
}

/** Applies invariant simple casing to a UTF-16 string, preserving unpaired surrogates. */
[[nodiscard]] inline std::u16string MapUtf16Invariant(const std::u16string& text,
                                                      InvariantCaseMapping mapping) {
    std::u16string result;
    result.reserve(text.size());
    std::size_t offset = 0;
    while (offset < text.size()) {
        const std::uint32_t first = text[offset];
        std::uint32_t codePoint = first;
        std::size_t length = 1;
        if (first >= 0xD800u && first <= 0xDBFFu && offset + 1 < text.size()) {
            const std::uint32_t second = text[offset + 1];
            if (second >= 0xDC00u && second <= 0xDFFFu) {
                codePoint = 0x10000u + ((first - 0xD800u) << 10) + (second - 0xDC00u);
                length = 2;
            }
        }

        if ((first >= 0xD800u && first <= 0xDFFFu) && length == 1) {
            result.push_back(static_cast<char16_t>(first));
            ++offset;
            continue;
        }

        const std::uint32_t mapped = MapInvariantCase(codePoint, mapping);
        if (mapped < 0x10000u) {
            result.push_back(static_cast<char16_t>(mapped));
        } else {
            const std::uint32_t supplementary = mapped - 0x10000u;
            result.push_back(static_cast<char16_t>(0xD800u + (supplementary >> 10)));
            result.push_back(static_cast<char16_t>(0xDC00u + (supplementary & 0x3FFu)));
        }
        offset += length;
    }
    return result;
}

} // namespace System::Globalization::detail
