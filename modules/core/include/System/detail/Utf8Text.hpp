// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "SharpRuntime/SharpRuntimeHelper.hpp"

#include <cstddef>
#include <cstdint>

namespace System::detail {

    /**
     * @brief The module's single UTF-8 decoding and Unicode-whitespace policy.
     *
     * `System::Core` has several public doors that take .NET-shaped *character* text but hold it
     * as UTF-8 bytes in a `std::string` or a `char` span. Each of them grew its own ad-hoc byte
     * handling, and each got a different answer:
     *
     * - `Char::Parse` derived a length from the lead byte and checked only continuation-bit
     *   shape, so `C0 80` decoded to U+0000, `C1 BF` to U+007F, `E0 80 80` to U+0000 and
     *   `ED A0 80` to the surrogate U+D800 (SR-AUD-017);
     * - `MemoryExtensions::TrimStart`/`TrimEnd` and
     *   `ArgumentException::ThrowIfNullOrWhiteSpace` classified whitespace with the single-byte
     *   C-locale `std::isspace`, so the UTF-8 encoding of U+00A0 was not whitespace — the trim
     *   kept it and the validator *accepted* an argument made only of it (SR-AUD-048).
     *
     * `CCF-015` asks for exactly one declared decode-and-classification policy rather than more
     * isolated byte checks, and this header is it. Both halves are deliberately
     * **locale-independent**: `std::isspace`/`std::iswspace` answer differently depending on the
     * active `LC_CTYPE`, which is why a port whose contract is Unicode cannot be built on them.
     *
     * Not part of any type's public API surface; it lives in a public header only because both
     * a header-only consumer (`MemoryExtensions`) and two `.cpp` consumers need it.
     */

    /** @brief Result of decoding one UTF-8 scalar value. */
    struct Utf8Decoded {
        char32_t scalar = 0;      ///< The decoded scalar value; meaningless when `!valid`.
        std::size_t length = 0;   ///< Bytes consumed; 1 when `!valid`, so callers can advance.
        bool valid = false;       ///< False for any sequence RFC 3629 does not permit.
    };

    /**
     * @brief Decodes the UTF-8 sequence beginning at @p data, rejecting everything RFC 3629 does.
     *
     * Rejected, each of which the previous per-site code accepted somewhere:
     * - a lead byte in `0x80..0xBF` (a stray continuation) or `0xF5..0xFF` (beyond U+10FFFF);
     * - `0xC0`/`0xC1`, which can only ever introduce an overlong two-byte form;
     * - a truncated sequence — fewer than @p size bytes remain;
     * - a byte that should be a continuation but is not `10xxxxxx`;
     * - an **overlong** encoding, checked against the minimum scalar for the length actually used;
     * - a UTF-16 **surrogate** `U+D800..U+DFFF`, which UTF-8 must never encode;
     * - a scalar above `U+10FFFF`.
     *
     * On rejection `length` is 1 so a scanning caller advances by one byte and resynchronises,
     * which is what lets the whitespace doors treat malformed input as "not whitespace" instead
     * of gaining a new exception they never had.
     *
     * @param data First byte of the sequence; may be null only when @p size is 0.
     * @param size Bytes available from @p data.
     */
    [[nodiscard]] inline Utf8Decoded DecodeUtf8(const char* data, std::size_t size) noexcept {
        if (size == 0 || data == nullptr) return {};
        const auto b0 = static_cast<unsigned char>(data[0]);

        if (b0 < 0x80u) return {static_cast<char32_t>(b0), 1, true};
        if (b0 < 0xC2u) return {0, 1, false};  // 0x80..0xBF stray continuation, 0xC0/0xC1 overlong

        std::size_t length;
        char32_t scalar;
        char32_t minimum;
        if (b0 < 0xE0u)      { length = 2; scalar = b0 & 0x1Fu; minimum = 0x80u; }
        else if (b0 < 0xF0u) { length = 3; scalar = b0 & 0x0Fu; minimum = 0x800u; }
        else if (b0 < 0xF5u) { length = 4; scalar = b0 & 0x07u; minimum = 0x10000u; }
        else                 { return {0, 1, false}; }  // 0xF5..0xFF exceed U+10FFFF

        if (size < length) return {0, 1, false};  // truncated
        for (std::size_t i = 1; i < length; ++i) {
            const auto bi = static_cast<unsigned char>(data[i]);
            if ((bi & 0xC0u) != 0x80u) return {0, 1, false};
            scalar = (scalar << 6) | (bi & 0x3Fu);
        }

        if (scalar < minimum) return {0, 1, false};                      // overlong
        if (scalar >= 0xD800u && scalar <= 0xDFFFu) return {0, 1, false}; // surrogate
        if (scalar > 0x10FFFFu) return {0, 1, false};
        return {scalar, length, true};
    }

    /**
     * @brief .NET's `Char.IsWhiteSpace` set, as an explicit locale-independent table.
     *
     * The Unicode `White_Space` property, which is what `Char.IsWhiteSpace` documents. Spelling
     * it out is deliberate: it is a small closed set, it needs no ICU data (absent here), and it
     * cannot drift with `LC_CTYPE` the way `std::isspace`/`std::iswspace` do.
     *
     * @see https://learn.microsoft.com/en-us/dotnet/api/system.char.iswhitespace
     */
    [[nodiscard]] constexpr bool IsUnicodeWhiteSpace(char32_t c) noexcept {
        return (c >= 0x0009u && c <= 0x000Du)   // TAB, LF, VT, FF, CR
            || c == 0x0020u                     // SPACE
            || c == 0x0085u                     // NEXT LINE
            || c == 0x00A0u                     // NO-BREAK SPACE
            || c == 0x1680u                     // OGHAM SPACE MARK
            || (c >= 0x2000u && c <= 0x200Au)   // EN QUAD .. HAIR SPACE
            || c == 0x2028u                     // LINE SEPARATOR
            || c == 0x2029u                     // PARAGRAPH SEPARATOR
            || c == 0x202Fu                     // NARROW NO-BREAK SPACE
            || c == 0x205Fu                     // MEDIUM MATHEMATICAL SPACE
            || c == 0x3000u;                    // IDEOGRAPHIC SPACE
    }

    /**
     * @brief True when the UTF-8 sequence at @p data is a single Unicode whitespace character.
     *
     * Malformed input is **not** whitespace, so a scanning caller stops at it rather than
     * throwing — the trim doors and `ThrowIfNullOrWhiteSpace` have never reported an encoding
     * error and do not start now.
     *
     * @param[out] length Bytes consumed, always at least 1, so a caller can always advance.
     */
    [[nodiscard]] inline bool IsUtf8WhiteSpace(const char* data, std::size_t size,
                                               std::size_t& length) noexcept {
        const Utf8Decoded d = DecodeUtf8(data, size);
        length = d.length == 0 ? 1 : d.length;
        return d.valid && IsUnicodeWhiteSpace(d.scalar);
    }

} // namespace System::detail
