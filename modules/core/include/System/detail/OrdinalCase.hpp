// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/detail/Utf8Scalar.hpp"
#include "System/Globalization/detail/UnicodeCategoryLookup.hpp"

namespace System::detail {
// Internal only. UCD16 is the existing sharp-runtime SA-4 source of record.
// CharUnicodeInfo.cs:294-310 supplies simple uppercase; pal_common.c:5-20
// preserves U+0131 and U+017F for ordinal casing. No expansions or normalization.
// .NET 4 OrdinalComparer delegates comparison to TextInfo (Compare IL_001d-0025)
// and invariant hashing to TextInfo (GetHashCode IL_000e-001c). TQ-078 uses
// the runtime's pinned UCD16 data (owner decision 2026-11-05); OS/NLS-version
// differences outside the measured witnesses remain a documented boundary.
inline std::uint32_t OrdinalUpper(std::uint32_t scalar) noexcept {
    if (scalar == 0x0131 || scalar == 0x017f) return scalar;
    return Globalization::detail::LookupToUpperInvariant(scalar);
}

inline std::u16string OrdinalIgnoreCaseKey(const std::string& text) {
    std::u16string result;
    result.reserve(text.size());
    for (std::size_t i = 0; i < text.size();) {
        std::uint32_t scalar = 0;
        std::size_t length = 1;
        if (!TryDecodeUtf8Scalar(text, i, scalar, length)) {
            // Native-only adaptation: preserve each malformed byte distinctly
            // using an unpaired low surrogate. Valid UTF-8 cannot encode one.
            result.push_back(static_cast<char16_t>(0xdc00u + static_cast<unsigned char>(text[i++])));
            continue;
        }
        i += length;
        scalar = OrdinalUpper(scalar);
        if (scalar <= 0xffffu) result.push_back(static_cast<char16_t>(scalar));
        else {
            scalar -= 0x10000u;
            result.push_back(static_cast<char16_t>(0xd800u + (scalar >> 10)));
            result.push_back(static_cast<char16_t>(0xdc00u + (scalar & 0x3ffu)));
        }
    }
    return result;
}
} // namespace System::detail
