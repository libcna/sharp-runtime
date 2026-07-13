// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cctype>
#include <cstdint>
#include <string>
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
// AllowLeadingSign) and NumberStyles.HexNumber (AllowLeadingWhite | AllowTrailingWhite |
// AllowHexSpecifier) -- the two styles explicitly prioritized by ticket 1717 as the
// highest-value subset ("hex parsing is a common game-code need"). AllowThousands,
// AllowDecimalPoint, AllowCurrencySymbol, AllowParentheses, and AllowExponent are NOT
// implemented: if the input string actually contains one of those characters (','  '.'  '('
// currency symbols, 'e'/'E'), parsing throws FormatException rather than silently accepting or
// silently misinterpreting it -- honest degradation (a clear parse failure) rather than a
// silently-wrong result. A plain digit string parses correctly regardless of which extra style
// flags are set, since those flags only ADD permitted grammar, they don't change how a
// plain-digit input is read. This is a deliberate, documented scope reduction (see
// POST_STABILIZATION_AUDIT.md finding #8 and ticket 1717's own acceptance criteria, which
// explicitly permits deferring the full grammar as long as the deferral is documented).
struct IntegerNumberStylesParser {

    // Parses the Integer-style subset (sign + decimal digits + optional surrounding whitespace)
    // into a 64-bit signed magnitude. Returns false on any grammar violation. `overflowed` is
    // set true if the digit sequence itself overflows int64_t's range (distinct from the
    // caller's own final per-type range check).
    static bool TryParseSignedCore(const std::string& s, NumberStyles style,
                                    SharpRuntime::longcs& result, bool& overflowed) {
        overflowed = false;
        std::size_t i = 0, n = s.size();
        const bool allowLeadingWhite  = (style & NumberStyles::AllowLeadingWhite)  != NumberStyles::None;
        const bool allowTrailingWhite = (style & NumberStyles::AllowTrailingWhite) != NumberStyles::None;
        const bool allowLeadingSign   = (style & NumberStyles::AllowLeadingSign)   != NumberStyles::None;

        if (allowLeadingWhite)
            while (i < n && std::isspace(static_cast<unsigned char>(s[i]))) ++i;

        bool negative = false;
        if (i < n && (s[i] == '+' || s[i] == '-')) {
            if (!allowLeadingSign) return false;
            negative = (s[i] == '-');
            ++i;
        }

        if (i >= n || !std::isdigit(static_cast<unsigned char>(s[i]))) return false;

        uint64_t magnitude = 0;
        bool any = false;
        while (i < n && std::isdigit(static_cast<unsigned char>(s[i]))) {
            unsigned digit = static_cast<unsigned>(s[i] - '0');
            if (magnitude > (UINT64_MAX - digit) / 10) { overflowed = true; }
            else { magnitude = magnitude * 10 + digit; }
            any = true;
            ++i;
        }
        if (!any) return false;

        if (allowTrailingWhite)
            while (i < n && std::isspace(static_cast<unsigned char>(s[i]))) ++i;

        if (i != n) return false;

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

    // Unsigned counterpart of TryParseSignedCore (no sign permitted; NumberStyles.Integer as
    // applied to an unsigned type still allows AllowLeadingSign in principle for a literal "+",
    // matching real .NET's UInt32.Parse accepting a leading '+' but rejecting '-').
    static bool TryParseUnsignedCore(const std::string& s, NumberStyles style,
                                      SharpRuntime::ulongcs& result, bool& overflowed) {
        overflowed = false;
        std::size_t i = 0, n = s.size();
        const bool allowLeadingWhite  = (style & NumberStyles::AllowLeadingWhite)  != NumberStyles::None;
        const bool allowTrailingWhite = (style & NumberStyles::AllowTrailingWhite) != NumberStyles::None;
        const bool allowLeadingSign   = (style & NumberStyles::AllowLeadingSign)   != NumberStyles::None;

        if (allowLeadingWhite)
            while (i < n && std::isspace(static_cast<unsigned char>(s[i]))) ++i;

        if (i < n && s[i] == '-') return false; // unsigned: a literal minus is always rejected
        if (i < n && s[i] == '+') {
            if (!allowLeadingSign) return false;
            ++i;
        }

        if (i >= n || !std::isdigit(static_cast<unsigned char>(s[i]))) return false;

        uint64_t magnitude = 0;
        bool any = false;
        while (i < n && std::isdigit(static_cast<unsigned char>(s[i]))) {
            unsigned digit = static_cast<unsigned>(s[i] - '0');
            if (magnitude > (UINT64_MAX - digit) / 10) { overflowed = true; }
            else { magnitude = magnitude * 10 + digit; }
            any = true;
            ++i;
        }
        if (!any) return false;

        if (allowTrailingWhite)
            while (i < n && std::isspace(static_cast<unsigned char>(s[i]))) ++i;

        if (i != n) return false;

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
