// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>

#include "System/Numerics/BigInteger.hpp"

using System::Numerics::BigInteger;

// ---------------------------------------------------------------------------
// Static constants
// ---------------------------------------------------------------------------

TEST(BigIntegerTests, ZeroConstant) {
    EXPECT_EQ(BigInteger::Zero.ToString(), "0");
    EXPECT_TRUE(BigInteger::Zero.getIsZeroProperty());
    EXPECT_EQ(BigInteger::Zero.Sign(), 0);
}

TEST(BigIntegerTests, OneConstant) {
    EXPECT_EQ(BigInteger::One.ToString(), "1");
    EXPECT_TRUE(BigInteger::One.getIsOneProperty());
    EXPECT_EQ(BigInteger::One.Sign(), 1);
}

TEST(BigIntegerTests, MinusOneConstant) {
    EXPECT_EQ(BigInteger::MinusOne.ToString(), "-1");
    EXPECT_TRUE(BigInteger::MinusOne.getIsNegativeProperty());
    EXPECT_EQ(BigInteger::MinusOne.Sign(), -1);
}

// ---------------------------------------------------------------------------
// Constructors & properties
// ---------------------------------------------------------------------------

TEST(BigIntegerTests, ConstructFromInt) {
    BigInteger a(42);
    EXPECT_EQ(a.ToString(), "42");
    EXPECT_FALSE(a.getIsNegativeProperty());
    EXPECT_EQ(a.Sign(), 1);
}

TEST(BigIntegerTests, ConstructFromNegativeInt) {
    BigInteger a(-7);
    EXPECT_EQ(a.ToString(), "-7");
    EXPECT_TRUE(a.getIsNegativeProperty());
    EXPECT_EQ(a.Sign(), -1);
}

TEST(BigIntegerTests, ConstructFromZeroInt) {
    BigInteger a(0);
    EXPECT_TRUE(a.getIsZeroProperty());
    EXPECT_EQ(a.Sign(), 0);
}

TEST(BigIntegerTests, ConstructFromLong) {
    BigInteger a(static_cast<int64_t>(9999999999LL));   // > int32 max
    EXPECT_EQ(a.ToString(), "9999999999");
}

TEST(BigIntegerTests, ConstructFromUInt64MaxViaLong) {
    // 2^63 - 1 = 9223372036854775807
    BigInteger a(static_cast<int64_t>(9223372036854775807LL));
    EXPECT_EQ(a.ToString(), "9223372036854775807");
}

// ---------------------------------------------------------------------------
// Parse / ToString roundtrip
// ---------------------------------------------------------------------------

TEST(BigIntegerTests, ParseZero) {
    EXPECT_EQ(BigInteger::Parse("0").ToString(), "0");
}

TEST(BigIntegerTests, ParsePositiveInteger) {
    EXPECT_EQ(BigInteger::Parse("42").ToString(), "42");
}

TEST(BigIntegerTests, ParseNegativeInteger) {
    EXPECT_EQ(BigInteger::Parse("-42").ToString(), "-42");
}

TEST(BigIntegerTests, ParseLargePositive) {
    // 81 decimal digits — well beyond uint64
    const std::string s = "123123123123123123123123123123123123123123123123123123123123123123123123123123123123123";
    EXPECT_EQ(BigInteger::Parse(s).ToString(), s);
}

TEST(BigIntegerTests, ParseLargeNegative) {
    const std::string s = "-123123123123123123123123123123123123123123123123123123123123123123123123123123123123123";
    EXPECT_EQ(BigInteger::Parse(s).ToString(), s);
}

TEST(BigIntegerTests, ParseUInt64MaxPlus1) {
    // 2^64 = 18446744073709551616
    EXPECT_EQ(BigInteger::Parse("18446744073709551616").ToString(), "18446744073709551616");
}

// ---------------------------------------------------------------------------
// Addition — official .NET vectors
// ---------------------------------------------------------------------------

TEST(BigIntegerTests, AddSmallIntegers) {
    EXPECT_EQ((BigInteger(3) + BigInteger(4)).ToString(), "7");
}

TEST(BigIntegerTests, AddNegatives) {
    EXPECT_EQ((BigInteger(-3) + BigInteger(-4)).ToString(), "-7");
}

TEST(BigIntegerTests, AddOppositeSignsPositiveResult) {
    // 7 + (-3) = 4
    EXPECT_EQ((BigInteger(7) + BigInteger(-3)).ToString(), "4");
}

TEST(BigIntegerTests, AddOppositeSignsNegativeResult) {
    // 3 + (-7) = -4
    EXPECT_EQ((BigInteger(3) + BigInteger(-7)).ToString(), "-4");
}

TEST(BigIntegerTests, AddWithZero) {
    BigInteger large = BigInteger::Parse("123123123123123123123123123123123123123123123123123123123123123123123123123123123123123");
    EXPECT_EQ((large + BigInteger::Zero).ToString(), large.ToString());
}

TEST(BigIntegerTests, AddUInt64MaxPlusOne) {
    // ulong.MaxValue + 1 = 18446744073709551616 (from .NET test vectors)
    BigInteger umax = BigInteger::Parse("18446744073709551615");
    EXPECT_EQ((umax + BigInteger::One).ToString(), "18446744073709551616");
}

TEST(BigIntegerTests, AddTwoUInt64Max) {
    // ulong.MaxValue + ulong.MaxValue = 36893488147419103230 (from .NET test vectors)
    BigInteger umax = BigInteger::Parse("18446744073709551615");
    EXPECT_EQ((umax + umax).ToString(), "36893488147419103230");
}

TEST(BigIntegerTests, AddLargeNumbers) {
    // From .NET AddTests: large + 123 = large000...246
    BigInteger large = BigInteger::Parse("123123123123123123123123123123123123123123123123123123123123123123123123123123123123123");
    EXPECT_EQ((large + BigInteger(123)).ToString(),
              "123123123123123123123123123123123123123123123123123123123123123123123123123123123123246");
}

// ---------------------------------------------------------------------------
// Subtraction — official .NET vectors
// ---------------------------------------------------------------------------

TEST(BigIntegerTests, SubtractBasic) {
    EXPECT_EQ((BigInteger(10) - BigInteger(3)).ToString(), "7");
}

TEST(BigIntegerTests, SubtractToNegative) {
    EXPECT_EQ((BigInteger(3) - BigInteger(10)).ToString(), "-7");
}

TEST(BigIntegerTests, SubtractUInt64MaxPlus1MinusOne) {
    // 18446744073709551616 - 1 = 18446744073709551615 (from .NET SubtractTests)
    BigInteger big = BigInteger::Parse("18446744073709551616");
    EXPECT_EQ((big - BigInteger::One).ToString(), "18446744073709551615");
}

TEST(BigIntegerTests, SubtractLargeMinusSmall) {
    // large - 123 = large...000 (trailing zeros due to 123 + 0 = 123 pattern)
    BigInteger large = BigInteger::Parse("123123123123123123123123123123123123123123123123123123123123123123123123123123123123123");
    EXPECT_EQ((large - BigInteger(123)).ToString(),
              "123123123123123123123123123123123123123123123123123123123123123123123123123123123123000");
}

// ---------------------------------------------------------------------------
// Multiplication — official .NET vectors
// ---------------------------------------------------------------------------

TEST(BigIntegerTests, MultiplyBasic) {
    EXPECT_EQ((BigInteger(6) * BigInteger(7)).ToString(), "42");
}

TEST(BigIntegerTests, MultiplyByZero) {
    BigInteger large = BigInteger::Parse("123123123123123123123123123");
    EXPECT_EQ((large * BigInteger::Zero).ToString(), "0");
}

TEST(BigIntegerTests, MultiplyByOne) {
    BigInteger a = BigInteger::Parse("999999999999999999");
    EXPECT_EQ((a * BigInteger::One).ToString(), a.ToString());
}

TEST(BigIntegerTests, MultiplyNegativeByPositive) {
    EXPECT_EQ((BigInteger(-6) * BigInteger(7)).ToString(), "-42");
}

TEST(BigIntegerTests, MultiplyTwoNegatives) {
    EXPECT_EQ((BigInteger(-6) * BigInteger(-7)).ToString(), "42");
}

TEST(BigIntegerTests, MultiplyUInt32MaxSquared) {
    // uint32Max = 4294967295; 4294967295^2 = 18446744065119617025 (from .NET BigIntegerPropertyTests)
    BigInteger u32max = BigInteger::Parse("4294967295");
    EXPECT_EQ((u32max * u32max).ToString(), "18446744065119617025");
}

TEST(BigIntegerTests, MultiplyUInt64MaxSquared) {
    // uint64Max^2 = 340282366920938463426481119284349108225 (from .NET BigIntegerPropertyTests)
    BigInteger u64max = BigInteger::Parse("18446744073709551615");
    EXPECT_EQ((u64max * u64max).ToString(), "340282366920938463426481119284349108225");
}

// ---------------------------------------------------------------------------
// Comparison
// ---------------------------------------------------------------------------

TEST(BigIntegerTests, EqualitySmall) {
    EXPECT_EQ(BigInteger(42), BigInteger(42));
    EXPECT_NE(BigInteger(42), BigInteger(43));
}

TEST(BigIntegerTests, EqualityLarge) {
    BigInteger a = BigInteger::Parse("999999999999999999999999999999");
    BigInteger b = BigInteger::Parse("999999999999999999999999999999");
    EXPECT_EQ(a, b);
}

TEST(BigIntegerTests, OrderingPositives) {
    EXPECT_LT(BigInteger(1), BigInteger(2));
    EXPECT_GT(BigInteger(2), BigInteger(1));
    EXPECT_LE(BigInteger(2), BigInteger(2));
    EXPECT_GE(BigInteger(2), BigInteger(2));
}

TEST(BigIntegerTests, OrderingNegatives) {
    EXPECT_LT(BigInteger(-5), BigInteger(-4));
    EXPECT_GT(BigInteger(-4), BigInteger(-5));
}

TEST(BigIntegerTests, NegativeLessThanPositive) {
    EXPECT_LT(BigInteger(-1), BigInteger(0));
    EXPECT_LT(BigInteger(-1), BigInteger(1));
}

TEST(BigIntegerTests, OrderingLargeNumbers) {
    BigInteger a = BigInteger::Parse("99999999999999999999999999999");
    BigInteger b = BigInteger::Parse("100000000000000000000000000000");
    EXPECT_LT(a, b);
    EXPECT_GT(b, a);
}

// ---------------------------------------------------------------------------
// Unary minus & Abs
// ---------------------------------------------------------------------------

TEST(BigIntegerTests, UnaryMinus) {
    EXPECT_EQ((-BigInteger(5)).ToString(), "-5");
    EXPECT_EQ((-BigInteger(-5)).ToString(), "5");
    EXPECT_EQ((-BigInteger::Zero).ToString(), "0");
}

TEST(BigIntegerTests, AbsPositive) {
    EXPECT_EQ(BigInteger(5).Abs().ToString(), "5");
}

TEST(BigIntegerTests, AbsNegative) {
    EXPECT_EQ(BigInteger(-5).Abs().ToString(), "5");
}

TEST(BigIntegerTests, AbsZero) {
    EXPECT_EQ(BigInteger::Zero.Abs().ToString(), "0");
}

// ---------------------------------------------------------------------------
// High-precision arithmetic (stress)
// ---------------------------------------------------------------------------

TEST(BigIntegerTests, AddSubtractRoundtrip) {
    // (a + b) - b == a  for large values
    BigInteger a = BigInteger::Parse("123456789012345678901234567890");
    BigInteger b = BigInteger::Parse("987654321098765432109876543210");
    EXPECT_EQ((a + b) - b, a);
}

TEST(BigIntegerTests, MultiplyLargeAndVerify) {
    // 10^30 = Parse("1" + 30 zeros)
    BigInteger ten = BigInteger(10);
    BigInteger p = BigInteger::One;
    for (int i = 0; i < 30; ++i) p = p * ten;
    std::string expected = "1" + std::string(30, '0');
    EXPECT_EQ(p.ToString(), expected);
}
