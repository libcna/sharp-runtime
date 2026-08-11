// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/Decimal.hpp"
#include "System/OverflowException.hpp"

using System::Decimal;

// ---------------------------------------------------------------------------
// getScaleProperty
// ---------------------------------------------------------------------------

TEST(DecimalTests, Scale_Integer_IsZero) {
    EXPECT_EQ(Decimal(42).getScaleProperty(), 0);
}

TEST(DecimalTests, Scale_OneTenth_IsOne) {
    EXPECT_EQ(Decimal::Parse("0.1").getScaleProperty(), 1);
}

TEST(DecimalTests, Scale_TwoDecimalPlaces) {
    EXPECT_EQ(Decimal::Parse("3.14").getScaleProperty(), 2);
}

TEST(DecimalTests, Scale_Zero_IsZero) {
    EXPECT_EQ(Decimal::Zero.getScaleProperty(), 0);
}

// ---------------------------------------------------------------------------
// getIsNegativeProperty
// ---------------------------------------------------------------------------

TEST(DecimalTests, IsNegative_Positive_False) {
    EXPECT_FALSE(Decimal(5).getIsNegativeProperty());
}

TEST(DecimalTests, IsNegative_Negative_True) {
    EXPECT_TRUE(Decimal(-5).getIsNegativeProperty());
}

TEST(DecimalTests, IsNegative_Zero_False) {
    EXPECT_FALSE(Decimal::Zero.getIsNegativeProperty());
}

TEST(DecimalTests, IsNegative_NegFrac_True) {
    EXPECT_TRUE(Decimal::Parse("-1.5").getIsNegativeProperty());
}

// ---------------------------------------------------------------------------
// CompareTo
// ---------------------------------------------------------------------------

TEST(DecimalTests, CompareTo_Equal_ReturnsZero) {
    EXPECT_EQ(Decimal(5).CompareTo(Decimal(5)), 0);
}

TEST(DecimalTests, CompareTo_Less_ReturnsNegative) {
    EXPECT_LT(Decimal(3).CompareTo(Decimal(5)), 0);
}

TEST(DecimalTests, CompareTo_Greater_ReturnsPositive) {
    EXPECT_GT(Decimal(7).CompareTo(Decimal(5)), 0);
}

TEST(DecimalTests, CompareTo_DifferentScales_EqualValue) {
    EXPECT_EQ(Decimal::Parse("1.0").CompareTo(Decimal::Parse("1.00")), 0);
}

TEST(DecimalTests, CompareTo_NegativeVsPositive) {
    EXPECT_LT(Decimal(-1).CompareTo(Decimal(1)), 0);
}

// ---------------------------------------------------------------------------
// Equals (instance)
// ---------------------------------------------------------------------------

TEST(DecimalTests, EqualsInstance_Same_True) {
    EXPECT_TRUE(Decimal(3).Equals(Decimal(3)));
}

TEST(DecimalTests, EqualsInstance_Different_False) {
    EXPECT_FALSE(Decimal(3).Equals(Decimal(4)));
}

TEST(DecimalTests, EqualsInstance_DifferentScale_True) {
    EXPECT_TRUE(Decimal::Parse("1.0").Equals(Decimal::Parse("1.00")));
}

// ---------------------------------------------------------------------------
// GetHashCode
// ---------------------------------------------------------------------------

TEST(DecimalTests, GetHashCode_EqualValues_SameHash) {
    EXPECT_EQ(Decimal(5).GetHashCode(), Decimal(5).GetHashCode());
}

TEST(DecimalTests, GetHashCode_DifferentScaleSameValue_SameHash) {
    // 1.0 and 1.00 are equal values — must have equal hashes
    EXPECT_EQ(Decimal::Parse("1.0").GetHashCode(),
              Decimal::Parse("1.00").GetHashCode());
}

// Replaces GetHashCode_DifferentValues_DifferentHash. Unequal decimals are permitted to hash
// equally (docs/HashAssertionContractRule.md R2) -- Decimal::GetHashCode folds a 128-bit mantissa,
// a scale and a sign into 31 bits, so it must collide somewhere. The contract direction is the
// two EXPECT_EQ tests above; the claim the removed assertion stood in for is this one.
TEST(DecimalTests, DifferentValues_AreUnequal) {
    EXPECT_NE(Decimal(1), Decimal(2));
    EXPECT_FALSE(Decimal(1).Equals(Decimal(2)));
}

// Not the general contract -- a signed hash code may legally be negative -- but
// Decimal::GetHashCode deliberately ends with `h & 0x7fffffff` (Decimal.hpp:246), so this pins
// that implementation choice, including for a negative value (docs/HashAssertionContractRule.md R5).
TEST(DecimalTests, GetHashCode_MasksTheSignBit) {
    EXPECT_GE(Decimal::Parse("3.14").GetHashCode(), 0);
    EXPECT_GE(Decimal::Parse("-3.14").GetHashCode(), 0);
}

// ---------------------------------------------------------------------------
// Static named helpers: Add, Subtract, Multiply, Divide, Remainder, Negate
// ---------------------------------------------------------------------------

TEST(DecimalTests, StaticAdd) {
    EXPECT_EQ(Decimal::Add(Decimal(3), Decimal(4)), Decimal(7));
}

TEST(DecimalTests, StaticSubtract) {
    EXPECT_EQ(Decimal::Subtract(Decimal(10), Decimal(3)), Decimal(7));
}

TEST(DecimalTests, StaticMultiply) {
    EXPECT_EQ(Decimal::Multiply(Decimal(6), Decimal(7)), Decimal(42));
}

TEST(DecimalTests, StaticDivide) {
    EXPECT_EQ(Decimal::Divide(Decimal(10), Decimal(2)), Decimal(5));
}

TEST(DecimalTests, StaticRemainder) {
    EXPECT_EQ(Decimal::Remainder(Decimal(10), Decimal(3)), Decimal(1));
}

TEST(DecimalTests, StaticNegate_Positive) {
    EXPECT_EQ(Decimal::Negate(Decimal(5)), Decimal(-5));
}

TEST(DecimalTests, StaticNegate_Negative) {
    EXPECT_EQ(Decimal::Negate(Decimal(-5)), Decimal(5));
}

TEST(DecimalTests, StaticNegate_Zero) {
    EXPECT_EQ(Decimal::Negate(Decimal::Zero), Decimal::Zero);
}

// ---------------------------------------------------------------------------
// Static Compare
// ---------------------------------------------------------------------------

TEST(DecimalTests, StaticCompare_Equal_Zero) {
    EXPECT_EQ(Decimal::Compare(Decimal(5), Decimal(5)), 0);
}

TEST(DecimalTests, StaticCompare_Less_Negative) {
    EXPECT_LT(Decimal::Compare(Decimal(1), Decimal(2)), 0);
}

TEST(DecimalTests, StaticCompare_Greater_Positive) {
    EXPECT_GT(Decimal::Compare(Decimal(3), Decimal(2)), 0);
}

// ---------------------------------------------------------------------------
// Max / Min
// ---------------------------------------------------------------------------

TEST(DecimalTests, Max_ReturnsLarger) {
    EXPECT_EQ(Decimal::Max(Decimal(3), Decimal(7)), Decimal(7));
}

TEST(DecimalTests, Max_Equal_ReturnsSame) {
    EXPECT_EQ(Decimal::Max(Decimal(5), Decimal(5)), Decimal(5));
}

TEST(DecimalTests, Max_Negative_ReturnsLessNegative) {
    EXPECT_EQ(Decimal::Max(Decimal(-3), Decimal(-1)), Decimal(-1));
}

TEST(DecimalTests, Min_ReturnsSmaller) {
    EXPECT_EQ(Decimal::Min(Decimal(3), Decimal(7)), Decimal(3));
}

TEST(DecimalTests, Min_Fractions) {
    EXPECT_EQ(Decimal::Min(Decimal::Parse("0.1"), Decimal::Parse("0.2")),
              Decimal::Parse("0.1"));
}

// ---------------------------------------------------------------------------
// Sign
// ---------------------------------------------------------------------------

TEST(DecimalTests, Sign_Positive_ReturnsOne) {
    EXPECT_EQ(Decimal::Sign(Decimal(5)), 1);
}

TEST(DecimalTests, Sign_Negative_ReturnsMinusOne) {
    EXPECT_EQ(Decimal::Sign(Decimal(-5)), -1);
}

TEST(DecimalTests, Sign_Zero_ReturnsZero) {
    EXPECT_EQ(Decimal::Sign(Decimal::Zero), 0);
}

TEST(DecimalTests, Sign_NegFrac_ReturnsMinusOne) {
    EXPECT_EQ(Decimal::Sign(Decimal::Parse("-0.01")), -1);
}

// ---------------------------------------------------------------------------
// Clamp
// ---------------------------------------------------------------------------

TEST(DecimalTests, Clamp_WithinRange_ReturnsSelf) {
    EXPECT_EQ(Decimal::Clamp(Decimal(5), Decimal(1), Decimal(10)), Decimal(5));
}

TEST(DecimalTests, Clamp_BelowMin_ReturnsMin) {
    EXPECT_EQ(Decimal::Clamp(Decimal(-5), Decimal(0), Decimal(10)), Decimal(0));
}

TEST(DecimalTests, Clamp_AboveMax_ReturnsMax) {
    EXPECT_EQ(Decimal::Clamp(Decimal(15), Decimal(0), Decimal(10)), Decimal(10));
}

TEST(DecimalTests, Clamp_Fractions) {
    EXPECT_EQ(Decimal::Clamp(Decimal::Parse("0.05"),
                             Decimal::Parse("0.1"),
                             Decimal::Parse("0.9")),
              Decimal::Parse("0.1"));
}
// SR-AUD-022 (#1846): an inverted interval must throw (was silently returning a bound).
TEST(DecimalTests, Clamp_MinGreaterThanMax_Throws) {
    EXPECT_THROW(Decimal::Clamp(Decimal(5), Decimal(10), Decimal(1)), System::ArgumentException);
    EXPECT_EQ(Decimal::Clamp(Decimal(5), Decimal(7), Decimal(7)), Decimal(7));
}

// ---------------------------------------------------------------------------
// ToUInt32 / ToUInt64
// ---------------------------------------------------------------------------

TEST(DecimalTests, ToUInt32_Valid) {
    EXPECT_EQ(Decimal(42).ToUInt32(), 42u);
}

TEST(DecimalTests, ToUInt32_Truncates) {
    EXPECT_EQ(Decimal::Parse("3.9").ToUInt32(), 3u);
}

TEST(DecimalTests, ToUInt32_Negative_Throws) {
    EXPECT_THROW(Decimal(-1).ToUInt32(), System::OverflowException);
}

TEST(DecimalTests, ToUInt64_Valid) {
    EXPECT_EQ(Decimal::Parse("100000000000").ToUInt64(), 100000000000ULL);
}

TEST(DecimalTests, ToUInt64_Negative_Throws) {
    EXPECT_THROW(Decimal(-1).ToUInt64(), System::OverflowException);
}
