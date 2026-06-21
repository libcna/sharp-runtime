// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <cmath>
#include <limits>
#include "System/Double.hpp"

using System::Double;

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

TEST(DoubleTests, MaxValue_IsPositive) {
    EXPECT_GT(Double::MaxValue, 0.0);
}

TEST(DoubleTests, MinValue_IsNegative) {
    EXPECT_LT(Double::MinValue, 0.0);
}

TEST(DoubleTests, Epsilon_IsPositive) {
    EXPECT_GT(Double::Epsilon, 0.0);
}

TEST(DoubleTests, NaN_IsNaN) {
    EXPECT_TRUE(std::isnan(Double::NaN));
}

TEST(DoubleTests, PositiveInfinity_IsInfinite) {
    EXPECT_TRUE(std::isinf(Double::PositiveInfinity));
    EXPECT_GT(Double::PositiveInfinity, 0.0);
}

TEST(DoubleTests, NegativeInfinity_IsInfinite) {
    EXPECT_TRUE(std::isinf(Double::NegativeInfinity));
    EXPECT_LT(Double::NegativeInfinity, 0.0);
}

TEST(DoubleTests, NegativeZero_IsZero) {
    EXPECT_EQ(Double::NegativeZero, 0.0);
}

TEST(DoubleTests, E_Approx) {
    EXPECT_NEAR(Double::E, 2.718281828459045, 1e-14);
}

TEST(DoubleTests, Pi_Approx) {
    EXPECT_NEAR(Double::Pi, 3.141592653589793, 1e-14);
}

TEST(DoubleTests, Tau_IsTwoPi) {
    EXPECT_NEAR(Double::Tau, 2.0 * Double::Pi, 1e-14);
}

// ---------------------------------------------------------------------------
// Classification predicates — existing
// ---------------------------------------------------------------------------

TEST(DoubleTests, IsNaN_True) {
    EXPECT_TRUE(Double::IsNaN(std::numeric_limits<double>::quiet_NaN()));
}

TEST(DoubleTests, IsNaN_False_ForFinite) {
    EXPECT_FALSE(Double::IsNaN(1.0));
}

TEST(DoubleTests, IsInfinity_True_Positive) {
    EXPECT_TRUE(Double::IsInfinity(Double::PositiveInfinity));
}

TEST(DoubleTests, IsInfinity_True_Negative) {
    EXPECT_TRUE(Double::IsInfinity(Double::NegativeInfinity));
}

TEST(DoubleTests, IsInfinity_False_Finite) {
    EXPECT_FALSE(Double::IsInfinity(42.0));
}

TEST(DoubleTests, IsPositiveInfinity_True) {
    EXPECT_TRUE(Double::IsPositiveInfinity(Double::PositiveInfinity));
}

TEST(DoubleTests, IsPositiveInfinity_False_NegInf) {
    EXPECT_FALSE(Double::IsPositiveInfinity(Double::NegativeInfinity));
}

TEST(DoubleTests, IsNegativeInfinity_True) {
    EXPECT_TRUE(Double::IsNegativeInfinity(Double::NegativeInfinity));
}

TEST(DoubleTests, IsFinite_True) {
    EXPECT_TRUE(Double::IsFinite(3.14));
}

TEST(DoubleTests, IsFinite_False_Inf) {
    EXPECT_FALSE(Double::IsFinite(Double::PositiveInfinity));
}

TEST(DoubleTests, IsNormal_True) {
    EXPECT_TRUE(Double::IsNormal(1.0));
}

TEST(DoubleTests, IsSubnormal_True) {
    EXPECT_TRUE(Double::IsSubnormal(5e-324));
}

// ---------------------------------------------------------------------------
// Classification predicates — new
// ---------------------------------------------------------------------------

TEST(DoubleTests, IsNegative_True_NegVal) {
    EXPECT_TRUE(Double::IsNegative(-1.0));
}

TEST(DoubleTests, IsNegative_True_NegZero) {
    EXPECT_TRUE(Double::IsNegative(-0.0));
}

TEST(DoubleTests, IsNegative_False_PosVal) {
    EXPECT_FALSE(Double::IsNegative(1.0));
}

TEST(DoubleTests, IsNegative_False_PosZero) {
    EXPECT_FALSE(Double::IsNegative(0.0));
}

TEST(DoubleTests, IsPositive_True_PosVal) {
    EXPECT_TRUE(Double::IsPositive(1.0));
}

TEST(DoubleTests, IsPositive_True_PosZero) {
    EXPECT_TRUE(Double::IsPositive(0.0));
}

TEST(DoubleTests, IsPositive_False_NegVal) {
    EXPECT_FALSE(Double::IsPositive(-1.0));
}

TEST(DoubleTests, IsInteger_True_Whole) {
    EXPECT_TRUE(Double::IsInteger(5.0));
}

TEST(DoubleTests, IsInteger_False_Frac) {
    EXPECT_FALSE(Double::IsInteger(5.5));
}

TEST(DoubleTests, IsInteger_False_Inf) {
    EXPECT_FALSE(Double::IsInteger(Double::PositiveInfinity));
}

TEST(DoubleTests, IsInteger_False_NaN) {
    EXPECT_FALSE(Double::IsInteger(Double::NaN));
}

TEST(DoubleTests, IsEvenInteger_True) {
    EXPECT_TRUE(Double::IsEvenInteger(4.0));
}

TEST(DoubleTests, IsEvenInteger_False_Odd) {
    EXPECT_FALSE(Double::IsEvenInteger(3.0));
}

TEST(DoubleTests, IsEvenInteger_False_Frac) {
    EXPECT_FALSE(Double::IsEvenInteger(4.5));
}

TEST(DoubleTests, IsOddInteger_True) {
    EXPECT_TRUE(Double::IsOddInteger(3.0));
}

TEST(DoubleTests, IsOddInteger_False_Even) {
    EXPECT_FALSE(Double::IsOddInteger(4.0));
}

TEST(DoubleTests, IsRealNumber_True_Finite) {
    EXPECT_TRUE(Double::IsRealNumber(1.0));
}

TEST(DoubleTests, IsRealNumber_True_Inf) {
    EXPECT_TRUE(Double::IsRealNumber(Double::PositiveInfinity));
}

TEST(DoubleTests, IsRealNumber_False_NaN) {
    EXPECT_FALSE(Double::IsRealNumber(Double::NaN));
}

TEST(DoubleTests, IsPow2_True) {
    EXPECT_TRUE(Double::IsPow2(1.0));
    EXPECT_TRUE(Double::IsPow2(2.0));
    EXPECT_TRUE(Double::IsPow2(0.5));
    EXPECT_TRUE(Double::IsPow2(256.0));
}

TEST(DoubleTests, IsPow2_False_NotPow2) {
    EXPECT_FALSE(Double::IsPow2(3.0));
    EXPECT_FALSE(Double::IsPow2(0.0));
    EXPECT_FALSE(Double::IsPow2(-2.0));
}

TEST(DoubleTests, IsPow2_False_NaN) {
    EXPECT_FALSE(Double::IsPow2(Double::NaN));
}

TEST(DoubleTests, IsPow2_False_Inf) {
    EXPECT_FALSE(Double::IsPow2(Double::PositiveInfinity));
}

// ---------------------------------------------------------------------------
// Math helpers
// ---------------------------------------------------------------------------

TEST(DoubleTests, Abs_Positive) {
    EXPECT_EQ(Double::Abs(3.0), 3.0);
}

TEST(DoubleTests, Abs_Negative) {
    EXPECT_EQ(Double::Abs(-3.0), 3.0);
}

TEST(DoubleTests, Clamp_WithinRange) {
    EXPECT_EQ(Double::Clamp(5.0, 1.0, 10.0), 5.0);
}

TEST(DoubleTests, Clamp_BelowMin) {
    EXPECT_EQ(Double::Clamp(-5.0, 1.0, 10.0), 1.0);
}

TEST(DoubleTests, Clamp_AboveMax) {
    EXPECT_EQ(Double::Clamp(15.0, 1.0, 10.0), 10.0);
}

TEST(DoubleTests, CopySign_PosToNeg) {
    EXPECT_EQ(Double::CopySign(3.0, -1.0), -3.0);
}

TEST(DoubleTests, CopySign_NegToPos) {
    EXPECT_EQ(Double::CopySign(-3.0, 1.0), 3.0);
}

TEST(DoubleTests, Max_ReturnsLarger) {
    EXPECT_EQ(Double::Max(3.0, 7.0), 7.0);
}

TEST(DoubleTests, Min_ReturnsSmaller) {
    EXPECT_EQ(Double::Min(3.0, 7.0), 3.0);
}

TEST(DoubleTests, Sign_Positive) {
    EXPECT_EQ(Double::Sign(5.0), 1);
}

TEST(DoubleTests, Sign_Negative) {
    EXPECT_EQ(Double::Sign(-5.0), -1);
}

TEST(DoubleTests, Sign_Zero) {
    EXPECT_EQ(Double::Sign(0.0), 0);
}

TEST(DoubleTests, Sign_NaN_Throws) {
    EXPECT_THROW(Double::Sign(Double::NaN), std::invalid_argument);
}

// ---------------------------------------------------------------------------
// Parse / TryParse
// ---------------------------------------------------------------------------

TEST(DoubleTests, Parse_ValidNumber) {
    EXPECT_DOUBLE_EQ(Double::Parse("3.14"), 3.14);
}

TEST(DoubleTests, Parse_NaN) {
    EXPECT_TRUE(std::isnan(Double::Parse("NaN")));
}

TEST(DoubleTests, Parse_Infinity) {
    EXPECT_TRUE(std::isinf(Double::Parse("Infinity")));
}

TEST(DoubleTests, Parse_NegativeInfinity) {
    EXPECT_EQ(Double::Parse("-Infinity"), Double::NegativeInfinity);
}

TEST(DoubleTests, Parse_Invalid_Throws) {
    EXPECT_THROW(Double::Parse("abc"), std::invalid_argument);
}

TEST(DoubleTests, TryParse_Valid_ReturnsTrue) {
    double r;
    EXPECT_TRUE(Double::TryParse("2.5", r));
    EXPECT_DOUBLE_EQ(r, 2.5);
}

TEST(DoubleTests, TryParse_Invalid_ReturnsFalse) {
    double r;
    EXPECT_FALSE(Double::TryParse("bad", r));
    EXPECT_EQ(r, 0.0);
}

// ---------------------------------------------------------------------------
// ToString
// ---------------------------------------------------------------------------

TEST(DoubleTests, ToString_NaN) {
    EXPECT_EQ(Double::ToString(Double::NaN), "NaN");
}

TEST(DoubleTests, ToString_Infinity) {
    EXPECT_EQ(Double::ToString(Double::PositiveInfinity), "Infinity");
}

TEST(DoubleTests, ToString_NegInfinity) {
    EXPECT_EQ(Double::ToString(Double::NegativeInfinity), "-Infinity");
}

TEST(DoubleTests, ToString_Value) {
    EXPECT_EQ(Double::ToString(0.0), "0");
}

TEST(DoubleTests, ToString_FormatF2) {
    EXPECT_EQ(Double::ToString(3.14159, "F2"), "3.14");
}

TEST(DoubleTests, ToString_FormatE) {
    std::string s = Double::ToString(1000.0, "E2");
    EXPECT_FALSE(s.empty());
}

TEST(DoubleTests, ToString_FormatR) {
    double v = 1.5;
    EXPECT_EQ(Double::ToString(v, "R"), Double::ToString(v));
}
