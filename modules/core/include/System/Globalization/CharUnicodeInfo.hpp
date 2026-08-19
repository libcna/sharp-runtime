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
 * @note <b>Category classification is a declared reduction, and is locale-independent.</b>
 *       GetUnicodeCategory answers from this port's own knowledge only: the ASCII range
 *       U+0000–U+007F, and the surrogate range U+D800–U+DFFF that
 *       System::Char::IsSurrogate already fixes. Every other code point — every
 *       assigned non-ASCII letter, every combining mark, every symbol, every private-use
 *       code point, and every supplementary code point — is reported
 *       UnicodeCategory::OtherNotAssigned. That value is a statement about what this port
 *       knows, not a claim that the code point is unassigned in Unicode. Answering for the
 *       rest of the code space requires a Unicode character database, its attribution and a
 *       stated Unicode version with an update policy; that decision is open and no partial
 *       table is hand-authored here.
 *
 * @note <b>The three numeric queries are a declared reduction too, and a much narrower
 *       one.</b> GetDecimalDigitValue answers only for U+0030-U+0039; GetDigitValue adds
 *       only U+00B9, U+00B2 and U+00B3 (superscript one, two, three); GetNumericValue adds
 *       to those only U+00BC, U+00BD and U+00BE (the vulgar fractions one quarter, one half,
 *       three quarters). <b>Sixteen code points in total.</b> Every other code point gets
 *       -1 (or -1.0), including code points that really do carry a Unicode numeric value:
 *       U+0665 ARABIC-INDIC DIGIT FIVE answers -1 here where .NET answers 5, U+216B ROMAN
 *       NUMERAL TWELVE answers -1.0 where .NET answers 12, and U+2153 VULGAR FRACTION ONE
 *       THIRD answers -1.0 where .NET answers 0.333.... As with the category above, -1
 *       states what this port knows, not that the character has no numeric value; the
 *       Numeric_Type/Numeric_Value data needed to answer properly is part of the same open
 *       Unicode-data decision and is not hand-authored here either. `System::Char::
 *       GetNumericValue` forwards to GetNumericValue and inherits the reduction exactly.
 */
class CharUnicodeInfo {
    /** @brief Throws ArgumentOutOfRangeException if @p index is out of bounds for @p s. */
    static void CheckIndex(const std::u16string& s, intcs index) {
        if (index < 0 || static_cast<size_t>(index) >= s.size()) {
            throw System::ArgumentOutOfRangeException("index");
        }
    }

public:
    CharUnicodeInfo() = delete;

    /**
     * @brief Gets the decimal digit value of a character, or -1 if this port has none.
     *
     * C++ counterpart of .NET CharUnicodeInfo.GetDecimalDigitValue(char), reduced to
     * <b>ASCII U+0030-U+0039 only</b> (see the class note). A non-ASCII decimal digit
     * such as U+0665 returns -1 here and 5 in .NET.
     * @param ch The character to evaluate.
     * @return The decimal digit value (0–9) for U+0030-U+0039; -1 for every other
     *         character, whether or not it is a decimal digit in Unicode.
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
     * @return The decimal digit value (0–9) for U+0030-U+0039; -1 otherwise, under the
     *         same reduction as the single-character overload.
     */
    static intcs GetDecimalDigitValue(const std::u16string& s, intcs index) {
        CheckIndex(s, index);
        return GetDecimalDigitValue(s[static_cast<size_t>(index)]);
    }

    /**
     * @brief Gets the digit value of a character, or -1 if this port has none.
     *
     * C++ counterpart of .NET CharUnicodeInfo.GetDigitValue(char).
     * Unlike GetDecimalDigitValue, this also recognizes characters whose Unicode
     * Numeric_Type is Digit rather than Decimal (e.g. superscript digits), matching
     * .NET's distinction between the two methods -- but only for the three superscripts
     * it hard-codes. <b>Reduced to U+0030-U+0039 plus U+00B9/U+00B2/U+00B3</b> (see the
     * class note); every other Numeric_Type Digit character returns -1.
     * @param ch The character to evaluate.
     * @return The digit value (0–9) for the thirteen characters above; -1 for every
     *         other character, whether or not it is a digit in Unicode.
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
     * @return The digit value (0–9) for the thirteen recognized characters; -1 otherwise,
     *         under the same reduction as the single-character overload.
     */
    static intcs GetDigitValue(const std::u16string& s, intcs index) {
        CheckIndex(s, index);
        return GetDigitValue(s[static_cast<size_t>(index)]);
    }

    /**
     * @brief Gets the numeric value associated with a Unicode character.
     *
     * C++ counterpart of .NET CharUnicodeInfo.GetNumericValue(char), <b>reduced to
     * sixteen code points</b>: U+0030-U+0039, the superscripts U+00B9/U+00B2/U+00B3, and
     * the vulgar fractions U+00BC/U+00BD/U+00BE (see the class note). U+216B answers -1.0
     * here where .NET answers 12, and U+2153 answers -1.0 where .NET answers 0.333....
     * @param ch The character to evaluate.
     * @return The numeric value for the sixteen characters above; -1.0 for every other
     *         character, whether or not it carries a Unicode numeric value.
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
     * @return The numeric value for the sixteen recognized characters; -1.0 otherwise,
     *         under the same reduction as the single-character overload.
     */
    static double GetNumericValue(const std::u16string& s, intcs index) {
        CheckIndex(s, index);
        return GetNumericValue(s[static_cast<size_t>(index)]);
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
        return GetUnicodeCategory(s[static_cast<size_t>(index)]);
    }

    /**
     * @brief Gets the Unicode category for a character specified by code point.
     *
     * C++ counterpart of .NET CharUnicodeInfo.GetUnicodeCategory(int) (for supplementary chars).
     *
     * The answer is <b>locale-independent</b>: no C or C++ locale facet is consulted, so a
     * process that installs a different global locale gets the same category for the same
     * code point. It is also <b>reduced</b>: only U+0000–U+007F and the surrogate range
     * U+D800–U+DFFF are classified, and every other code point — BMP or
     * supplementary — returns OtherNotAssigned (see the class note). Within ASCII, every
     * punctuation and symbol character is reported OtherPunctuation rather than its finer
     * .NET subcategory (MathSymbol, CurrencySymbol, OpenPunctuation, DashPunctuation, ...);
     * that too is part of the reduction and is unchanged here.
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
