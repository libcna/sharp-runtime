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
// AllowHexSpecifier), NumberStyles.BinaryNumber (AllowLeadingWhite | AllowTrailingWhite |
// AllowBinarySpecifier -- added 2026-07-14, see TryParseBinaryCore below; previously defined in
// NumberStyles.hpp but silently ignored by every caller, so BinaryNumber input fell through to
// decimal parsing instead of throwing or parsing as binary), and -- as of an earlier extension --
// AllowTrailingSign, AllowParentheses, AllowDecimalPoint, AllowThousands, and
// AllowCurrencySymbol (so NumberStyles.Number and NumberStyles.Currency are now both fully
// supported). Verified against real .NET's TryParseNumber/TryStringToNumber
// (Number.Parsing.Common.cs) and TryNumberBufferToBinaryInteger/
// TryParseBinaryIntegerHexOrBinaryNumberStyle (Number.Parsing.cs). Since this port has no
// culture-aware NumberFormatInfo (@p provider is always ignored -- see every Parse/TryParse
// overload's own doc-comment), the separators used are NumberFormatInfo.InvariantInfo's fixed
// defaults: '.' for both NumberDecimalSeparator and CurrencyDecimalSeparator, ',' for both
// NumberGroupSeparator and CurrencyGroupSeparator, and U+00A4 ("¤", the international currency
// sign) for CurrencySymbol -- verified against NumberFormatInfo.cs's field initializers, not
// guessed.
//
// Leading/trailing whitespace tolerance (AllowLeadingWhite/AllowTrailingWhite) is interleaved
// with token matching (sign/parentheses/currency symbol), not just skipped once before/after all
// tokens -- fixed 2026-07-14 after confirming a real discrepancy against .NET/Mono: e.g.
// int.Parse("123-  ", NumberStyles.Number) == -123 requires whitespace to be tolerated AFTER a
// trailing sign, not just immediately after the digits.
//
// AllowExponent IS implemented, as of ticket #2268 (2026-08-17). It was previously recorded here
// as a "non-gap" on the grounds that only NumberStyles.Float/.HexFloat carry the flag and those
// "don't apply to integer types". Ticket #2267 (finding SR-AUD-177) measured that premise and it
// is WRONG, on this port's own evidence: NumberStyles::Any is 0x1FF and INCLUDES AllowExponent
// 0x80 (NumberStyles.hpp:34), it is a public style accepted by every integer Parse/TryParse
// overload, and the flag can be passed on its own besides. Real .NET parses "1E2", "1E+2" and
// "1e2" as 100; this parser returned FormatException for all three, and for
// Int32::Parse("1E2", NumberStyles::Any) as well.
//
// #2268 was the deferred VERIFICATION, not an approval: the gap was never disputed, but writing
// the grammar to this file's standard needed .NET's exact rule for how an exponent folds into
// number.Scale and how that interacts with TryNumberBufferToBinaryInteger's "significant digits
// exceed scale" rule. Both are now transcribed rather than inferred, and .NET's own test suite
// pins the decisive rows:
//
//   Int32Tests.cs:353-358   "1E2"/"1E+2"/"1e2" -> 100, "1E0" -> 1, "-1E2" -> -100,
//                           "(1E2)" with AllowParentheses -> -100
//   Int32Tests.cs:473       "1E23" under NumberStyles.Integer -> FormatException (no flag, no
//                           exponent -- the 'E' is simply trailing garbage)
//   Int32Tests.cs:546-548   "65E10", "65E+10" -> OverflowException,
//                           and "65E-1" -> OverflowException.  <-- the row that settles the
//                           negative-exponent question the ticket was blocked on
//   Int32Tests.cs:665       "2E10" -> OverflowException
//
// The model, from Number.Parsing.Common.cs:98-232 and Number.Parsing.cs:150-208, is a digit
// buffer plus a scale:
//
//   * `digits` holds the SIGNIFICANT digits: leading zeros are never stored, and for
//     NumberBufferKind.Integer a TRAILING zero does not advance DigitsCount either
//     (`if ((ch != '0') || (number.Kind != NumberBufferKind.Integer)) digEnd = digCount + 1;`
//     -- Number.Parsing.Common.cs:109-112). So "100" is digits "1" with Scale 3, not "100".
//   * `Scale` counts digits before the decimal separator, and decrements for each leading zero
//     AFTER it (":126-129" and ":146-149").
//   * The exponent is added to Scale wholesale (":212"), with an overflow cap: at 100,000,000 the
//     exponent becomes int.MaxValue and Scale is reset to 0 first (":191-203"), which makes any
//     exponent of that magnitude overflow whichever way it is signed.
//   * The value is then `digits * 10^(Scale - DigitsCount)`, and the conversion FAILS -- as
//     OverflowException, never FormatException -- when `Scale > MaxDigitCount` or
//     `Scale < DigitsCount` (Number.Parsing.cs:157).
//
// That single rule decides every case the ticket named: "1.5E1" is digits "15" with Scale 2, so
// 15; "100E-2" is digits "1" with Scale 1, so 1; "1E-2" is digits "1" with Scale -1, and -1 < 1
// is an OverflowException. It is also the rule the pre-existing "123.5 overflows" quirk
// documented below already followed, so the exponent is an extension of that model rather than
// a second one beside it.
//
// ONE DELIBERATE DEVIATION, and it is a NON-CHANGE rather than a new one. Read literally, the
// same rule makes an all-zero magnitude with a fractional part overflow: "0.0" leaves Scale at
// -1 (the fractional zero takes the `Scale--` branch, because StateNonZero was never set by the
// leading zero) against DigitsCount 0, and -1 < 0 fails. So does "000.000", and so does "0E-2".
// This port keeps returning 0 for an all-zero magnitude. The reason is not disagreement: it is
// that .NET's own test suite pins no such row, the reading is a source trace that cannot be
// executed here, and turning `Parse("0.0", NumberStyles::Number)` -- an input that looks valid,
// is valid for every other numeric type, and works today -- into an OverflowException is a
// narrowing SR-AUD-177 never asked for. It is recorded as follow-up ticket #2356 rather than
// smuggled in beside a widening. This mirrors the unsigned-negative deviation already documented
// below: the port declines one .NET edge case whose only effect is which exception a degenerate
// input raises.
//
// Style-mask validation is a SEPARATE finding (SR-AUD-178, approval ticket #2269) and NOT the
// same defect: it is a missing precondition check rather than a missing grammar production, and
// repairing it TIGHTENS what the parser accepts where AllowExponent WIDENS it. .NET's
// ValidateParseStyleInteger does not reject AllowExponent -- the flag is inside the valid mask --
// so neither repair implies or blocks the other.
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

    // .NET checks `Scale > TInteger.MaxDigitCount` per target type. This shared core produces a
    // 64-bit magnitude and every caller applies its own range check afterwards, so the widest
    // value here -- UInt64Precision, Number.NumberBuffer.cs:22 -- is the equivalent bound: a
    // scale a narrower type cannot hold still fails, just one step later and with the same
    // OverflowException. It also stops a scale of a hundred million from driving the
    // trailing-zero expansion below into a loop that would never finish.
    static constexpr long long kMaxSignificantScale = 20;

    /**
     * @brief .NET's `NumberBuffer` digits-and-scale state, shared by the signed and unsigned
     *        cores so the two grammars cannot drift apart.
     *
     * The value a successful scan represents is `magnitude * 10^(scale - digitsCount)`; see the
     * class doc-comment for the transcription and its citations.
     */
    struct DigitScan {
        uint64_t  magnitude   = 0;      ///< The significant digits, as a number (.NET's `Digits`).
        int       digitsCount = 0;      ///< .NET's `number.DigitsCount`.
        long long scale       = 0;      ///< .NET's `number.Scale`.
        bool      any         = false;  ///< .NET's `StateDigits` — at least one digit was seen.
        bool      overflowed  = false;  ///< The significant digits do not fit in 64 bits.
    };

    /**
     * @brief Consumes digits, an optional decimal separator, group separators and — when the
     *        style allows one — an exponent, advancing @p i past everything it consumed.
     *
     * Transcribed from `Number.Parsing.Common.cs:98-219`. Two details are easy to get wrong and
     * are both deliberate here: a zero is only *stored* once a nonzero digit has been seen
     * (leading zeros never enter the buffer at all), and a **trailing** zero does not advance
     * `digitsCount` for an integer buffer — it becomes part of the value only if a later nonzero
     * digit follows it. That is what makes `"100"` digits `"1"` with scale 3 rather than digits
     * `"100"` with scale 3, and it is what lets `"100E-2"` be exactly 1.
     */
    static void ScanDigitsAndExponent(const std::string& s, std::size_t& i, NumberStyles style,
                                      DigitScan& scan) {
        const std::size_t n = s.size();
        const bool allowThousands    = (style & NumberStyles::AllowThousands)    != NumberStyles::None;
        const bool allowDecimalPoint = (style & NumberStyles::AllowDecimalPoint) != NumberStyles::None;
        const bool allowExponent     = (style & NumberStyles::AllowExponent)     != NumberStyles::None;

        bool sawDecimal = false, sawNonZero = false;
        int  pendingZeros = 0;  // stored in .NET's buffer, but `digEnd` has not passed them yet
        while (i < n) {
            if (std::isdigit(static_cast<unsigned char>(s[i]))) {
                const unsigned digit = static_cast<unsigned>(s[i] - '0');
                scan.any = true;
                if (digit != 0 || sawNonZero) {
                    if (digit != 0) {
                        // A nonzero digit makes every zero since the last one significant.
                        for (; pendingZeros > 0; --pendingZeros) {
                            if (scan.magnitude > UINT64_MAX / 10) scan.overflowed = true;
                            else scan.magnitude *= 10;
                            ++scan.digitsCount;
                        }
                        if (scan.magnitude > (UINT64_MAX - digit) / 10) scan.overflowed = true;
                        else scan.magnitude = scan.magnitude * 10 + digit;
                        ++scan.digitsCount;
                    } else {
                        ++pendingZeros;
                    }
                    if (!sawDecimal) ++scan.scale;
                    sawNonZero = true;
                } else if (sawDecimal) {
                    --scan.scale;  // a leading zero after the separator, e.g. "0.001"
                }
                ++i;
            } else if (allowDecimalPoint && !sawDecimal && s[i] == kDecimalSeparator) {
                sawDecimal = true; ++i;
            } else if (allowThousands && scan.any && !sawDecimal && s[i] == kGroupSeparator) {
                ++i;
            } else break;
        }

        if (!scan.any || !allowExponent) return;
        if (i >= n || (s[i] != 'E' && s[i] != 'e')) return;

        const std::size_t rewind = i;
        ++i;
        bool negativeExponent = false;
        if (i < n && (s[i] == '+' || s[i] == '-')) { negativeExponent = (s[i] == '-'); ++i; }
        if (i >= n || !std::isdigit(static_cast<unsigned char>(s[i]))) {
            // .NET rewinds to the 'E' (`p = temp`), which then fails as trailing garbage --
            // a FormatException, not an OverflowException. So "1E" and "1E+" are malformed.
            i = rewind;
            return;
        }

        long long exponent = 0;
        while (i < n && std::isdigit(static_cast<unsigned char>(s[i]))) {
            if (exponent >= 100'000'000) {
                // .NET's own cap: the exponent becomes int.MaxValue and the scale is reset to
                // zero first, so an exponent this large overflows whichever way it is signed.
                exponent   = 2147483647LL;
                scan.scale = 0;
                while (i < n && std::isdigit(static_cast<unsigned char>(s[i]))) ++i;
                break;
            }
            exponent = exponent * 10 + (s[i] - '0');
            ++i;
        }
        scan.scale += negativeExponent ? -exponent : exponent;
    }

    /**
     * @brief `TryNumberBufferToBinaryInteger`'s preconditions and its trailing-zero expansion.
     *
     * Transcribed from `Number.Parsing.cs:150-187`. Returns false when the scan cannot be
     * represented, which the callers report as **OverflowException** — never FormatException,
     * however small the number looks. `"65E-1"` is 6.5 and fails here, which is exactly what
     * .NET's own `Int32Tests.cs:548` pins.
     *
     * @note An all-zero magnitude with a non-positive scale is the one case this port does not
     *       follow; see the class doc-comment's deviation note and ticket #2356.
     */
    static bool TryFoldScale(DigitScan& scan) {
        if (scan.overflowed) return false;
        if (scan.scale > kMaxSignificantScale) return false;
        if (scan.digitsCount == 0) { scan.magnitude = 0; return true; }
        if (scan.scale < scan.digitsCount) return false;
        for (long long k = scan.scale - scan.digitsCount; k > 0; --k) {
            if (scan.magnitude > UINT64_MAX / 10) return false;
            scan.magnitude *= 10;
        }
        return true;
    }

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
        // AllowThousands, AllowDecimalPoint and AllowExponent are read by ScanDigitsAndExponent.

        // Leading tokens: whitespace (if allowed), a sign, an opening '(', and/or a currency
        // symbol, interleaved in any order, each token matched at most once (mirrors real
        // .NET's TryParseNumber prefix loop, which re-checks for skippable whitespace after
        // every token match rather than only once before any token -- verified against
        // Number.Parsing.Common.cs and empirically cross-checked, 2026-07-14: e.g.
        // int.Parse("123-  ", NumberStyles.Number) == -123, which requires whitespace to be
        // tolerated AFTER the trailing sign, not just before it; the mirrored bug existed here
        // on the leading side too, e.g. a leading currency symbol followed by whitespace before
        // the digits, such as "¤  123").
        bool negative = false, haveSign = false, haveParen = false, haveCurrency = false;
        for (bool matched = true; matched; ) {
            matched = false;
            if (allowLeadingWhite && i < n && std::isspace(static_cast<unsigned char>(s[i]))) {
                while (i < n && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
                matched = true;
            } else if (allowLeadingSign && !haveSign && i < n && (s[i] == '+' || s[i] == '-')) {
                negative = (s[i] == '-'); haveSign = true; ++i; matched = true;
            } else if (allowParens && !haveSign && i < n && s[i] == '(') {
                haveParen = true; haveSign = true; negative = true; ++i; matched = true;
            } else if (allowCurrency && !haveCurrency &&
                       s.compare(i, kCurrencySymbol.size(), kCurrencySymbol) == 0) {
                haveCurrency = true; i += kCurrencySymbol.size(); matched = true;
            }
        }

        DigitScan scan;
        ScanDigitsAndExponent(s, i, style, scan);
        if (!scan.any) return false;

        if (allowTrailingWhite)
            while (i < n && std::isspace(static_cast<unsigned char>(s[i]))) ++i;

        // Trailing tokens: same three kinds as the leading loop, plus a closing ')', with
        // whitespace (if allowed) interleaved after any of them too -- real .NET's trailing
        // whitespace has no gating condition tying it only to the position right after the
        // digits (verified 2026-07-14, see the leading-loop comment above for the full
        // rationale and empirical cross-check).
        for (bool matched = true; matched; ) {
            matched = false;
            if (allowTrailingSign && !haveSign && i < n && (s[i] == '+' || s[i] == '-')) {
                negative = (s[i] == '-'); haveSign = true; ++i; matched = true;
            } else if (haveParen && i < n && s[i] == ')') {
                haveParen = false; ++i; matched = true;
            } else if (allowCurrency && !haveCurrency &&
                       s.compare(i, kCurrencySymbol.size(), kCurrencySymbol) == 0) {
                haveCurrency = true; i += kCurrencySymbol.size(); matched = true;
            } else if (allowTrailingWhite && i < n && std::isspace(static_cast<unsigned char>(s[i]))) {
                while (i < n && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
                matched = true;
            }
        }

        if (haveParen) return false;   // '(' opened but never closed
        if (i != n) return false;
        // Format-grammar validity takes precedence over the scale overflow, matching real .NET's
        // own documented precedence rule -- checked only after the two lines above.
        if (!TryFoldScale(scan)) { overflowed = true; return true; }
        const uint64_t magnitude = scan.magnitude;

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
        // AllowThousands, AllowDecimalPoint and AllowExponent are read by ScanDigitsAndExponent.

        // Leading tokens: whitespace (if allowed), a literal '+', and/or a currency symbol,
        // interleaved in any order -- see TryParseSignedCore's identical leading-loop comment
        // for the full whitespace-interleaving rationale (2026-07-14). The negative-token
        // rejection is checked AFTER this loop settles (not just once at the very start) so it
        // still correctly rejects a '-'/'(' appearing after whitespace or a currency symbol has
        // already been consumed, e.g. "¤-123".
        //
        // `haveSign` is shared across BOTH the leading and trailing token loops below (mirroring
        // TryParseSignedCore's single shared `haveSign`), so at most one '+' total is ever
        // consumed -- fixed 2026-07-14 (duplicated-implementation audit finding): without this
        // guard, the leading loop alone would re-match '+' on every iteration with no "already
        // consumed a sign" check, so e.g. UInt32::TryParse("++5", NumberStyles::Integer, ...)
        // incorrectly returned true with result 5, and "5++" (multiple trailing signs) was
        // likewise wrongly accepted -- confirmed via a standalone repro before this fix.
        bool haveSign = false, haveCurrency = false;
        for (bool matched = true; matched; ) {
            matched = false;
            if (allowLeadingWhite && i < n && std::isspace(static_cast<unsigned char>(s[i]))) {
                while (i < n && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
                matched = true;
            } else if (allowLeadingSign && !haveSign && i < n && s[i] == '+') { haveSign = true; ++i; matched = true; }
            else if (allowCurrency && !haveCurrency &&
                     s.compare(i, kCurrencySymbol.size(), kCurrencySymbol) == 0) {
                haveCurrency = true; i += kCurrencySymbol.size(); matched = true;
            }
        }
        if (i < n && (s[i] == '-' || (allowParens && s[i] == '('))) return false;

        DigitScan scan;
        ScanDigitsAndExponent(s, i, style, scan);
        if (!scan.any) return false;

        if (allowTrailingWhite)
            while (i < n && std::isspace(static_cast<unsigned char>(s[i]))) ++i;

        // Trailing tokens: same interleaved-whitespace treatment as the leading loop above (and
        // TryParseSignedCore's trailing loop) -- see those comments for the full rationale. The
        // trailing-minus rejection is checked after the loop settles, so it still correctly
        // rejects a '-' appearing after a trailing '+'/currency/whitespace has been consumed.
        // `haveSign` is the SAME flag the leading loop above uses, so a sign already consumed on
        // either side blocks a second one on the other -- see that loop's comment for why.
        for (bool matched = true; matched; ) {
            matched = false;
            if (allowTrailingSign && !haveSign && i < n && s[i] == '+') { haveSign = true; ++i; matched = true; }
            else if (allowCurrency && !haveCurrency &&
                     s.compare(i, kCurrencySymbol.size(), kCurrencySymbol) == 0) {
                haveCurrency = true; i += kCurrencySymbol.size(); matched = true;
            } else if (allowTrailingWhite && i < n && std::isspace(static_cast<unsigned char>(s[i]))) {
                while (i < n && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
                matched = true;
            }
        }
        if (i < n && s[i] == '-') return false; // trailing minus: same rejection as leading

        if (i != n) return false;
        if (!TryFoldScale(scan)) { overflowed = true; return true; }

        result = static_cast<SharpRuntime::ulongcs>(scan.magnitude);
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
        // Skip leading zeros BEFORE counting toward maxDigits, matching real .NET's
        // TryParseBinaryIntegerHexOrBinaryNumberStyle (Number.Parsing.cs) -- confirmed via a
        // real discrepancy, 2026-07-14: this port previously counted every hex character
        // including leading zeros, so a harmlessly zero-padded value like
        // "00000000FFFFFFFF" (16 chars: 8 leading zeros + 8 significant digits) incorrectly
        // overflowed for UInt32 (maxDigits=8) even though its SIGNIFICANT digit count fits.
        while (i < n && s[i] == '0') ++i;
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

    // Parses the BinaryNumber-style subset (pure '0'/'1' digits, no sign, optional surrounding
    // whitespace) into a raw 64-bit bit pattern -- the binary counterpart of TryParseHexCore
    // above, added 2026-07-14 to close a silently-wrong-value gap: NumberStyles.AllowBinary-
    // Specifier/BinaryNumber were defined in NumberStyles.hpp but never checked anywhere, so a
    // caller passing NumberStyles.BinaryNumber got a DECIMAL reinterpretation instead of a
    // FormatException/correct binary parse (e.g. Int32::TryParse("101", NumberStyles::
    // BinaryNumber, ...) silently returned 101, not 5) -- violating this project's own "never
    // silently return a wrong value" rule (CLAUDE.md). Mirrors real .NET's
    // TryParseBinaryIntegerHexOrBinaryNumberStyle (Number.Parsing.cs), including skipping
    // leading zeros before counting toward @p maxDigits (here: max BIT count, e.g. 32 for
    // Int32 -- NOT divided by 4 the way hex's maxDigits is, since each binary digit is one bit).
    static bool TryParseBinaryCore(const std::string& s, NumberStyles style,
                                    uint64_t& bits, intcs maxDigits, bool& tooManyDigits) {
        tooManyDigits = false;
        std::size_t i = 0, n = s.size();
        const bool allowLeadingWhite  = (style & NumberStyles::AllowLeadingWhite)  != NumberStyles::None;
        const bool allowTrailingWhite = (style & NumberStyles::AllowTrailingWhite) != NumberStyles::None;

        if (allowLeadingWhite)
            while (i < n && std::isspace(static_cast<unsigned char>(s[i]))) ++i;

        if (i >= n || (s[i] != '0' && s[i] != '1')) return false;

        bits = 0;
        while (i < n && s[i] == '0') ++i; // skip leading zeros before counting -- see TryParseHexCore
        intcs digitCount = 0;
        while (i < n && (s[i] == '0' || s[i] == '1')) {
            bits = (bits << 1) | static_cast<unsigned>(s[i] - '0');
            ++digitCount;
            ++i;
        }
        if (digitCount > maxDigits) { tooManyDigits = true; return false; }

        if (allowTrailingWhite)
            while (i < n && std::isspace(static_cast<unsigned char>(s[i]))) ++i;

        return i == n;
    }
};

} // namespace System::detail
