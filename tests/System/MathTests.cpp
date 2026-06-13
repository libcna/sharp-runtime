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

// ---------------------------------------------------------------------------
// Tau constant
// ---------------------------------------------------------------------------

TEST(MathTests, TauConstant) {
    EXPECT_NEAR(Math::Tau, 2.0 * Math::PI, 1e-14);
}

// ---------------------------------------------------------------------------
// Logarithms
// ---------------------------------------------------------------------------

TEST(MathTests, LogNatural) {
    EXPECT_NEAR(Math::Log(Math::E), 1.0, kEps);
}
TEST(MathTests, LogNatural_One) {
    EXPECT_NEAR(Math::Log(1.0), 0.0, kEps);
}
TEST(MathTests, Log_WithBase) {
    EXPECT_NEAR(Math::Log(8.0, 2.0), 3.0, kEps);
}
TEST(MathTests, Log2) {
    EXPECT_NEAR(Math::Log2(8.0), 3.0, kEps);
}
TEST(MathTests, Log10) {
    EXPECT_NEAR(Math::Log10(1000.0), 3.0, kEps);
}
TEST(MathTests, Exp) {
    EXPECT_NEAR(Math::Exp(1.0), Math::E, kEps);
}
TEST(MathTests, Exp_Zero) {
    EXPECT_NEAR(Math::Exp(0.0), 1.0, kEps);
}

// ---------------------------------------------------------------------------
// Inverse trig
// ---------------------------------------------------------------------------

TEST(MathTests, Asin_One) {
    EXPECT_NEAR(Math::Asin(1.0), Math::PI / 2.0, kEps);
}
TEST(MathTests, Acos_One) {
    EXPECT_NEAR(Math::Acos(1.0), 0.0, kEps);
}
TEST(MathTests, Atan_One) {
    EXPECT_NEAR(Math::Atan(1.0), Math::PI / 4.0, kEps);
}
TEST(MathTests, Atan2_OneOne) {
    EXPECT_NEAR(Math::Atan2(1.0, 1.0), Math::PI / 4.0, kEps);
}

// ---------------------------------------------------------------------------
// Hyperbolic
// ---------------------------------------------------------------------------

TEST(MathTests, Sinh_Zero) {
    EXPECT_NEAR(Math::Sinh(0.0), 0.0, kEps);
}
TEST(MathTests, Cosh_Zero) {
    EXPECT_NEAR(Math::Cosh(0.0), 1.0, kEps);
}
TEST(MathTests, Tanh_Zero) {
    EXPECT_NEAR(Math::Tanh(0.0), 0.0, kEps);
}

// ---------------------------------------------------------------------------
// Sign
// ---------------------------------------------------------------------------

TEST(MathTests, Sign_Positive) { EXPECT_EQ(Math::Sign(5),    1); }
TEST(MathTests, Sign_Negative) { EXPECT_EQ(Math::Sign(-3),  -1); }
TEST(MathTests, Sign_Zero)     { EXPECT_EQ(Math::Sign(0),    0); }
TEST(MathTests, Sign_DoublePos) { EXPECT_EQ(Math::Sign(2.5),  1); }
TEST(MathTests, Sign_DoubleNeg) { EXPECT_EQ(Math::Sign(-1.5),-1); }

// ---------------------------------------------------------------------------
// Truncate
// ---------------------------------------------------------------------------

TEST(MathTests, Truncate_Positive) {
    EXPECT_NEAR(Math::Truncate(3.9), 3.0, kEps);
}
TEST(MathTests, Truncate_Negative) {
    EXPECT_NEAR(Math::Truncate(-3.9), -3.0, kEps);
}

// ---------------------------------------------------------------------------
// IEEERemainder
// ---------------------------------------------------------------------------

TEST(MathTests, IEEERemainder_Basic) {
    // 7 - (4 * round(7/4)) = 7 - 8 = -1
    EXPECT_NEAR(Math::IEEERemainder(7.0, 4.0), -1.0, kEps);
}

// ---------------------------------------------------------------------------
// DivRem
// ---------------------------------------------------------------------------

TEST(MathTests, DivRem_Basic) {
    int rem = 0;
    int q = Math::DivRem(17, 5, rem);
    EXPECT_EQ(q, 3);
    EXPECT_EQ(rem, 2);
}

// ---------------------------------------------------------------------------
// BigMul
// ---------------------------------------------------------------------------

TEST(MathTests, BigMul_Basic) {
    EXPECT_EQ(Math::BigMul(100000, 100000), 10000000000LL);
}

// ---------------------------------------------------------------------------
// ScaleB
// ---------------------------------------------------------------------------

TEST(MathTests, ScaleB_Basic) {
    EXPECT_NEAR(Math::ScaleB(1.0, 3), 8.0, kEps);
}
TEST(MathTests, ScaleB_Negative) {
    EXPECT_NEAR(Math::ScaleB(8.0, -3), 1.0, kEps);
}

// ---------------------------------------------------------------------------
// Cbrt
// ---------------------------------------------------------------------------
TEST(MathTests, Cbrt_PositivePerfect) { EXPECT_NEAR(Math::Cbrt(27.0),  3.0, 1e-10); }
TEST(MathTests, Cbrt_NegativeValue)   { EXPECT_NEAR(Math::Cbrt(-8.0), -2.0, 1e-10); }
TEST(MathTests, Cbrt_Zero)            { EXPECT_EQ(Math::Cbrt(0.0), 0.0); }

// ---------------------------------------------------------------------------
// Acosh / Asinh / Atanh
// ---------------------------------------------------------------------------
TEST(MathTests, Acosh_One)     { EXPECT_NEAR(Math::Acosh(1.0),   0.0,         1e-10); }
TEST(MathTests, Acosh_Larger)  { EXPECT_NEAR(Math::Acosh(std::cosh(2.0)), 2.0, 1e-10); }
TEST(MathTests, Asinh_Zero)    { EXPECT_NEAR(Math::Asinh(0.0),   0.0,         1e-10); }
TEST(MathTests, Asinh_RoundTrip) { EXPECT_NEAR(Math::Asinh(std::sinh(1.5)), 1.5, 1e-10); }
TEST(MathTests, Atanh_Zero)    { EXPECT_NEAR(Math::Atanh(0.0),   0.0,         1e-10); }
TEST(MathTests, Atanh_RoundTrip) { EXPECT_NEAR(Math::Atanh(std::tanh(0.7)), 0.7, 1e-10); }

// ---------------------------------------------------------------------------
// Round(value, digits)
// ---------------------------------------------------------------------------
TEST(MathTests, Round_TwoDigits)   { EXPECT_NEAR(Math::Round(3.14159, 2), 3.14, 1e-10); }
TEST(MathTests, Round_ZeroDigits)  { EXPECT_NEAR(Math::Round(2.7, 0),     3.0,  1e-10); }
TEST(MathTests, Round_ThreeDigits) { EXPECT_NEAR(Math::Round(1.2345, 3),  1.235, 1e-6); }

// ---------------------------------------------------------------------------
// CopySign
// ---------------------------------------------------------------------------
TEST(MathTests, CopySign_PositiveMag_NegativeSign) { EXPECT_EQ(Math::CopySign(3.0, -1.0), -3.0); }
TEST(MathTests, CopySign_NegativeMag_PositiveSign) { EXPECT_EQ(Math::CopySign(-5.0, 1.0),  5.0); }

// ---------------------------------------------------------------------------
// BitIncrement / BitDecrement
// ---------------------------------------------------------------------------
TEST(MathTests, BitIncrement_GreaterThanOriginal) {
    double x = 1.0;
    EXPECT_GT(Math::BitIncrement(x), x);
}
TEST(MathTests, BitDecrement_LessThanOriginal) {
    double x = 1.0;
    EXPECT_LT(Math::BitDecrement(x), x);
}

// ---------------------------------------------------------------------------
// FusedMultiplyAdd
// ---------------------------------------------------------------------------
TEST(MathTests, FusedMultiplyAdd_Basic) {
    EXPECT_NEAR(Math::FusedMultiplyAdd(2.0, 3.0, 4.0), 10.0, 1e-10);
}
TEST(MathTests, FusedMultiplyAdd_Zero) {
    EXPECT_NEAR(Math::FusedMultiplyAdd(0.0, 99.0, 7.0), 7.0, 1e-10);
}
