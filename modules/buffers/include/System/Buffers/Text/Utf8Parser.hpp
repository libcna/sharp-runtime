// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/FormatException.hpp"
#include "System/Buffers/StandardFormat.hpp"
#include "System/ReadOnlySpan.hpp"

namespace System::Buffers::Text {

using SharpRuntime::intcs;

/**
 * @brief Methods to parse common data types from UTF-8 encoded text.
 *
 * C++ counterpart of .NET System.Buffers.Text.Utf8Parser.
 * All methods are static TryParse overloads that read from a ReadOnlySpan&lt;byte&gt;
 * and return true on success or false if the input is not syntactically valid or
 * overflows the target type. On such a failure BOTH outputs are written: bytesConsumed
 * is set to 0 and value is set to a value-initialized T, matching .NET's documented
 * contract and its own `FalseExit: bytesConsumed = default; value = default;` blocks.
 * The one exception is an invalid @p standardFormat, which throws FormatException and
 * writes NEITHER output -- .NET does the same deliberately, bypassing C#'s
 * definite-assignment rule with `Unsafe.SkipInit` before throwing (ParserHelpers.cs).
 * See docs/TryOutputFailureContractPlan.md (ticket #1872 / SR-AUD-085 / CCF-014).
 *
 * Covers bool and all integer types (byte/sbyte/short/ushort/int/uint/long/ulong)
 * with the 'G' (default), 'D', 'N', and 'X' format specifiers. .NET's Guid,
 * DateTime, DateTimeOffset, TimeSpan, and floating-point/decimal TryParse
 * overloads are not yet implemented in this port — a documented gap, not a
 * silent omission (see NEXT.md).
 */
class Utf8Parser {
public:
    Utf8Parser() = delete;

    // -----------------------------------------------------------------------
    // bool
    // -----------------------------------------------------------------------

    /**
     * @brief Parses a Boolean at the start of a UTF-8 string.
     *
     * C++ counterpart of .NET Utf8Parser.TryParse(ReadOnlySpan&lt;byte&gt;, out bool, out int, char).
     * Accepts "True"/"False" and "true"/"false" case-insensitively regardless of which
     * of 'G'/'l'/default was requested — the format is validated but treated identically,
     * matching .NET's documented behavior.
     * @param source        UTF-8 input.
     * @param value         Receives the parsed value on success, or false on failure.
     * @param bytesConsumed Receives the number of bytes consumed on success, or 0 on failure.
     * @param standardFormat Format character ('G', 'l', or '\0' for default).
     * @return true on success; false if input is not a valid boolean.
     * @throws FormatException if @p standardFormat is not '\0', 'G', or 'l'. Neither
     *         output is written on this path, matching .NET.
     */
    static bool TryParse(System::ReadOnlySpan<uint8_t> source, bool& value, intcs& bytesConsumed,
                         char standardFormat = '\0') {
        if (!(standardFormat == '\0' || standardFormat == 'G' || standardFormat == 'l'))
            throw System::FormatException("Format specifier was invalid.");

        bytesConsumed = 0;
        intcs len = source.getLengthProperty();
        const uint8_t* p = source.getPointer();

        auto iequal = [](const uint8_t* a, const char* b, intcs n) {
            for (intcs i = 0; i < n; ++i)
                if ((a[i] | 0x20) != static_cast<uint8_t>(b[i] | 0x20)) return false;
            return true;
        };

        if (len >= 4 && iequal(p, "true", 4)) {
            value = true; bytesConsumed = 4; return true;
        }
        if (len >= 5 && iequal(p, "false", 5)) {
            value = false; bytesConsumed = 5; return true;
        }
        return fail(value, bytesConsumed);
    }

    // -----------------------------------------------------------------------
    // Integer types
    // -----------------------------------------------------------------------

    /**
     * @brief Parses a uint8_t from UTF-8 text.
     * C++ counterpart of .NET Utf8Parser.TryParse(ReadOnlySpan&lt;byte&gt;, out byte, out int, char).
     * Formats supported: G/g (default), D/d, N/n, X/x.
     */
    static bool TryParse(System::ReadOnlySpan<uint8_t> source, uint8_t& value, intcs& bytesConsumed,
                         char standardFormat = '\0') {
        int64_t sv = 0; uint64_t uv = 0; intcs n = 0;
        if (!tryParseIntegerCore(source, standardFormat, 1, false, sv, uv, n) || uv > 0xFFu) {
            return fail(value, bytesConsumed);
        }
        value = static_cast<uint8_t>(uv); bytesConsumed = n; return true;
    }

    /**
     * @brief Parses an int8_t from UTF-8 text.
     * C++ counterpart of .NET Utf8Parser.TryParse(ReadOnlySpan&lt;byte&gt;, out sbyte, out int, char).
     * Formats supported: G/g (default), D/d, N/n, X/x.
     */
    static bool TryParse(System::ReadOnlySpan<uint8_t> source, int8_t& value, intcs& bytesConsumed,
                         char standardFormat = '\0') {
        int64_t sv = 0; uint64_t uv = 0; intcs n = 0;
        if (!tryParseIntegerCore(source, standardFormat, 1, true, sv, uv, n) || sv < -128 || sv > 127) {
            return fail(value, bytesConsumed);
        }
        value = static_cast<int8_t>(sv); bytesConsumed = n; return true;
    }

    /**
     * @brief Parses a uint16_t from UTF-8 text.
     * C++ counterpart of .NET Utf8Parser.TryParse(ReadOnlySpan&lt;byte&gt;, out ushort, out int, char).
     * Formats supported: G/g (default), D/d, N/n, X/x.
     */
    static bool TryParse(System::ReadOnlySpan<uint8_t> source, uint16_t& value, intcs& bytesConsumed,
                         char standardFormat = '\0') {
        int64_t sv = 0; uint64_t uv = 0; intcs n = 0;
        if (!tryParseIntegerCore(source, standardFormat, 2, false, sv, uv, n) || uv > 0xFFFFu) {
            return fail(value, bytesConsumed);
        }
        value = static_cast<uint16_t>(uv); bytesConsumed = n; return true;
    }

    /**
     * @brief Parses an int16_t from UTF-8 text.
     * C++ counterpart of .NET Utf8Parser.TryParse(ReadOnlySpan&lt;byte&gt;, out short, out int, char).
     * Formats supported: G/g (default), D/d, N/n, X/x.
     */
    static bool TryParse(System::ReadOnlySpan<uint8_t> source, int16_t& value, intcs& bytesConsumed,
                         char standardFormat = '\0') {
        int64_t sv = 0; uint64_t uv = 0; intcs n = 0;
        if (!tryParseIntegerCore(source, standardFormat, 2, true, sv, uv, n) || sv < -32768 || sv > 32767) {
            return fail(value, bytesConsumed);
        }
        value = static_cast<int16_t>(sv); bytesConsumed = n; return true;
    }

    /**
     * @brief Parses a uint32_t from UTF-8 text.
     * C++ counterpart of .NET Utf8Parser.TryParse(ReadOnlySpan&lt;byte&gt;, out uint, out int, char).
     * Formats supported: G/g (default), D/d, N/n, X/x.
     */
    static bool TryParse(System::ReadOnlySpan<uint8_t> source, uint32_t& value, intcs& bytesConsumed,
                         char standardFormat = '\0') {
        int64_t sv = 0; uint64_t uv = 0; intcs n = 0;
        if (!tryParseIntegerCore(source, standardFormat, 4, false, sv, uv, n) || uv > 0xFFFFFFFFu) {
            return fail(value, bytesConsumed);
        }
        value = static_cast<uint32_t>(uv); bytesConsumed = n; return true;
    }

    /**
     * @brief Parses an int32_t from UTF-8 text.
     * C++ counterpart of .NET Utf8Parser.TryParse(ReadOnlySpan&lt;byte&gt;, out int, out int, char).
     * Formats supported: G/g (default), D/d, N/n, X/x.
     */
    static bool TryParse(System::ReadOnlySpan<uint8_t> source, int32_t& value, intcs& bytesConsumed,
                         char standardFormat = '\0') {
        int64_t sv = 0; uint64_t uv = 0; intcs n = 0;
        if (!tryParseIntegerCore(source, standardFormat, 4, true, sv, uv, n) || sv < INT32_MIN || sv > INT32_MAX) {
            return fail(value, bytesConsumed);
        }
        value = static_cast<int32_t>(sv); bytesConsumed = n; return true;
    }

    /**
     * @brief Parses a uint64_t from UTF-8 text.
     * C++ counterpart of .NET Utf8Parser.TryParse(ReadOnlySpan&lt;byte&gt;, out ulong, out int, char).
     * Formats supported: G/g (default), D/d, N/n, X/x.
     */
    static bool TryParse(System::ReadOnlySpan<uint8_t> source, uint64_t& value, intcs& bytesConsumed,
                         char standardFormat = '\0') {
        int64_t sv = 0; uint64_t uv = 0; intcs n = 0;
        if (!tryParseIntegerCore(source, standardFormat, 8, false, sv, uv, n)) {
            return fail(value, bytesConsumed);
        }
        value = uv; bytesConsumed = n; return true;
    }

    /**
     * @brief Parses an int64_t from UTF-8 text.
     * C++ counterpart of .NET Utf8Parser.TryParse(ReadOnlySpan&lt;byte&gt;, out long, out int, char).
     * Formats supported: G/g (default), D/d, N/n, X/x.
     */
    static bool TryParse(System::ReadOnlySpan<uint8_t> source, int64_t& value, intcs& bytesConsumed,
                         char standardFormat = '\0') {
        int64_t sv = 0; uint64_t uv = 0; intcs n = 0;
        if (!tryParseIntegerCore(source, standardFormat, 8, true, sv, uv, n)) {
            return fail(value, bytesConsumed);
        }
        value = sv; bytesConsumed = n; return true;
    }

private:
    /**
     * @brief Normalizes both outputs for a non-throwing failure and returns false.
     *
     * Single failure boundary for every TryParse overload above. .NET writes both
     * outputs at one labelled `FalseExit:` per parser -- `bytesConsumed = default;
     * value = default;` (Utf8Parser.Integer.Signed.D.cs:79-83 and its siblings,
     * Utf8Parser.Boolean.cs:52-54) -- because C#'s definite-assignment rule makes the
     * assignment mandatory for an `out` parameter. Porting `out T` to a C++ `T&` lost
     * that guarantee, and the port kept only the bytesConsumed half, so a checked
     * failure left the caller's own previous value in place and was indistinguishable
     * from a stale success. Routing every exit through here makes it structurally
     * impossible to normalize one output without the other, which is how the two
     * halves diverged in the first place.
     *
     * Deliberately NOT used on the FormatException path: .NET leaves both outputs
     * unwritten there (`ParserHelpers.TryParseThrowFormatException` calls
     * `Unsafe.SkipInit` on both before throwing), and a permanent test pins that a
     * caller sentinel survives the throw.
     *
     * Ticket #1872 / SR-AUD-085 / CCF-014; see docs/TryOutputFailureContractPlan.md.
     */
    template<typename T>
    static bool fail(T& value, intcs& bytesConsumed) {
        value = T{};
        bytesConsumed = 0;
        return false;
    }

    static bool tryParseUInt(System::ReadOnlySpan<uint8_t> src, uint64_t& out, intcs& n) {
        intcs len = src.getLengthProperty();
        const uint8_t* p = src.getPointer();
        if (len == 0 || p[0] < '0' || p[0] > '9') return false;
        uint64_t v = 0; n = 0;
        while (n < len && p[n] >= '0' && p[n] <= '9') {
            // The common "next = v*10+digit; if (next < v) overflow" idiom is NOT airtight for
            // a multiply-by-10 accumulator: confirmed via brute-force testing against
            // __uint128_t ground truth that it falsely ACCEPTS some genuinely-overflowing
            // 21-digit inputs (e.g. "184467440737095516159" = UINT64_MAX*10+9) as valid,
            // silently returning a wrapped, wrong value instead of failing to parse. Checking
            // `v > (UINT64_MAX - digit) / 10` *before* multiplying is the standard airtight
            // idiom (never itself risks overflow, and was verified against 200k randomized
            // digit strings with zero false accepts/rejects).
            uint64_t digit = static_cast<uint64_t>(p[n] - '0');
            if (v > (UINT64_MAX - digit) / 10) return false; // would overflow
            v = v * 10 + digit; ++n;
        }
        out = v; return n > 0;
    }

    static bool tryParseInt(System::ReadOnlySpan<uint8_t> src, int64_t& out, intcs& n) {
        intcs len = src.getLengthProperty();
        const uint8_t* p = src.getPointer();
        bool neg = (len > 0 && p[0] == '-');
        System::ReadOnlySpan<uint8_t> rest(p + (neg ? 1 : 0), len - (neg ? 1 : 0));
        uint64_t v = 0; intcs digits = 0;
        if (!tryParseUInt(rest, v, digits)) return false;
        if (neg) {
            if (v > static_cast<uint64_t>(INT64_MAX) + 1) return false;
            // CCF-004 class A (defined wrap): `v` is the unsigned magnitude and the check
            // above deliberately admits INT64_MAX+1, i.e. exactly INT64_MIN's magnitude.
            // Converting that to a signed int64_t FIRST and then negating it is undefined
            // behaviour -- "-9223372036854775808" reported
            // `negation of -9223372036854775808 cannot be represented in type 'long int'`
            // (SR-AUD-084). Negating in the unsigned domain is always defined and produces
            // the same bit pattern, so the parsed value and bytesConsumed are unchanged.
            // Not only the int64_t overload reaches this: every narrower signed width shares
            // this site through tryParseIntegerCore, whose magnitude limit is INT64_MAX+1
            // regardless of byteWidth, with the width check applied by the caller afterwards.
            out = static_cast<int64_t>(-v);
        } else {
            if (v > static_cast<uint64_t>(INT64_MAX)) return false;
            out = static_cast<int64_t>(v);
        }
        n = digits + (neg ? 1 : 0);
        return true;
    }

    static bool tryParseHex(System::ReadOnlySpan<uint8_t> src, uint64_t widthMaxValue, uint64_t& value, intcs& n) {
        intcs len = src.getLengthProperty();
        const uint8_t* p = src.getPointer();
        if (len < 1) return false;
        auto hexVal = [](uint8_t c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        };
        int d0 = hexVal(p[0]);
        if (d0 < 0) return false;
        uint64_t v = static_cast<uint64_t>(d0);
        intcs i = 1;
        for (; i < len; ++i) {
            int d = hexVal(p[i]);
            if (d < 0) break;
            if (v > (widthMaxValue >> 4)) return false; // would overflow this type's width
            v = (v << 4) | static_cast<uint64_t>(d);
        }
        value = v; n = i;
        return true;
    }

    // Parses the 'N' format grammar: optional sign, digit groups separated by ',', and an
    // optional '.' followed by any run of '0' digits (a non-zero fractional digit is an error).
    static bool tryParseGrouped(System::ReadOnlySpan<uint8_t> src, bool allowMinus,
                                uint64_t& magnitude, bool& neg, intcs& n) {
        intcs len = src.getLengthProperty();
        const uint8_t* p = src.getPointer();
        if (len < 1) return false;

        intcs index = 0;
        neg = false;
        uint8_t c = p[index];
        if (allowMinus && c == '-') {
            neg = true; ++index;
            if (index >= len) return false;
            c = p[index];
        } else if (c == '+') {
            ++index;
            if (index >= len) return false;
            c = p[index];
        }

        uint64_t answer;
        if (c == '.') goto fractionalNoLeading;
        if (c < '0' || c > '9') return false;
        answer = static_cast<uint64_t>(c - '0');

        while (true) {
            ++index;
            if (index >= len) goto done;
            c = p[index];
            if (c == ',') continue;
            if (c == '.') goto fractionalDigits;
            if (c < '0' || c > '9') goto done;
            {
                // Same airtight-vs-not distinction as tryParseUInt's accumulator above: check
                // before multiplying, not after.
                uint64_t digit = static_cast<uint64_t>(c - '0');
                if (answer > (UINT64_MAX - digit) / 10) return false; // overflow
                answer = answer * 10 + digit;
            }
        }

    fractionalNoLeading:
        answer = 0;
        ++index;
        if (index >= len) return false;
        if (p[index] != '0') return false;

    fractionalDigits:
        do {
            ++index;
            if (index >= len) goto done;
            c = p[index];
        } while (c == '0');
        if (c >= '0' && c <= '9') return false; // non-zero digit in the fractional part
        goto done;

    done:
        magnitude = answer;
        n = index;
        return true;
    }

    // Dispatches on standardFormat, parsing into signedOut (for isSigned==true) or unsignedOut
    // (for isSigned==false). Callers apply the final type-width range check afterward, matching
    // this file's existing convention for the default 'D' path.
    static bool tryParseIntegerCore(System::ReadOnlySpan<uint8_t> source, char standardFormat, intcs byteWidth,
                                     bool isSigned, int64_t& signedOut, uint64_t& unsignedOut, intcs& consumed) {
        char symbol = (standardFormat == '\0') ? 'G' : standardFormat;

        switch (symbol | 0x20) {
            case 'g':
            case 'd':
            case 'r':
                return isSigned ? tryParseInt(source, signedOut, consumed)
                                 : tryParseUInt(source, unsignedOut, consumed);

            case 'x': {
                uint64_t widthMax = (byteWidth >= 8) ? ~0ULL : ((1ULL << (byteWidth * 8)) - 1);
                uint64_t v = 0;
                if (!tryParseHex(source, widthMax, v, consumed) || v > widthMax) return false;
                if (isSigned) {
                    switch (byteWidth) {
                        case 1: signedOut = static_cast<int8_t>(static_cast<uint8_t>(v)); break;
                        case 2: signedOut = static_cast<int16_t>(static_cast<uint16_t>(v)); break;
                        case 4: signedOut = static_cast<int32_t>(static_cast<uint32_t>(v)); break;
                        default: signedOut = static_cast<int64_t>(v); break;
                    }
                } else {
                    unsignedOut = v;
                }
                return true;
            }

            case 'n': {
                uint64_t magnitude = 0; bool neg = false;
                if (!tryParseGrouped(source, isSigned, magnitude, neg, consumed)) return false;
                if (isSigned) {
                    if (neg) {
                        if (magnitude > static_cast<uint64_t>(INT64_MAX) + 1) return false;
                        // CCF-004 class A, identical to tryParseInt's site above and for the
                        // same reason: "-9,223,372,036,854,775,808" reported
                        // `negation of -9223372036854775808 cannot be represented in type
                        // 'long int'` (SR-AUD-084's second site). Negate in the unsigned
                        // domain; the parsed value and bytesConsumed are unchanged.
                        signedOut = static_cast<int64_t>(-magnitude);
                    } else {
                        if (magnitude > static_cast<uint64_t>(INT64_MAX)) return false;
                        signedOut = static_cast<int64_t>(magnitude);
                    }
                } else {
                    if (neg) return false;
                    unsignedOut = magnitude;
                }
                return true;
            }

            default:
                throw System::FormatException("Format specifier was invalid.");
        }
    }
};

} // namespace System::Buffers::Text
