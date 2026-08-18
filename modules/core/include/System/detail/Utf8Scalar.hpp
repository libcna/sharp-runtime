// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace System::detail {

    /**
     * @brief One UTF-8 scalar decode, in one place — the reporting form.
     *
     * This runtime stores `System::String` as UTF-8, so every component that converts *characters*
     * rather than *storage bytes* has to decode a scalar first — and by ticket #2014 five copies
     * of that decode had accumulated across `modules/text`. #2014 factored the rule out here and
     * moved the two `.cpp` copies onto it; **ticket #2354 (2026-08-18) moved the last three**,
     * which are header-inline and entangled with their types' own loops: `UnicodeEncoding.hpp`,
     * `UTF32Encoding.hpp` and `Rune.hpp`. There is now exactly one definition.
     *
     * **It lives in `Core.Base` as of ticket #2106**, not in `modules/text`. `System::BinaryData`
     * needed the same decode to give `ToString()` .NET's replacement behaviour, and
     * `modules/io` does not depend on `Text` -- so the choice was a sixth copy, a new **public**
     * component edge from `io` to `Text`, or moving the one definition somewhere everything
     * already depends on. `Core.Base` is that place. `System::Text::detail` re-exports these
     * names, so every existing caller is unchanged.
     *
     * **Why there are two forms.** `Rune::TryGetRuneAt` must *report* an ill-formed sequence,
     * while an `Encoding` must *substitute* `U+FFFD` and carry on; and the two disagree about
     * one thing that a single function cannot express — how many bytes a **structurally valid**
     * sequence that encodes a **non-scalar** (a surrogate, or a value above `U+10FFFF`) consumes.
     * `Rune` reports the sequence's own length there, so a caller can skip the whole thing; the
     * substituting form always reports `1`, so one replacement character is emitted per byte, as
     * .NET's replacement fallback does. Collapsing that difference would have been a silent
     * behaviour change in whichever door lost, so it is a parameter of the contract instead.
     *
     * @param s          The UTF-8 bytes.
     * @param size       Exclusive end of the readable range; a caller decoding a sub-range
     *                   passes that sub-range's end, and a truncated sequence at the boundary
     *                   is reported as ill-formed rather than read past.
     * @param i          Index of the first byte of the sequence; must be `< size`.
     * @param codePoint  Receives the decoded scalar; untouched unless the call returns `true`.
     * @param length     Receives the number of bytes to advance by — the sequence's length on
     *                   success; `1` for a structurally ill-formed sequence (bad lead byte, bad
     *                   or missing continuation byte, overlong encoding), so a caller resumes one
     *                   byte later and cannot loop forever; and the sequence's own length for a
     *                   structurally valid encoding of a non-scalar value.
     * @return @c true when a Unicode scalar was decoded.
     */
    [[nodiscard]] inline bool TryDecodeUtf8Scalar(const char* s, std::size_t size, std::size_t i,
                                                  std::uint32_t& codePoint, std::size_t& length) {
        auto isContinuation = [](unsigned char b) { return (b & 0xC0) == 0x80; };
        const unsigned char c0 = static_cast<unsigned char>(s[i]);
        std::uint32_t cp;
        std::size_t len;
        if (c0 < 0x80) {
            cp = c0;
            len = 1;
        } else if ((c0 & 0xE0) == 0xC0 && i + 1 < size &&
                   isContinuation(static_cast<unsigned char>(s[i + 1]))) {
            cp = (static_cast<std::uint32_t>(c0 & 0x1F) << 6) |
                 (static_cast<unsigned char>(s[i + 1]) & 0x3F);
            len = 2;
            if (cp < 0x80) { length = 1; return false; }   // overlong
        } else if ((c0 & 0xF0) == 0xE0 && i + 2 < size &&
                   isContinuation(static_cast<unsigned char>(s[i + 1])) &&
                   isContinuation(static_cast<unsigned char>(s[i + 2]))) {
            cp = (static_cast<std::uint32_t>(c0 & 0x0F) << 12) |
                 ((static_cast<unsigned char>(s[i + 1]) & 0x3F) << 6) |
                 (static_cast<unsigned char>(s[i + 2]) & 0x3F);
            len = 3;
            if (cp < 0x800) { length = 1; return false; }  // overlong
        } else if ((c0 & 0xF8) == 0xF0 && i + 3 < size &&
                   isContinuation(static_cast<unsigned char>(s[i + 1])) &&
                   isContinuation(static_cast<unsigned char>(s[i + 2])) &&
                   isContinuation(static_cast<unsigned char>(s[i + 3]))) {
            cp = (static_cast<std::uint32_t>(c0 & 0x07) << 18) |
                 ((static_cast<unsigned char>(s[i + 1]) & 0x3F) << 12) |
                 ((static_cast<unsigned char>(s[i + 2]) & 0x3F) << 6) |
                 (static_cast<unsigned char>(s[i + 3]) & 0x3F);
            len = 4;
            if (cp < 0x10000) { length = 1; return false; }  // overlong
        } else {
            length = 1;
            return false;
        }
        if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
            length = len;   // structurally valid, but not a scalar -- see the note above
            return false;
        }
        codePoint = cp;
        length = len;
        return true;
    }

    /** @brief `std::string` overload of the reporting form. */
    [[nodiscard]] inline bool TryDecodeUtf8Scalar(const std::string& s, std::size_t i,
                                                  std::uint32_t& codePoint, std::size_t& length) {
        return TryDecodeUtf8Scalar(s.data(), s.size(), i, codePoint, length);
    }

    /**
     * @brief One UTF-8 scalar decode — the substituting form every `Encoding` uses.
     *
     * Ill-formed input decodes to `U+FFFD` over a single byte, matching .NET's default
     * `DecoderFallback`. See `TryDecodeUtf8Scalar` for why the length differs between the two
     * forms on one specific class of input.
     *
     * @param s          The UTF-8 text.
     * @param i          Index of the first byte of the sequence; must be `< s.size()`.
     * @param codePoint  Receives the decoded scalar, or `U+FFFD` for an ill-formed sequence.
     * @param length     Receives the number of bytes consumed — always `1` for an ill-formed
     *                   sequence, so a caller resumes one byte later and cannot loop forever.
     */
    inline void DecodeUtf8Scalar(const std::string& s, std::size_t i,
                                 std::uint32_t& codePoint, std::size_t& length) {
        if (!TryDecodeUtf8Scalar(s, i, codePoint, length)) {
            codePoint = 0xFFFD;
            length    = 1;
        }
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

} // namespace System::detail
