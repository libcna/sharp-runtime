// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/Buffers/Text/Utf8Parser.hpp"
#include "System/FormatException.hpp"

using System::Buffers::Text::Utf8Parser;
using System::ReadOnlySpan;
using System::FormatException;

static ReadOnlySpan<uint8_t> span(const char* s) {
    return ReadOnlySpan<uint8_t>(reinterpret_cast<const uint8_t*>(s), static_cast<int>(strlen(s)));
}

TEST(Utf8ParserTest, ParseBoolTrue) {
    bool v = false; int n = 0;
    EXPECT_TRUE(Utf8Parser::TryParse(span("True"), v, n));
    EXPECT_TRUE(v);
    EXPECT_EQ(n, 4);
}

TEST(Utf8ParserTest, ParseBoolFalse) {
    bool v = true; int n = 0;
    EXPECT_TRUE(Utf8Parser::TryParse(span("False"), v, n));
    EXPECT_FALSE(v);
    EXPECT_EQ(n, 5);
}

TEST(Utf8ParserTest, ParseBoolCaseInsensitive) {
    bool v = false; int n = 0;
    EXPECT_TRUE(Utf8Parser::TryParse(span("true"), v, n));
    EXPECT_TRUE(v);
}

TEST(Utf8ParserTest, ParseBoolInvalid) {
    bool v = false; int n = 0;
    EXPECT_FALSE(Utf8Parser::TryParse(span("yes"), v, n));
    EXPECT_EQ(n, 0);
}

TEST(Utf8ParserTest, ParseInt32Positive) {
    int32_t v = 0; int n = 0;
    EXPECT_TRUE(Utf8Parser::TryParse(span("12345"), v, n));
    EXPECT_EQ(v, 12345);
    EXPECT_EQ(n, 5);
}

TEST(Utf8ParserTest, ParseInt32Negative) {
    int32_t v = 0; int n = 0;
    EXPECT_TRUE(Utf8Parser::TryParse(span("-42"), v, n));
    EXPECT_EQ(v, -42);
    EXPECT_EQ(n, 3);
}

TEST(Utf8ParserTest, ParseInt32Zero) {
    int32_t v = 1; int n = 0;
    EXPECT_TRUE(Utf8Parser::TryParse(span("0"), v, n));
    EXPECT_EQ(v, 0);
}

TEST(Utf8ParserTest, ParseUInt64Large) {
    uint64_t v = 0; int n = 0;
    EXPECT_TRUE(Utf8Parser::TryParse(span("18446744073709551615"), v, n));
    EXPECT_EQ(v, UINT64_MAX);
}

TEST(Utf8ParserTest, ParseUInt64_OneOverMax_Fails) {
    uint64_t v = 0; int n = 0;
    EXPECT_FALSE(Utf8Parser::TryParse(span("18446744073709551616"), v, n));
    EXPECT_EQ(n, 0);
}

TEST(Utf8ParserTest, ParseUInt64_MultiWrapOverflow_Fails) {
    // "184467440737095516159" (UINT64_MAX*10+9, a 21-digit genuinely-overflowing value) was
    // previously silently ACCEPTED by an overflow check that only tested `next < v` *after*
    // the multiply -- not airtight for a multiply-by-10 accumulator; the true value wraps
    // around 2^64 more than once for inputs in this range, landing back above the previous
    // accumulator value by coincidence. Verified via a 200k-case randomized brute-force
    // cross-check against __uint128_t ground truth before/after the fix.
    uint64_t v = 0; int n = 0;
    EXPECT_FALSE(Utf8Parser::TryParse(span("184467440737095516159"), v, n));
    EXPECT_EQ(n, 0);
}

TEST(Utf8ParserTest, ParseUInt64_N_MultiWrapOverflow_Fails) {
    // Same overflow-detection idiom, same bug, in the 'N' (grouped) format's digit
    // accumulator.
    uint64_t v = 0; int n = 0;
    EXPECT_FALSE(Utf8Parser::TryParse(span("184,467,440,737,095,516,159"), v, n, 'N'));
    EXPECT_EQ(n, 0);
}

TEST(Utf8ParserTest, ParseUInt8) {
    uint8_t v = 0; int n = 0;
    EXPECT_TRUE(Utf8Parser::TryParse(span("255"), v, n));
    EXPECT_EQ(v, 255);
}

TEST(Utf8ParserTest, ParseInvalidNotDigit) {
    int32_t v = 0; int n = 0;
    EXPECT_FALSE(Utf8Parser::TryParse(span("abc"), v, n));
}

TEST(Utf8ParserTest, ParseInvalidNotDigit_BytesConsumedIsZero) {
    int32_t v = 99; int n = 42;
    EXPECT_FALSE(Utf8Parser::TryParse(span("abc"), v, n));
    EXPECT_EQ(n, 0);
}

// ===========================================================================
// Format specifiers: D, X (hex), N (grouping), bad format
// ===========================================================================

TEST(Utf8ParserTest, ParseBool_BadSpecifier_Throws) {
    bool v = false; int n = 0;
    EXPECT_THROW(Utf8Parser::TryParse(span("true"), v, n, 'X'), FormatException);
}

TEST(Utf8ParserTest, ParseInt32_D_Explicit) {
    int32_t v = 0; int n = 0;
    EXPECT_TRUE(Utf8Parser::TryParse(span("123"), v, n, 'D'));
    EXPECT_EQ(v, 123);
    EXPECT_EQ(n, 3);
}

TEST(Utf8ParserTest, ParseUInt32_X_Lowercase) {
    uint32_t v = 0; int n = 0;
    EXPECT_TRUE(Utf8Parser::TryParse(span("abcd"), v, n, 'x'));
    EXPECT_EQ(v, 0xABCDu);
    EXPECT_EQ(n, 4);
}

TEST(Utf8ParserTest, ParseInt8_X_ReinterpretsBitPattern) {
    int8_t v = 0; int n = 0;
    EXPECT_TRUE(Utf8Parser::TryParse(span("FF"), v, n, 'X'));
    EXPECT_EQ(v, -1);
}

TEST(Utf8ParserTest, ParseUInt32_X_StopsAtNonHexChar) {
    uint32_t v = 0; int n = 0;
    EXPECT_TRUE(Utf8Parser::TryParse(span("1Fg"), v, n, 'X'));
    EXPECT_EQ(v, 0x1Fu);
    EXPECT_EQ(n, 2);
}

TEST(Utf8ParserTest, ParseInt32_N_WithGrouping) {
    int32_t v = 0; int n = 0;
    EXPECT_TRUE(Utf8Parser::TryParse(span("32,767"), v, n, 'N'));
    EXPECT_EQ(v, 32767);
    EXPECT_EQ(n, 6);
}

TEST(Utf8ParserTest, ParseInt32Negative_N_WithGrouping) {
    int32_t v = 0; int n = 0;
    EXPECT_TRUE(Utf8Parser::TryParse(span("-1,234,567"), v, n, 'N'));
    EXPECT_EQ(v, -1234567);
}

TEST(Utf8ParserTest, ParseInt32_N_WithZeroFraction) {
    int32_t v = 0; int n = 0;
    EXPECT_TRUE(Utf8Parser::TryParse(span("32,767.00"), v, n, 'N'));
    EXPECT_EQ(v, 32767);
    EXPECT_EQ(n, 9);
}

TEST(Utf8ParserTest, ParseInt32_N_NonZeroFraction_Fails) {
    int32_t v = 0; int n = 0;
    EXPECT_FALSE(Utf8Parser::TryParse(span("32,767.5"), v, n, 'N'));
}

TEST(Utf8ParserTest, ParseUInt32_N_MinusSign_Fails) {
    uint32_t v = 0; int n = 0;
    EXPECT_FALSE(Utf8Parser::TryParse(span("-5"), v, n, 'N'));
}

TEST(Utf8ParserTest, ParseInt32_BadSpecifier_Throws) {
    int32_t v = 0; int n = 0;
    EXPECT_THROW(Utf8Parser::TryParse(span("42"), v, n, 'Q'), FormatException);
}

// ---------------------------------------------------------------------------
// CCF-004 / SR-AUD-084 — Utf8Parser signed-minimum negation (ticket #1835)
//
// `tryParseInt` (Utf8Parser.hpp) and the grouped `N` branch of
// `tryParseIntegerCore` both deliberately admit a negative magnitude of
// `INT64_MAX + 1` — exactly `INT64_MIN`'s magnitude — and then converted it to a signed
// `int64_t` and negated it, which is undefined behaviour. Both now negate in the unsigned
// domain. The audit recorded NO direct test for either minimum input; these are it.
//
// CCF-004 class A: no value and no `bytesConsumed` may change. Every expectation is the
// value measured before the change (build-probe/1835_prefix_values.log).
//
// The narrower signed widths matter here even though they have no site of their own: they
// share these two sites through `tryParseIntegerCore`, whose magnitude limit is
// `INT64_MAX + 1` regardless of `byteWidth`, with the width check applied by the CALLER
// afterwards. So `TryParse("-9223372036854775808", int32_t&, …)` executed the undefined
// negation and only THEN returned false. Those cases are covered below.
// ---------------------------------------------------------------------------

TEST(Utf8ParserDefinedArithmeticTests, Int64MinimumParsesExactlyInBothFormats) {
    {
        int64_t v = 0; int n = -1;
        ASSERT_TRUE(Utf8Parser::TryParse(span("-9223372036854775808"), v, n));
        EXPECT_EQ(v, INT64_MIN);
        EXPECT_EQ(n, 20);
    }
    {   // The explicit 'D' specifier reaches the same code as the default.
        int64_t v = 0; int n = -1;
        ASSERT_TRUE(Utf8Parser::TryParse(span("-9223372036854775808"), v, n, 'D'));
        EXPECT_EQ(v, INT64_MIN);
        EXPECT_EQ(n, 20);
    }
    {   // The grouped 'N' branch is the second, independent site.
        int64_t v = 0; int n = -1;
        ASSERT_TRUE(Utf8Parser::TryParse(span("-9,223,372,036,854,775,808"), v, n, 'N'));
        EXPECT_EQ(v, INT64_MIN);
        EXPECT_EQ(n, 26);
    }
}

TEST(Utf8ParserDefinedArithmeticTests, Int64BoundariesInBothDirectionsAreUnchanged) {
    int64_t v = 0; int n = -1;
    // One inside the minimum.
    ASSERT_TRUE(Utf8Parser::TryParse(span("-9223372036854775807"), v, n));
    EXPECT_EQ(v, INT64_MIN + 1);
    EXPECT_EQ(n, 20);
    // One past the minimum: still rejected, so the unsigned negation did not widen the
    // accepted domain by one.
    EXPECT_FALSE(Utf8Parser::TryParse(span("-9223372036854775809"), v, n));
    EXPECT_EQ(n, 0);
    EXPECT_FALSE(Utf8Parser::TryParse(span("-9,223,372,036,854,775,809"), v, n, 'N'));
    EXPECT_EQ(n, 0);
    // The positive end is untouched by the change but is asserted so the guard cannot
    // have been made asymmetric.
    ASSERT_TRUE(Utf8Parser::TryParse(span("9223372036854775807"), v, n));
    EXPECT_EQ(v, INT64_MAX);
    EXPECT_EQ(n, 19);
    EXPECT_FALSE(Utf8Parser::TryParse(span("9223372036854775808"), v, n));
    ASSERT_TRUE(Utf8Parser::TryParse(span("9,223,372,036,854,775,807"), v, n, 'N'));
    EXPECT_EQ(v, INT64_MAX);
    EXPECT_EQ(n, 25);
}

TEST(Utf8ParserDefinedArithmeticTests, OrdinaryAndZeroMagnitudeNegativesAreUnchanged) {
    int64_t v = 42; int n = -1;
    // A zero magnitude is the degenerate case of the changed expression.
    ASSERT_TRUE(Utf8Parser::TryParse(span("-0"), v, n));
    EXPECT_EQ(v, 0);
    EXPECT_EQ(n, 2);
    ASSERT_TRUE(Utf8Parser::TryParse(span("0"), v, n));
    EXPECT_EQ(v, 0);
    EXPECT_EQ(n, 1);
    ASSERT_TRUE(Utf8Parser::TryParse(span("-42"), v, n));
    EXPECT_EQ(v, -42);
    EXPECT_EQ(n, 3);
    ASSERT_TRUE(Utf8Parser::TryParse(span("-1,234"), v, n, 'N'));
    EXPECT_EQ(v, -1234);
    EXPECT_EQ(n, 6);
}

TEST(Utf8ParserDefinedArithmeticTests, NarrowSignedWidthsShareTheSiteAndKeepTheirOwnLimits) {
    // Each narrow width at its OWN minimum: must still succeed, both formats.
    {
        int32_t v = 0; int n = -1;
        ASSERT_TRUE(Utf8Parser::TryParse(span("-2147483648"), v, n));
        EXPECT_EQ(v, INT32_MIN);
        EXPECT_EQ(n, 11);
        ASSERT_TRUE(Utf8Parser::TryParse(span("-2,147,483,648"), v, n, 'N'));
        EXPECT_EQ(v, INT32_MIN);
        EXPECT_EQ(n, 14);
        EXPECT_FALSE(Utf8Parser::TryParse(span("-2147483649"), v, n));
    }
    {
        int16_t v = 0; int n = -1;
        ASSERT_TRUE(Utf8Parser::TryParse(span("-32768"), v, n));
        EXPECT_EQ(v, INT16_MIN);
        EXPECT_EQ(n, 6);
        ASSERT_TRUE(Utf8Parser::TryParse(span("-32,768"), v, n, 'N'));
        EXPECT_EQ(v, INT16_MIN);
        EXPECT_EQ(n, 7);
        EXPECT_FALSE(Utf8Parser::TryParse(span("-32769"), v, n));
    }
    {
        int8_t v = 0; int n = -1;
        ASSERT_TRUE(Utf8Parser::TryParse(span("-128"), v, n));
        EXPECT_EQ(v, INT8_MIN);
        EXPECT_EQ(n, 4);
        EXPECT_FALSE(Utf8Parser::TryParse(span("-129"), v, n));
    }
    // The int64 minimum string into a narrower width: this is the call that used to execute
    // the undefined negation and only afterwards fail the caller's width check.
    {
        int32_t v32 = 0; int16_t v16 = 0; int8_t v8 = 0; int n = -1;
        EXPECT_FALSE(Utf8Parser::TryParse(span("-9223372036854775808"), v32, n));
        EXPECT_EQ(n, 0);
        EXPECT_FALSE(Utf8Parser::TryParse(span("-9223372036854775808"), v16, n));
        EXPECT_EQ(n, 0);
        EXPECT_FALSE(Utf8Parser::TryParse(span("-9223372036854775808"), v8, n));
        EXPECT_EQ(n, 0);
        EXPECT_FALSE(Utf8Parser::TryParse(span("-9,223,372,036,854,775,808"), v32, n, 'N'));
        EXPECT_EQ(n, 0);
    }
}

TEST(Utf8ParserDefinedArithmeticTests, UnsignedAndHexPathsAreUntouched) {
    // Neither ever negated; asserted so the change cannot have leaked across the branch.
    {
        uint64_t v = 0; int n = -1;
        ASSERT_TRUE(Utf8Parser::TryParse(span("18446744073709551615"), v, n));
        EXPECT_EQ(v, UINT64_MAX);
        EXPECT_EQ(n, 20);
        EXPECT_FALSE(Utf8Parser::TryParse(span("-1"), v, n));
    }
    {
        int64_t v = 0; int n = -1;
        ASSERT_TRUE(Utf8Parser::TryParse(span("8000000000000000"), v, n, 'X'));
        EXPECT_EQ(v, INT64_MIN);
        EXPECT_EQ(n, 16);
    }
}
