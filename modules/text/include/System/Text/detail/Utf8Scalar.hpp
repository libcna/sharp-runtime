// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace System::Text::detail {

    /**
     * @brief One UTF-8 scalar decode, in one place.
     *
     * This runtime stores `System::String` as UTF-8, so every encoding that converts *characters*
     * rather than *storage bytes* has to decode a scalar first — and by ticket #2014 five copies
     * of that decode had accumulated across `modules/text`: `ASCIIEncoding.cpp`,
     * `UTF8Encoding.cpp` (as a validity-length variant), `UnicodeEncoding.hpp`,
     * `UTF32Encoding.hpp` and `Rune.hpp`. Adding a sixth for `Latin1Encoding` is exactly the
     * duplication this repository keeps having to repair, so #2014 factored the rule out here and
     * moved the two `.cpp` copies onto it. The three header-inline copies are a separate,
     * recorded follow-up (**#2354**), because they are entangled with those types' own loops.
     *
     * @param s          The UTF-8 text.
     * @param i          Index of the first byte of the sequence; must be `< s.size()`.
     * @param codePoint  Receives the decoded scalar, or `U+FFFD` for an ill-formed sequence.
     * @param length     Receives the number of bytes consumed — always `1` for an ill-formed
     *                   sequence, so a caller resumes one byte later and cannot loop forever.
     *
     * Conformance, unchanged from the copies it replaces: continuation bytes are validated,
     * overlong encodings are rejected, and a surrogate or an out-of-range scalar decodes to
     * `U+FFFD`.
     */
    inline void DecodeUtf8Scalar(const std::string& s, std::size_t i,
                                 std::uint32_t& codePoint, std::size_t& length) {
        auto isContinuation = [](unsigned char b) { return (b & 0xC0) == 0x80; };
        const unsigned char c0 = static_cast<unsigned char>(s[i]);
        std::uint32_t cp;
        std::size_t len;
        if (c0 < 0x80) {
            cp = c0;
            len = 1;
        } else if ((c0 & 0xE0) == 0xC0 && i + 1 < s.size() &&
                   isContinuation(static_cast<unsigned char>(s[i + 1]))) {
            cp = (static_cast<std::uint32_t>(c0 & 0x1F) << 6) |
                 (static_cast<unsigned char>(s[i + 1]) & 0x3F);
            len = 2;
            if (cp < 0x80) { codePoint = 0xFFFD; length = 1; return; }
        } else if ((c0 & 0xF0) == 0xE0 && i + 2 < s.size() &&
                   isContinuation(static_cast<unsigned char>(s[i + 1])) &&
                   isContinuation(static_cast<unsigned char>(s[i + 2]))) {
            cp = (static_cast<std::uint32_t>(c0 & 0x0F) << 12) |
                 ((static_cast<unsigned char>(s[i + 1]) & 0x3F) << 6) |
                 (static_cast<unsigned char>(s[i + 2]) & 0x3F);
            len = 3;
            if (cp < 0x800) { codePoint = 0xFFFD; length = 1; return; }
        } else if ((c0 & 0xF8) == 0xF0 && i + 3 < s.size() &&
                   isContinuation(static_cast<unsigned char>(s[i + 1])) &&
                   isContinuation(static_cast<unsigned char>(s[i + 2])) &&
                   isContinuation(static_cast<unsigned char>(s[i + 3]))) {
            cp = (static_cast<std::uint32_t>(c0 & 0x07) << 18) |
                 ((static_cast<unsigned char>(s[i + 1]) & 0x3F) << 12) |
                 ((static_cast<unsigned char>(s[i + 2]) & 0x3F) << 6) |
                 (static_cast<unsigned char>(s[i + 3]) & 0x3F);
            len = 4;
            if (cp < 0x10000) { codePoint = 0xFFFD; length = 1; return; }
        } else {
            codePoint = 0xFFFD;
            length = 1;
            return;
        }
        if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
            codePoint = 0xFFFD;
            length = 1;
            return;
        }
        codePoint = cp;
        length = len;
    }

    /**
     * @brief Appends @p codePoint to @p out as UTF-8.
     *
     * The inverse of DecodeUtf8Scalar, needed by every encoding that decodes *into* this
     * runtime's UTF-8 `System::String` representation.
     */
    inline void AppendUtf8Scalar(std::string& out, std::uint32_t codePoint) {
        if (codePoint < 0x80) {
            out.push_back(static_cast<char>(codePoint));
        } else if (codePoint < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
            out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        } else if (codePoint < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
            out.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
            out.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        }
    }

} // namespace System::Text::detail
