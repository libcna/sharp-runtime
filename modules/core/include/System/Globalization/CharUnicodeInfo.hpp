// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Globalization/UnicodeCategory.hpp"
#include "System/Globalization/detail/UnicodeCategoryLookup.hpp"
#include "System/Globalization/detail/UnicodeNumericLookup.hpp"

namespace System::Globalization {

using SharpRuntime::charcs;
using SharpRuntime::intcs;

/**
 * @brief Retrieves information about a Unicode character, such as its category and numeric value.
 *
 * C++ counterpart of .NET System.Globalization.CharUnicodeInfo.
 * All methods are static; this class cannot be instantiated.
 *
 * @note <b>Category and numeric classification cover the complete Unicode scalar range and
 *       are locale-independent.</b> Tickets #2315 and #2336 replaced the former ASCII/sixteen-
 *       code-point reductions with generated Unicode 16.0 lookup tables. No C or C++ locale
 *       facet participates, so installing a different process locale cannot change an answer.
 *       The generated data, version pin and update procedure are documented in
 *       `docs/Migration-UnicodeCategoryTable.md` and the generator sources.
 *
 * @note The string/index overloads combine a valid UTF-16 surrogate pair and query its code
 *       point, matching .NET. An index that names the low surrogate of a pair still observes that
 *       code unit as a surrogate, also matching .NET's index contract.
 */
class CharUnicodeInfo {
    /** @brief Throws ArgumentOutOfRangeException if @p index is out of bounds for @p s. */
    static void CheckIndex(const std::u16string& s, intcs index) {
        if (index < 0 || static_cast<size_t>(index) >= s.size()) {
            throw System::ArgumentOutOfRangeException("index");
        }
    }

    /**
     * @brief The code POINT at @p index, combining a surrogate pair. Ticket #2385.
     *
     * `CharUnicodeInfo.GetCodePoint` (`CharUnicodeInfo.cs:436-465`), transcribed. All four
     * `(string, index)` overloads route through it in .NET, and none of them did here -- each
     * indexed a code UNIT and handed a lone surrogate to the single-character overload.
     *
     * The gap was **invisible until #2315 and #2336 landed**: while every non-ASCII code point
     * answered `OtherNotAssigned` and every one answered `-1`, a supplementary character and a
     * lone surrogate gave the same answer, so nothing could observe the difference.
     *
     * @par Three conditions, all of them .NET's
     * A pair is combined only when the unit at @p index is a **high** surrogate, `index + 1` is
     * in range, and the unit there is a **low** surrogate. Otherwise the unit itself is returned
     * -- so a lone high surrogate, a lone low surrogate, a high surrogate at the end of the
     * string, and the **low half of a valid pair** all answer as that surrogate. The last is
     * easy to get wrong by "helpfully" looking backwards; .NET does not, and neither does this.
     */
    /*
     * HONEST NOTE ON THE EVIDENCE: a mutation removing the `i + 1 < s.size()` bound below is
     * NOT caught, and it is a genuine equivalence rather than a gap in the tests. `CheckIndex`
     * has already established `i < s.size()`, so `i + 1 <= s.size()`, and
     * `std::basic_string::operator[](size())` is *specified* to return a reference to a null
     * character -- it is defined behaviour, not a read past the end. A `u'\0'` successor makes
     * `low` underflow well clear of `0x3FF`, so no pair is combined either way.
     *
     * The bound is kept because it is .NET's, and because resting the correctness of a
     * surrogate scan on `operator[]`'s null-terminator guarantee is a subtler contract than a
     * bounds test -- one that would silently become wrong if this ever moved to a span.
     */
    static uint32_t GetCodePointAt(const std::u16string& s, intcs index) {
        const auto i = static_cast<size_t>(index);
        uint32_t codePoint = s[i];
        const uint32_t high = codePoint - 0xD800u;
        if (high <= 0x3FFu && i + 1 < s.size()) {
            const uint32_t low = static_cast<uint32_t>(s[i + 1]) - 0xDC00u;
            if (low <= 0x3FFu) codePoint = (high << 10) + low + 0x10000u;
        }
        return codePoint;
    }

public:
    CharUnicodeInfo() = delete;

    /**
     * @brief Gets the Unicode decimal digit value of a character.
     *
     * C++ counterpart of .NET CharUnicodeInfo.GetDecimalDigitValue(char).
     * @param ch The character to evaluate.
     * @return The decimal digit value (0–9), or -1 if @p ch is not a Unicode decimal digit.
     */
    static intcs GetDecimalDigitValue(charcs ch) {
        // #2336: the whole code space, from the generated UCD 16.0 numeric table.
        return detail::LookupDecimalDigitValue(static_cast<uint32_t>(ch));
    }

    /**
     * @brief Gets the decimal digit value of the character at the specified index in a string.
     *
     * C++ counterpart of .NET CharUnicodeInfo.GetDecimalDigitValue(string, int).
     * @param s     The string containing the character.
     * @param index The zero-based index of the character.
     * @return The decimal digit value (0–9), or -1 if the code point at @p index is not a
     *         Unicode decimal digit.
     */
    static intcs GetDecimalDigitValue(const std::u16string& s, intcs index) {
        CheckIndex(s, index);
        // #2385: a code POINT, so a surrogate pair is combined as .NET's GetCodePoint does.
        return detail::LookupDecimalDigitValue(GetCodePointAt(s, index));
    }

    /**
     * @brief Gets the Unicode digit value of a character.
     *
     * C++ counterpart of .NET CharUnicodeInfo.GetDigitValue(char).
     * Unlike GetDecimalDigitValue, this also recognizes characters whose Unicode
     * Numeric_Type is Digit rather than Decimal (for example superscript digits), matching
     * .NET's distinction between the two methods.
     * @param ch The character to evaluate.
     * @return The digit value (0–9), or -1 if @p ch has neither Decimal nor Digit numeric type.
     */
    static intcs GetDigitValue(charcs ch) {
        // #2336. Note this is NOT "decimal value, else the digit-only cases": .NET reads a
        // DIFFERENT NIBBLE of the same table byte (the low one, where GetDecimalDigitValue
        // reads the high one), so the two are independent properties rather than one built on
        // the other. The old fall-through happened to agree for the thirteen code points it
        // covered and would not have for the rest.
        return detail::LookupDigitValue(static_cast<uint32_t>(ch));
    }

    /**
     * @brief Gets the digit value of the character at the specified index in a string.
     *
     * C++ counterpart of .NET CharUnicodeInfo.GetDigitValue(string, int).
     * @param s     The string containing the character.
     * @param index The zero-based index of the character.
     * @return The digit value (0–9), or -1 if the code point at @p index has neither Decimal
     *         nor Digit numeric type.
     */
    static intcs GetDigitValue(const std::u16string& s, intcs index) {
        CheckIndex(s, index);
        // #2385: a code POINT, so a surrogate pair is combined as .NET's GetCodePoint does.
        return detail::LookupDigitValue(GetCodePointAt(s, index));
    }

    /**
     * @brief Gets the numeric value associated with a Unicode character.
     *
     * C++ counterpart of .NET CharUnicodeInfo.GetNumericValue(char).
     * @param ch The character to evaluate.
     * @return The Unicode numeric value, including fractional values; -1.0 if @p ch has none.
     */
    static double GetNumericValue(charcs ch) {
        // #2336: the whole code space. The rationals are real rationals -- U+2153 is
        // 0.333..., not a digit -- which is why the table is doubles.
        return detail::LookupNumericValue(static_cast<uint32_t>(ch));
    }

    /**
     * @brief Gets the numeric value of the character at the specified index in a string.
     *
     * C++ counterpart of .NET CharUnicodeInfo.GetNumericValue(string, int).
     * @param s     The string containing the character.
     * @param index The zero-based index of the character.
     * @return The Unicode numeric value of the code point at @p index, or -1.0 if it has none.
     */
    static double GetNumericValue(const std::u16string& s, intcs index) {
        CheckIndex(s, index);
        // #2385: a code POINT, so a surrogate pair is combined as .NET's GetCodePoint does.
        return detail::LookupNumericValue(GetCodePointAt(s, index));
    }

    /**
     * @brief Gets the Unicode category of a character.
     *
     * C++ counterpart of .NET CharUnicodeInfo.GetUnicodeCategory(char).
     * @param ch The character to categorize.
     * @return The UnicodeCategory value for @p ch.
     */
    static UnicodeCategory GetUnicodeCategory(charcs ch) {
        return GetUnicodeCategory(static_cast<intcs>(ch));
    }

    /**
     * @brief Gets the Unicode category of the character at the specified index in a string.
     *
     * C++ counterpart of .NET CharUnicodeInfo.GetUnicodeCategory(string, int).
     * @param s     The string containing the character.
     * @param index The zero-based index of the character.
     * @return The UnicodeCategory value for the character at @p index.
     */
    static UnicodeCategory GetUnicodeCategory(const std::u16string& s, intcs index) {
        CheckIndex(s, index);
        // #2385: a code POINT, so a surrogate pair is combined as .NET's GetCodePoint does.
        return detail::LookupUnicodeCategory(GetCodePointAt(s, index));
    }

    /**
     * @brief Gets the Unicode category for a character specified by code point.
     *
     * C++ counterpart of .NET CharUnicodeInfo.GetUnicodeCategory(int) (for supplementary chars).
     *
     * The answer is read from the generated Unicode 16.0 category table and is
     * <b>locale-independent</b>: no C or C++ locale facet is consulted, so a process that
     * installs a different global locale gets the same category for the same code point.
     *
     * @param codePoint The Unicode code point to categorize.
     * @return The UnicodeCategory value for the code point.
     * @throws System::ArgumentOutOfRangeException if @p codePoint is not in [0, 0x10FFFF].
     */
    static UnicodeCategory GetUnicodeCategory(intcs codePoint) {
        if (codePoint < 0 || codePoint > 0x10FFFF) {
            throw System::ArgumentOutOfRangeException("codePoint");
        }
        // #2315. The whole code space is answered from the generated Unicode 16.0 table, and
        // the ASCII/surrogate ladder that stood here is GONE rather than kept as a fast path.
        // Keeping it would have been two sources of truth for the same 128 code points, which
        // is the shape of defect this repository keeps removing -- and it would have hidden
        // #2316's own finding, since the ladder answered OtherPunctuation for all 32 ASCII
        // punctuation and symbol characters where Unicode has six different categories for
        // them. Measured, the table disagrees with the ladder on 17 of the 128 ASCII code
        // points -- every one a punctuation or symbol character the ladder called
        // OtherPunctuation and Unicode calls something more specific -- and on 292,420 of the
        // 1,114,112 code points overall. See docs/Migration-UnicodeCategoryTable.md.
        return detail::LookupUnicodeCategory(static_cast<uint32_t>(codePoint));
    }
};

} // namespace System::Globalization
