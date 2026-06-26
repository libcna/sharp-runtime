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

/** <summary>Retrieves information about a Unicode character, such as its category and numeric value.</summary> */
class CharUnicodeInfo {
public:
    CharUnicodeInfo() = delete;

    /** <summary>Gets the decimal digit value of a character, or -1 if not a decimal digit.</summary> */
    static intcs GetDecimalDigitValue(charcs ch) {
        if (ch >= u'0' && ch <= u'9') return static_cast<intcs>(ch - u'0');
        return -1;
    }

    static intcs GetDecimalDigitValue(const std::u16string& s, intcs index) {
        return GetDecimalDigitValue(s[static_cast<size_t>(index)]);
    }

    /** <summary>Gets the digit value of a character, or -1 if not a digit.</summary> */
    static intcs GetDigitValue(charcs ch) {
        return GetDecimalDigitValue(ch);
    }

    static intcs GetDigitValue(const std::u16string& s, intcs index) {
        return GetDigitValue(s[static_cast<size_t>(index)]);
    }

    /** <summary>Gets the numeric value of a Unicode character (-1.0 if not numeric).</summary> */
    static double GetNumericValue(charcs ch) {
        if (ch >= u'0' && ch <= u'9') return static_cast<double>(ch - u'0');
        // Superscript digits U+00B2, U+00B3, U+00B9
        if (ch == 0x00B2) return 2.0;
        if (ch == 0x00B3) return 3.0;
        if (ch == 0x00B9) return 1.0;
        // Vulgar fractions U+00BC–U+00BE
        if (ch == 0x00BC) return 0.25;
        if (ch == 0x00BD) return 0.5;
        if (ch == 0x00BE) return 0.75;
        return -1.0;
    }

    static double GetNumericValue(const std::u16string& s, intcs index) {
        return GetNumericValue(s[static_cast<size_t>(index)]);
    }

    /** <summary>Gets the Unicode category of a character.</summary> */
    static UnicodeCategory GetUnicodeCategory(charcs ch) {
        return GetUnicodeCategory(static_cast<intcs>(ch));
    }

    static UnicodeCategory GetUnicodeCategory(const std::u16string& s, intcs index) {
        return GetUnicodeCategory(s[static_cast<size_t>(index)]);
    }

    static UnicodeCategory GetUnicodeCategory(intcs codePoint) {
        // wchar_t is 16-bit on Windows; clamp non-BMP code points to avoid narrowing UB.
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
