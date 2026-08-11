// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Globalization/UnicodeCategory.hpp"

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
     * @brief Gets the decimal digit value of a character, or -1 if not a decimal digit.
     *
     * C++ counterpart of .NET CharUnicodeInfo.GetDecimalDigitValue(char).
     * @param ch The character to evaluate.
     * @return The decimal digit value (0–9), or -1 if @p ch is not a decimal digit.
     */
    static intcs GetDecimalDigitValue(charcs ch) {
        if (ch >= u'0' && ch <= u'9') return static_cast<intcs>(ch - u'0');
        return -1;
    }

    /**
     * @brief Gets the decimal digit value of the character at the specified index in a string.
     *
     * C++ counterpart of .NET CharUnicodeInfo.GetDecimalDigitValue(string, int).
     * @param s     The string containing the character.
     * @param index The zero-based index of the character.
     * @return The decimal digit value (0–9), or -1 if the character is not a decimal digit.
     */
    static intcs GetDecimalDigitValue(const std::u16string& s, intcs index) {
        CheckIndex(s, index);
        return GetDecimalDigitValue(s[static_cast<size_t>(index)]);
    }

    /**
     * @brief Gets the digit value of a character, or -1 if not a digit.
     *
     * C++ counterpart of .NET CharUnicodeInfo.GetDigitValue(char).
     * Unlike GetDecimalDigitValue, this also recognizes characters whose Unicode
     * Numeric_Type is Digit rather than Decimal (e.g. superscript digits), matching
     * .NET's distinction between the two methods.
     * @param ch The character to evaluate.
     * @return The digit value (0–9), or -1 if @p ch is not a digit.
     */
    static intcs GetDigitValue(charcs ch) {
        intcs decimalValue = GetDecimalDigitValue(ch);
        if (decimalValue != -1) return decimalValue;
        switch (ch) {
            case 0x00B9: return 1; // superscript 1
            case 0x00B2: return 2; // superscript 2
            case 0x00B3: return 3; // superscript 3
            default: return -1;
        }
    }

    /**
     * @brief Gets the digit value of the character at the specified index in a string.
     *
     * C++ counterpart of .NET CharUnicodeInfo.GetDigitValue(string, int).
     * @param s     The string containing the character.
     * @param index The zero-based index of the character.
     * @return The digit value (0–9), or -1 if the character is not a digit.
     */
    static intcs GetDigitValue(const std::u16string& s, intcs index) {
        CheckIndex(s, index);
        return GetDigitValue(s[static_cast<size_t>(index)]);
    }

    /**
     * @brief Gets the numeric value associated with a Unicode character.
     *
     * C++ counterpart of .NET CharUnicodeInfo.GetNumericValue(char).
     * Returns -1.0 if the character has no numeric value.
     * @param ch The character to evaluate.
     * @return The numeric value, or -1.0 if @p ch is not a numeric character.
     */
    static double GetNumericValue(charcs ch) {
        if (ch >= u'0' && ch <= u'9') return static_cast<double>(ch - u'0');
        if (ch == 0x00B2) return 2.0;  // superscript 2
        if (ch == 0x00B3) return 3.0;  // superscript 3
        if (ch == 0x00B9) return 1.0;  // superscript 1
        if (ch == 0x00BC) return 0.25; // vulgar fraction 1/4
        if (ch == 0x00BD) return 0.5;  // vulgar fraction 1/2
        if (ch == 0x00BE) return 0.75; // vulgar fraction 3/4
        return -1.0;
    }

    /**
     * @brief Gets the numeric value of the character at the specified index in a string.
     *
     * C++ counterpart of .NET CharUnicodeInfo.GetNumericValue(string, int).
     * @param s     The string containing the character.
     * @param index The zero-based index of the character.
     * @return The numeric value, or -1.0 if the character has no numeric value.
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
        // ASCII, U+0000-U+007F. These ranges reproduce exactly what the previous
        // std::iswupper/iswlower/iswdigit/iswspace/iswpunct/iswalpha/iswcntrl ladder
        // returned in the "C" locale, restated as explicit ranges. The isw* functions are
        // locale-sensitive, so the old ladder gave a different category for the same code
        // point once any process installed a different global locale -- the very thing a
        // culture-insensitive CharUnicodeInfo must not do.
        //
        // C0 controls (U+0000-U+001F) and DEL (U+007F) are Cc; only U+0020 itself is Zs, so
        // TAB/LF/VT/FF/CR must be Control and not SpaceSeparator.
        if (codePoint < 0x20 || codePoint == 0x7F)      return UnicodeCategory::Control;
        if (codePoint >= 'A' && codePoint <= 'Z')       return UnicodeCategory::UppercaseLetter;
        if (codePoint >= 'a' && codePoint <= 'z')       return UnicodeCategory::LowercaseLetter;
        if (codePoint >= '0' && codePoint <= '9')       return UnicodeCategory::DecimalDigitNumber;
        if (codePoint == ' ')                           return UnicodeCategory::SpaceSeparator;
        if ((codePoint >= 0x21 && codePoint <= 0x2F) || (codePoint >= 0x3A && codePoint <= 0x40)
            || (codePoint >= 0x5B && codePoint <= 0x60) || (codePoint >= 0x7B && codePoint <= 0x7E))
            return UnicodeCategory::OtherPunctuation;
        // Surrogate code points. This is not Unicode-table knowledge: the range is already
        // fixed by this port itself in System::Char::IsSurrogate ("c >= 0xD800u && c <=
        // 0xDFFFu"), and answering OtherNotAssigned here made the two contradict each other
        // -- one calling U+D800-U+DFFF surrogates, the other calling them unassigned.
        if (codePoint >= 0xD800 && codePoint <= 0xDFFF) return UnicodeCategory::Surrogate;
        return UnicodeCategory::OtherNotAssigned;
    }
};

} // namespace System::Globalization
