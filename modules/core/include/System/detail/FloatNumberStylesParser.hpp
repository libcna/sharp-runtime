// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstddef>
#include <string>
#include <string_view>
#include "System/ArgumentException.hpp"
#include "System/Globalization/NumberStyles.hpp"

namespace System::detail {

/**
 * @brief The `NumberStyles` grammar for floating-point text, over the invariant spelling.
 *
 * `Single`/`Double::Parse(string)` implement .NET's default style, `NumberStyles::Float |
 * AllowThousands`, directly. The overloads that take an explicit style need the narrower and the
 * wider grammars too: `NumberStyles::Float` alone must **reject** a group separator, `Number` must
 * accept a trailing sign, `Currency` a currency symbol and parentheses, and `None` nothing but
 * digits. This parser applies those flags to text that has already been normalised to the
 * invariant spelling (`NumberFormatText::NormalizeForParsing`) and emits the canonical
 * `[-]digits[.digits][E[+-]digits]` form the existing invariant core accepts.
 *
 * It is the floating-point sibling of `IntegerNumberStylesParser`, transcribed from the same
 * `Number.Parsing.Common.cs` state machine: leading white, a leading sign or an opening
 * parenthesis, a currency symbol before or after the digits, digits with optional group
 * separators, a decimal point, an exponent, a trailing sign or closing parenthesis, trailing
 * white. Whitespace is the ASCII set .NET's `IsWhite` admits (U+0009 to U+000D and U+0020).
 */
struct FloatNumberStylesParser {
    /**
     * @brief .NET's `NumberFormatInfo.ValidateParseStyleFloatingPoint`, transcribed.
     *
     * @throws System::ArgumentException for a bit outside `NumberStyles::Any`, and for
     *         `AllowHexSpecifier` or `AllowBinarySpecifier`, which floating-point parsing does not
     *         support.
     */
    static void ValidateParseStyleFloatingPoint(System::Globalization::NumberStyles style) {
        using System::Globalization::NumberStyles;
        constexpr auto invalidMask = static_cast<NumberStyles>(~0x7FF);
        if ((style & invalidMask) != NumberStyles::None) {
            throw System::ArgumentException("An undefined NumberStyles value is being used.", "style");
        }
        if ((style & NumberStyles::AllowHexSpecifier) != NumberStyles::None) {
            throw System::ArgumentException(
                "The number style AllowHexSpecifier is not supported on floating point data types.");
        }
        if ((style & NumberStyles::AllowBinarySpecifier) != NumberStyles::None) {
            throw System::ArgumentException(
                "The number style AllowBinarySpecifier is not supported on floating point data types.");
        }
    }

    /**
     * @brief Applies @p style to @p text and produces the canonical invariant form.
     *
     * @param text      Invariant-spelled input.
     * @param style     The styles to admit.
     * @param canonical Receives `[-]digits[.digits][E[+-]digits]` on success.
     * @return true when @p text is a number under @p style; false otherwise (the caller then
     *         checks the NaN/Infinity symbols, which .NET matches outside the grammar).
     */
    static bool TryCanonicalize(std::string_view text, System::Globalization::NumberStyles style,
                                std::string& canonical) {
        using System::Globalization::NumberStyles;
        const auto has = [style](NumberStyles flag) {
            return (style & flag) != NumberStyles::None;
        };
        std::size_t i = 0;
        const std::size_t n = text.size();

        if (has(NumberStyles::AllowLeadingWhite)) {
            while (i < n && isWhite(text[i])) ++i;
        }

        bool negative = false;
        bool signSeen = false;
        bool parenthesis = false;
        bool currencySeen = false;

        // Sign, parenthesis and currency symbol may appear in either order before the digits.
        for (int pass = 0; pass < 2; ++pass) {
            if (!signSeen && has(NumberStyles::AllowLeadingSign) && i < n && (text[i] == '-' || text[i] == '+')) {
                negative = text[i] == '-';
                signSeen = true;
                ++i;
                skipWhiteIf(text, i, has(NumberStyles::AllowLeadingWhite));
            } else if (!signSeen && has(NumberStyles::AllowParentheses) && i < n && text[i] == '(') {
                negative = true;
                signSeen = true;
                parenthesis = true;
                ++i;
                skipWhiteIf(text, i, has(NumberStyles::AllowLeadingWhite));
            } else if (!currencySeen && has(NumberStyles::AllowCurrencySymbol) && startsWithCurrency(text, i)) {
                currencySeen = true;
                i += currencyLength();
                skipWhiteIf(text, i, has(NumberStyles::AllowLeadingWhite));
            }
        }

        std::string integerDigits;
        std::string fractionDigits;
        bool anyDigit = false;
        bool lastWasDigit = false;
        while (i < n) {
            const char c = text[i];
            if (c >= '0' && c <= '9') {
                integerDigits.push_back(c);
                anyDigit = true;
                lastWasDigit = true;
                ++i;
            } else if (c == ',' && has(NumberStyles::AllowThousands) && lastWasDigit) {
                lastWasDigit = false;
                ++i;
            } else {
                break;
            }
        }
        if (i < n && text[i] == '.' && has(NumberStyles::AllowDecimalPoint)) {
            ++i;
            while (i < n && text[i] >= '0' && text[i] <= '9') {
                fractionDigits.push_back(text[i]);
                anyDigit = true;
                ++i;
            }
        }
        if (!anyDigit) return false;

        std::string exponent;
        if (i < n && (text[i] == 'e' || text[i] == 'E') && has(NumberStyles::AllowExponent)) {
            std::size_t j = i + 1;
            std::string sign;
            if (j < n && (text[j] == '-' || text[j] == '+')) {
                sign.push_back(text[j]);
                ++j;
            }
            std::string digits;
            while (j < n && text[j] >= '0' && text[j] <= '9') {
                digits.push_back(text[j]);
                ++j;
            }
            // An 'E' not followed by digits is not an exponent; .NET backs up and treats it as
            // trailing text, which then fails the grammar below.
            if (!digits.empty()) {
                exponent = "E" + sign + digits;
                i = j;
            }
        }

        // Trailing currency, sign or closing parenthesis, in either order, with white between.
        for (int pass = 0; pass < 2; ++pass) {
            skipWhiteIf(text, i, has(NumberStyles::AllowTrailingWhite));
            if (!currencySeen && has(NumberStyles::AllowCurrencySymbol) && startsWithCurrency(text, i)) {
                currencySeen = true;
                i += currencyLength();
            } else if (!signSeen && has(NumberStyles::AllowTrailingSign) && i < n && (text[i] == '-' || text[i] == '+')) {
                negative = text[i] == '-';
                signSeen = true;
                ++i;
            } else if (parenthesis && i < n && text[i] == ')') {
                parenthesis = false;
                ++i;
            }
        }
        if (parenthesis) return false;
        skipWhiteIf(text, i, has(NumberStyles::AllowTrailingWhite));
        if (i != n) return false;

        canonical.clear();
        if (negative) canonical.push_back('-');
        canonical += integerDigits.empty() ? std::string("0") : integerDigits;
        if (!fractionDigits.empty()) {
            canonical.push_back('.');
            canonical += fractionDigits;
        }
        canonical += exponent;
        return true;
    }

private:
    static bool isWhite(char c) noexcept {
        return c == ' ' || (c >= '\t' && c <= '\r');
    }

    static void skipWhiteIf(std::string_view text, std::size_t& i, bool allowed) noexcept {
        if (!allowed) return;
        while (i < text.size() && isWhite(text[i])) ++i;
    }

    // U+00A4 CURRENCY SIGN, the invariant currency symbol, in UTF-8.
    static constexpr std::string_view currency() noexcept { return "\xc2\xa4"; }
    static constexpr std::size_t currencyLength() noexcept { return currency().size(); }
    static bool startsWithCurrency(std::string_view text, std::size_t i) noexcept {
        return text.compare(i, currencyLength(), currency()) == 0;
    }
};

} // namespace System::detail
