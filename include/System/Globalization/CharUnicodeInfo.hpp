// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <climits>
#include <cwctype>
#include <string>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/Globalization/UnicodeCategory.hpp"

namespace System::Globalization {

using SharpRuntime::charcs;
using SharpRuntime::intcs;

/**
 * @brief Retrieves information about a Unicode character, such as its category and numeric value.
 *
 * C++ counterpart of .NET System.Globalization.CharUnicodeInfo.
 * All methods are static; this class cannot be instantiated.
 */
class CharUnicodeInfo {
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
        return GetDecimalDigitValue(s[static_cast<size_t>(index)]);
    }

    /**
     * @brief Gets the digit value of a character, or -1 if not a digit.
     *
     * C++ counterpart of .NET CharUnicodeInfo.GetDigitValue(char).
     * @param ch The character to evaluate.
     * @return The digit value (0–9), or -1 if @p ch is not a digit.
     */
    static intcs GetDigitValue(charcs ch) {
        return GetDecimalDigitValue(ch);
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
        return GetUnicodeCategory(s[static_cast<size_t>(index)]);
    }

    /**
     * @brief Gets the Unicode category for a character specified by code point.
     *
     * C++ counterpart of .NET CharUnicodeInfo.GetUnicodeCategory(int) (for supplementary chars).
     * Uses POSIX wide-character classification; non-BMP code points are mapped to OtherNotAssigned.
     * @param codePoint The Unicode code point to categorize.
     * @return The UnicodeCategory value for the code point.
     */
    static UnicodeCategory GetUnicodeCategory(intcs codePoint) {
        wchar_t wc = (codePoint >= 0 && codePoint <= WCHAR_MAX)
                     ? static_cast<wchar_t>(codePoint) : L'\0';
        if (std::iswupper(wc)) return UnicodeCategory::UppercaseLetter;
        if (std::iswlower(wc)) return UnicodeCategory::LowercaseLetter;
        if (std::iswdigit(wc)) return UnicodeCategory::DecimalDigitNumber;
        if (std::iswspace(wc)) return UnicodeCategory::SpaceSeparator;
        if (std::iswpunct(wc)) return UnicodeCategory::OtherPunctuation;
        if (codePoint == 0)    return UnicodeCategory::Control;
        if (codePoint < 32)    return UnicodeCategory::Control;
        if (std::iswalpha(wc)) return UnicodeCategory::OtherLetter;
        if (std::iswcntrl(wc)) return UnicodeCategory::Control;
        return UnicodeCategory::OtherNotAssigned;
    }
};

} // namespace System::Globalization
