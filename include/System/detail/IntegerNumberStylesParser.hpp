// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cctype>
#include <cstdint>
#include <string>
#include <string_view>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/Globalization/NumberStyles.hpp"

namespace System::detail {

using SharpRuntime::intcs;
using System::Globalization::NumberStyles;

// Shared NumberStyles-aware integer parsing core, used by Int16/Int32/Int64/UInt16/UInt32/
// UInt64/SByte/Byte's Parse(string, NumberStyles, IFormatProvider*) overloads. Mirrors real
// .NET's Number.Parsing.cs architecture of one shared parsing core per bit-width class with
// per-type range checks layered on top, rather than 8 independent hand-written parsers.
//
// SCOPE: implements NumberStyles.Integer (AllowLeadingWhite | AllowTrailingWhite |
// AllowLeadingSign), NumberStyles.HexNumber (AllowLeadingWhite | AllowTrailingWhite |
// AllowHexSpecifier), and -- as of the extension below -- AllowTrailingSign, AllowParentheses,
// AllowDecimalPoint, AllowThousands, and AllowCurrencySymbol (so NumberStyles.Number and
// NumberStyles.Currency are now both fully supported). Verified against real .NET's
// TryParseNumber/TryStringToNumber (Number.Parsing.Common.cs) and TryNumberBufferToBinaryInteger
// (Number.Parsing.cs). Since this port has no culture-aware NumberFormatInfo (@p provider is
// always ignored -- see every Parse/TryParse overload's own doc-comment), the separators used
// are NumberFormatInfo.InvariantInfo's fixed defaults: '.' for both NumberDecimalSeparator and
// CurrencyDecimalSeparator, ',' for both NumberGroupSeparator and CurrencyGroupSeparator, and
// U+00A4 ("¤", the international currency sign) for CurrencySymbol -- verified against
// NumberFormatInfo.cs's field initializers, not guessed.
//
// AllowExponent is still NOT implemented (out of scope: NumberStyles.Number/.Currency don't
// include it either in real .NET -- only .Float/.HexFloat do, which don't apply to integer
// types -- so this is a non-gap, not a scope reduction).
//
// A faithful, deliberate quirk carried over from real .NET: a decimal point followed by only
// zero digits (e.g. "123.00") parses successfully, but a decimal point followed by any NONZERO
// digit (e.g. "123.5") throws OverflowException, not FormatException -- confirmed against
// TryNumberBufferToBinaryInteger, which rejects any digit buffer whose significant-digit count
// exceeds its integer scale unconditionally, regardless of magnitude. Format-grammar violations
// (e.g. trailing garbage after the number) still take precedence over this overflow, matching
// real .NET's own explicitly-commented precedence rule.
//
// A deliberate, documented DEVIATION from real .NET for the *unsigned* parsers specifically:
// real .NET's general (non-Integer-style) parse path allows a negative-indicating token (a
// literal '-' or a closed '(...)') to reach an unsigned type's buffer conversion, where it then
// fails with OverflowException (via TryNumberBufferToBinaryInteger's
// `!TInteger.IsSigned && number.IsNegative` check) rather than FormatException -- except for an
// all-zero magnitude ("-0"), which real .NET actually accepts as positive zero. This port does
// not replicate that: consistent with this file's pre-existing Integer-style convention (a
// leading '-' was already a hard, immediate reject before this extension), any negative-
// indicating token in an unsigned parse is rejected outright as a format failure. This keeps
// unsigned-parsing behavior uniform across every style rather than importing one more real-.NET
// edge case whose only practical effect is which exception TYPE a clearly-invalid input throws.
struct IntegerNumberStylesParser {

    // Invariant-culture separators/symbol this port's Parse/TryParse grammar uses -- see the
    // class doc-comment above for why these exact values (matches NumberFormatInfo.InvariantInfo).
    static constexpr char kGroupSeparator = ',';
    static constexpr char kDecimalSeparator = '.';
    static constexpr std::string_view kCurrencySymbol = "\xC2\xA4"; // UTF-8 for U+00A4 "¤"

    // Parses the full Integer/Number/Currency-style grammar (sign, parentheses, currency
    // symbol, decimal digits, thousands separators, an optional trailing-zeros-only decimal
    // point, and surrounding whitespace, in any style-permitted combination) into a 64-bit
    // signed magnitude. Returns false on any grammar violation. `overflowed` is set true if the
    // digit sequence itself overflows int64_t's range, OR if a nonzero fractional digit was
    // present (see the class doc-comment's real-.NET-quirk note) -- distinct from the caller's
    // own final per-type range check.
    static bool TryParseSignedCore(const std::string& s, NumberStyles style,
                                    SharpRuntime::longcs& result, bool& overflowed) {
        overflowed = false;
        std::size_t i = 0, n = s.size();
        const bool allowLeadingWhite  = (style & NumberStyles::AllowLeadingWhite)  != NumberStyles::None;
        const bool allowTrailingWhite = (style & NumberStyles::AllowTrailingWhite) != NumberStyles::None;
        const bool allowLeadingSign   = (style & NumberStyles::AllowLeadingSign)   != NumberStyles::None;
        const bool allowTrailingSign  = (style & NumberStyles::AllowTrailingSign)  != NumberStyles::None;
        const bool allowParens        = (style & NumberStyles::AllowParentheses)   != NumberStyles::None;
        const bool allowCurrency      = (style & NumberStyles::AllowCurrencySymbol) != NumberStyles::None;
        const bool allowThousands     = (style & NumberStyles::AllowThousands)     != NumberStyles::None;
        const bool allowDecimalPoint  = (style & NumberStyles::AllowDecimalPoint)  != NumberStyles::None;

        if (allowLeadingWhite)
            while (i < n && std::isspace(static_cast<unsigned char>(s[i]))) ++i;

        // Leading tokens: a sign, an opening '(', and/or a currency symbol, in any order, each
        // matched at most once (mirrors real .NET's TryParseNumber prefix loop).
        bool negative = false, haveSign = false, haveParen = false, haveCurrency = false;
        for (bool matched = true; matched; ) {
            matched = false;
            if (allowLeadingSign && !haveSign && i < n && (s[i] == '+' || s[i] == '-')) {
                negative = (s[i] == '-'); haveSign = true; ++i; matched = true;
            } else if (allowParens && !haveSign && i < n && s[i] == '(') {
                haveParen = true; haveSign = true; negative = true; ++i; matched = true;
            } else if (allowCurrency && !haveCurrency &&
                       s.compare(i, kCurrencySymbol.size(), kCurrencySymbol) == 0) {
                haveCurrency = true; i += kCurrencySymbol.size(); matched = true;
            }
        }

        uint64_t magnitude = 0;
        bool any = false, sawDecimal = false, fracNonZero = false;
        while (i < n) {
            if (std::isdigit(static_cast<unsigned char>(s[i]))) {
                unsigned digit = static_cast<unsigned>(s[i] - '0');
                if (!sawDecimal) {
                    if (magnitude > (UINT64_MAX - digit) / 10) { overflowed = true; }
                    else { magnitude = magnitude * 10 + digit; }
                } else if (digit != 0) {
                    fracNonZero = true;
                }
                any = true;
                ++i;
            } else if (allowDecimalPoint && !sawDecimal && s[i] == kDecimalSeparator) {
                sawDecimal = true; ++i;
            } else if (allowThousands && any && !sawDecimal && s[i] == kGroupSeparator) {
                ++i;
            } else break;
        }
        if (!any) return false;

        if (allowTrailingWhite)
            while (i < n && std::isspace(static_cast<unsigned char>(s[i]))) ++i;

        // Trailing tokens: same three kinds as the leading loop, plus a closing ')'.
        for (bool matched = true; matched; ) {
            matched = false;
            if (allowTrailingSign && !haveSign && i < n && (s[i] == '+' || s[i] == '-')) {
                negative = (s[i] == '-'); haveSign = true; ++i; matched = true;
            } else if (haveParen && i < n && s[i] == ')') {
                haveParen = false; ++i; matched = true;
            } else if (allowCurrency && !haveCurrency &&
                       s.compare(i, kCurrencySymbol.size(), kCurrencySymbol) == 0) {
                haveCurrency = true; i += kCurrencySymbol.size(); matched = true;
            }
        }

        if (haveParen) return false;   // '(' opened but never closed
        if (i != n) return false;
        // Format-grammar validity takes precedence over the fractional-overflow quirk, matching
        // real .NET's own documented precedence rule -- checked only after the two lines above.
        if (fracNonZero) { overflowed = true; return true; }

        // Magnitude of Int64::MinValue (9223372036854775808) doesn't fit in a signed longcs,
        // but does fit as the unsigned two's-complement magnitude -- handle the boundary
        // explicitly rather than overflowing when negating.
        constexpr uint64_t kInt64MinMagnitude = 9223372036854775808ULL;
        if (negative) {
            if (magnitude > kInt64MinMagnitude) { overflowed = true; return true; }
            result = (magnitude == kInt64MinMagnitude)
                ? std::numeric_limits<SharpRuntime::longcs>::min()
                : -static_cast<SharpRuntime::longcs>(magnitude);
        } else {
            if (magnitude > static_cast<uint64_t>(std::numeric_limits<SharpRuntime::longcs>::max())) {
                overflowed = true; return true;
            }
            result = static_cast<SharpRuntime::longcs>(magnitude);
        }
        return true;
    }

    // Unsigned counterpart of TryParseSignedCore. NumberStyles.Integer/.Number/.Currency as
    // applied to an unsigned type still allow AllowLeadingSign/AllowTrailingSign in principle
    // for a literal "+" (matching real .NET's UInt32.Parse accepting a leading '+'), but any
    // token that would indicate a negative value ('-', or a closed "(...)") is always rejected
    // as a format failure -- see the class doc-comment's "deliberate DEVIATION" note for why
    // this port doesn't chase real .NET's negative-unsigned-throws-OverflowException quirk.
    static bool TryParseUnsignedCore(const std::string& s, NumberStyles style,
                                      SharpRuntime::ulongcs& result, bool& overflowed) {
        overflowed = false;
        std::size_t i = 0, n = s.size();
        const bool allowLeadingWhite  = (style & NumberStyles::AllowLeadingWhite)  != NumberStyles::None;
        const bool allowTrailingWhite = (style & NumberStyles::AllowTrailingWhite) != NumberStyles::None;
        const bool allowLeadingSign   = (style & NumberStyles::AllowLeadingSign)   != NumberStyles::None;
        const bool allowTrailingSign  = (style & NumberStyles::AllowTrailingSign)  != NumberStyles::None;
        const bool allowParens        = (style & NumberStyles::AllowParentheses)   != NumberStyles::None;
        const bool allowCurrency      = (style & NumberStyles::AllowCurrencySymbol) != NumberStyles::None;
        const bool allowThousands     = (style & NumberStyles::AllowThousands)     != NumberStyles::None;
        const bool allowDecimalPoint  = (style & NumberStyles::AllowDecimalPoint)  != NumberStyles::None;

        if (allowLeadingWhite)
            while (i < n && std::isspace(static_cast<unsigned char>(s[i]))) ++i;

        if (i < n && (s[i] == '-' || (allowParens && s[i] == '('))) return false;

        bool haveCurrency = false;
        for (bool matched = true; matched; ) {
            matched = false;
            if (allowLeadingSign && i < n && s[i] == '+') { ++i; matched = true; }
            else if (allowCurrency && !haveCurrency &&
                     s.compare(i, kCurrencySymbol.size(), kCurrencySymbol) == 0) {
                haveCurrency = true; i += kCurrencySymbol.size(); matched = true;
            }
        }

        uint64_t magnitude = 0;
        bool any = false, sawDecimal = false, fracNonZero = false;
        while (i < n) {
            if (std::isdigit(static_cast<unsigned char>(s[i]))) {
                unsigned digit = static_cast<unsigned>(s[i] - '0');
                if (!sawDecimal) {
                    if (magnitude > (UINT64_MAX - digit) / 10) { overflowed = true; }
                    else { magnitude = magnitude * 10 + digit; }
                } else if (digit != 0) {
                    fracNonZero = true;
                }
                any = true;
                ++i;
            } else if (allowDecimalPoint && !sawDecimal && s[i] == kDecimalSeparator) {
                sawDecimal = true; ++i;
            } else if (allowThousands && any && !sawDecimal && s[i] == kGroupSeparator) {
                ++i;
            } else break;
        }
        if (!any) return false;

        if (allowTrailingWhite)
            while (i < n && std::isspace(static_cast<unsigned char>(s[i]))) ++i;

        if (i < n && s[i] == '-') return false; // trailing minus: same rejection as leading

        for (bool matched = true; matched; ) {
            matched = false;
            if (allowTrailingSign && i < n && s[i] == '+') { ++i; matched = true; }
            else if (allowCurrency && !haveCurrency &&
                     s.compare(i, kCurrencySymbol.size(), kCurrencySymbol) == 0) {
                haveCurrency = true; i += kCurrencySymbol.size(); matched = true;
            }
        }

        if (i != n) return false;
        if (fracNonZero) { overflowed = true; return true; }

        result = static_cast<SharpRuntime::ulongcs>(magnitude);
        return true;
    }

    // Parses the HexNumber-style subset (pure hex digits, no sign, optional surrounding
    // whitespace) into a raw 64-bit bit pattern. The caller reinterprets/truncates that pattern
    // to its own type's width, matching real .NET's semantic that hex parsing produces a
    // two's-complement bit pattern rather than a signed magnitude+sign (e.g. parsing "FFFFFFFF"
    // as Int32 with NumberStyles.HexNumber yields -1, not an OverflowException).
    //
    // `tooManyDigits` distinguishes "grammatically valid hex, but more digits than the target
    // type's width allows" (real .NET throws OverflowException for this) from every other
    // grammar violation (real .NET throws FormatException). It is always set -- including on
    // the true-returning path -- so callers can rely on it without separately checking the
    // return value first.
    static bool TryParseHexCore(const std::string& s, NumberStyles style,
                                 uint64_t& bits, intcs maxDigits, bool& tooManyDigits) {
        tooManyDigits = false;
        std::size_t i = 0, n = s.size();
        const bool allowLeadingWhite  = (style & NumberStyles::AllowLeadingWhite)  != NumberStyles::None;
        const bool allowTrailingWhite = (style & NumberStyles::AllowTrailingWhite) != NumberStyles::None;

        if (allowLeadingWhite)
            while (i < n && std::isspace(static_cast<unsigned char>(s[i]))) ++i;

        if (i >= n || !std::isxdigit(static_cast<unsigned char>(s[i]))) return false;

        bits = 0;
        intcs digitCount = 0;
        while (i < n && std::isxdigit(static_cast<unsigned char>(s[i]))) {
            char c = s[i];
            unsigned v = (c >= '0' && c <= '9') ? static_cast<unsigned>(c - '0')
                       : (c >= 'a' && c <= 'f') ? static_cast<unsigned>(c - 'a' + 10)
                                                 : static_cast<unsigned>(c - 'A' + 10);
            bits = (bits << 4) | v;
            ++digitCount;
            ++i;
        }
        if (digitCount > maxDigits) { tooManyDigits = true; return false; } // matches real .NET's ThrowOverflowException

        if (allowTrailingWhite)
            while (i < n && std::isspace(static_cast<unsigned char>(s[i]))) ++i;

        return i == n;
    }
};

} // namespace System::detail
