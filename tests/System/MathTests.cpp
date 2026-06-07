// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <cmath>
#include <numbers>

#include "System/Math.hpp"

using System::Math;

static constexpr double kEps = 1e-10;

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

TEST(MathTests, EConstant) {
    // From .NET MathTests.cs: exact bit pattern 0x4005BF0A8B145769
    EXPECT_NEAR(Math::E, 2.718281828459045, 1e-14);
}

TEST(MathTests, PIConstant) {
    // From .NET MathTests.cs: exact bit pattern 0x400921FB54442D18
    EXPECT_NEAR(Math::PI, 3.141592653589793, 1e-14);
}

// ---------------------------------------------------------------------------
// Abs
// ---------------------------------------------------------------------------

TEST(MathTests, AbsDoublePositive) {
    EXPECT_NEAR(Math::Abs(3.0), 3.0, kEps);
}

TEST(MathTests, AbsDoubleNegative) {
    EXPECT_NEAR(Math::Abs(-3.0), 3.0, kEps);
}

TEST(MathTests, AbsDoubleZero) {
    EXPECT_NEAR(Math::Abs(0.0), 0.0, kEps);
}

TEST(MathTests, AbsIntPositive) {
    EXPECT_EQ(Math::Abs(3), 3);
}

TEST(MathTests, AbsIntNegative) {
    // From .NET MathTests.cs: Assert.Equal(3, Math.Abs(-3))
    EXPECT_EQ(Math::Abs(-3), 3);
}

TEST(MathTests, AbsIntZero) {
    EXPECT_EQ(Math::Abs(0), 0);
}

// ---------------------------------------------------------------------------
// Min / Max (int and double)
// ---------------------------------------------------------------------------

TEST(MathTests, MinIntBasic) {
    // From .NET: Assert.Equal(-2, Math.Max(-2, 3)) — actually Min
    EXPECT_EQ(Math::Min(-2, 3), -2);
    EXPECT_EQ(Math::Min(3, -2), -2);
}

TEST(MathTests, MaxIntBasic) {
    // From .NET MathTests.cs: Assert.Equal(3, Math.Max(-2, 3))
    EXPECT_EQ(Math::Max(-2, 3), 3);
    EXPECT_EQ(Math::Max(3, -2), 3);
}

TEST(MathTests, MinMaxIntEqual) {
    EXPECT_EQ(Math::Min(5, 5), 5);
    EXPECT_EQ(Math::Max(5, 5), 5);
}

TEST(MathTests, MinDoubleBasic) {
    EXPECT_NEAR(Math::Min(-1.5, 2.5), -1.5, kEps);
}

TEST(MathTests, MaxDoubleBasic) {
    EXPECT_NEAR(Math::Max(-1.5, 2.5), 2.5, kEps);
}

// ---------------------------------------------------------------------------
// Clamp
// ---------------------------------------------------------------------------

TEST(MathTests, ClampIntBelowMin) {
    EXPECT_EQ(Math::Clamp(0, 1, 10), 1);
}

TEST(MathTests, ClampIntAboveMax) {
    EXPECT_EQ(Math::Clamp(20, 1, 10), 10);
}

TEST(MathTests, ClampIntInRange) {
    EXPECT_EQ(Math::Clamp(5, 1, 10), 5);
}

TEST(MathTests, ClampDoubleBasic) {
    EXPECT_NEAR(Math::Clamp(0.5, 1.0, 3.0), 1.0, kEps);
    EXPECT_NEAR(Math::Clamp(2.0, 1.0, 3.0), 2.0, kEps);
    EXPECT_NEAR(Math::Clamp(4.0, 1.0, 3.0), 3.0, kEps);
}

// ---------------------------------------------------------------------------
// Floor / Ceiling / Round
// ---------------------------------------------------------------------------

TEST(MathTests, FloorPositive) {
    // From .NET: Assert.Equal(1.0, Math.Floor(1.1)) and Math.Floor(1.9)
    EXPECT_NEAR(Math::Floor(1.1), 1.0, kEps);
    EXPECT_NEAR(Math::Floor(1.9), 1.0, kEps);
}

TEST(MathTests, FloorNegative) {
    // From .NET: Assert.Equal(-2.0, Math.Floor(-1.1))
    EXPECT_NEAR(Math::Floor(-1.1), -2.0, kEps);
}

TEST(MathTests, CeilingPositive) {
    // From .NET: Assert.Equal(2.0, Math.Ceiling(1.1)) and Math.Ceiling(1.9)
    EXPECT_NEAR(Math::Ceiling(1.1), 2.0, kEps);
    EXPECT_NEAR(Math::Ceiling(1.9), 2.0, kEps);
}

TEST(MathTests, CeilingNegative) {
    // From .NET: Assert.Equal(-1.0, Math.Ceiling(-1.1))
    EXPECT_NEAR(Math::Ceiling(-1.1), -1.0, kEps);
}

TEST(MathTests, RoundHalfUp) {
    EXPECT_NEAR(Math::Round(2.5), 3.0, kEps);
    EXPECT_NEAR(Math::Round(2.4), 2.0, kEps);
}

TEST(MathTests, RoundNegative) {
    EXPECT_NEAR(Math::Round(-2.5), -3.0, kEps);
}

// ---------------------------------------------------------------------------
// Sqrt
// ---------------------------------------------------------------------------

TEST(MathTests, SqrtFour) {
    EXPECT_NEAR(Math::Sqrt(4.0), 2.0, kEps);
}

TEST(MathTests, SqrtTwo) {
    EXPECT_NEAR(Math::Sqrt(2.0), std::sqrt(2.0), kEps);
}

TEST(MathTests, SqrtZero) {
    EXPECT_NEAR(Math::Sqrt(0.0), 0.0, kEps);
}

// ---------------------------------------------------------------------------
// Pow
// ---------------------------------------------------------------------------

TEST(MathTests, PowTwoThree) {
    EXPECT_NEAR(Math::Pow(2.0, 3.0), 8.0, kEps);
}

TEST(MathTests, PowZeroExponent) {
    EXPECT_NEAR(Math::Pow(99.0, 0.0), 1.0, kEps);
}

TEST(MathTests, PowOneExponent) {
    EXPECT_NEAR(Math::Pow(5.0, 1.0), 5.0, kEps);
}

TEST(MathTests, PowFractional) {
    // 8^(1/3) = 2
    EXPECT_NEAR(Math::Pow(8.0, 1.0 / 3.0), 2.0, 1e-9);
}

// ---------------------------------------------------------------------------
// Trig — canonical values and identities
// ---------------------------------------------------------------------------

TEST(MathTests, SinZero) {
    EXPECT_NEAR(Math::Sin(0.0), 0.0, kEps);
}

TEST(MathTests, SinHalfPi) {
    EXPECT_NEAR(Math::Sin(Math::PI / 2.0), 1.0, kEps);
}

TEST(MathTests, CosZero) {
    EXPECT_NEAR(Math::Cos(0.0), 1.0, kEps);
}

TEST(MathTests, CosPi) {
    EXPECT_NEAR(Math::Cos(Math::PI), -1.0, kEps);
}

TEST(MathTests, TanZero) {
    EXPECT_NEAR(Math::Tan(0.0), 0.0, kEps);
}

TEST(MathTests, TanQuarterPi) {
    // tan(π/4) = 1
    EXPECT_NEAR(Math::Tan(Math::PI / 4.0), 1.0, kEps);
}

TEST(MathTests, SinSquaredPlusCosSquaredIsOne) {
    // Pythagorean identity for several angles
    for (double angle : {0.0, 0.5, 1.0, 1.5, Math::PI / 3.0}) {
        double s = Math::Sin(angle), c = Math::Cos(angle);
        EXPECT_NEAR(s * s + c * c, 1.0, kEps) << "angle=" << angle;
    }
}
