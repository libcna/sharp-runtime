// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <cmath>
#include <numbers>
#include <limits>
#include <string>

#include "System/Numerics/Complex.hpp"

using System::Numerics::Complex;

static constexpr double kEps = 1e-10;

// Helpers matching the style of the official .NET ComplexTests.cs
static void VerifyRI(const Complex& c, double re, double im) {
    EXPECT_NEAR(c.getRealProperty(),      re, kEps);
    EXPECT_NEAR(c.getImaginaryProperty(), im, kEps);
}

// ---------------------------------------------------------------------------
// Static constants — from .NET ComplexTests.cs
// ---------------------------------------------------------------------------

TEST(ComplexTests, ZeroConstant) {
    VerifyRI(Complex::Zero, 0.0, 0.0);
    EXPECT_NEAR(Complex::Zero.getMagnitudeProperty(), 0.0, kEps);
}

TEST(ComplexTests, OneConstant) {
    VerifyRI(Complex::One, 1.0, 0.0);
    EXPECT_NEAR(Complex::One.getMagnitudeProperty(), 1.0, kEps);
    EXPECT_NEAR(Complex::One.getPhaseProperty(),     0.0, kEps);
}

TEST(ComplexTests, ImaginaryOneConstant) {
    VerifyRI(Complex::ImaginaryOne, 0.0, 1.0);
    EXPECT_NEAR(Complex::ImaginaryOne.getMagnitudeProperty(), 1.0, kEps);
    EXPECT_NEAR(Complex::ImaginaryOne.getPhaseProperty(), std::numbers::pi / 2.0, kEps);
}

// ---------------------------------------------------------------------------
// Construction and property access
// ---------------------------------------------------------------------------

TEST(ComplexTests, ConstructRealImag) {
    Complex c(3.0, 4.0);
    EXPECT_NEAR(c.getRealProperty(),      3.0, kEps);
    EXPECT_NEAR(c.getImaginaryProperty(), 4.0, kEps);
}

TEST(ComplexTests, MagnitudeThreeFourFive) {
    // Pythagorean triple: |3 + 4i| = 5
    Complex c(3.0, 4.0);
    EXPECT_NEAR(c.getMagnitudeProperty(), 5.0, kEps);
}

TEST(ComplexTests, PureImaginaryPhaseIsHalfPi) {
    Complex c(0.0, 1.0);
    EXPECT_NEAR(c.getPhaseProperty(), std::numbers::pi / 2.0, kEps);
}

TEST(ComplexTests, PureRealNegativePhaseIsPi) {
    Complex c(-1.0, 0.0);
    EXPECT_NEAR(c.getPhaseProperty(), std::numbers::pi, kEps);
}

TEST(ComplexTests, PhaseFirstQuadrant) {
    // (1,1): phase = π/4
    Complex c(1.0, 1.0);
    EXPECT_NEAR(c.getPhaseProperty(), std::numbers::pi / 4.0, kEps);
}

// ---------------------------------------------------------------------------
// Arithmetic — official vectors derived from .NET ComplexTests.cs
// ---------------------------------------------------------------------------

TEST(ComplexTests, AddBasic) {
    // (1,2) + (3,4) = (4,6)
    Complex a(1.0, 2.0), b(3.0, 4.0);
    VerifyRI(a + b, 4.0, 6.0);
}

TEST(ComplexTests, AddWithNegative) {
    Complex a(5.0, -3.0), b(-2.0, 7.0);
    VerifyRI(a + b, 3.0, 4.0);
}

TEST(ComplexTests, AddWithZero) {
    Complex a(3.0, 4.0);
    VerifyRI(a + Complex::Zero, 3.0, 4.0);
}

TEST(ComplexTests, SubtractBasic) {
    // (5,7) - (2,3) = (3,4)
    Complex a(5.0, 7.0), b(2.0, 3.0);
    VerifyRI(a - b, 3.0, 4.0);
}

TEST(ComplexTests, SubtractToNegative) {
    Complex a(1.0, 1.0), b(3.0, 5.0);
    VerifyRI(a - b, -2.0, -4.0);
}

TEST(ComplexTests, MultiplyBasic) {
    // (1+2i)*(3+4i) = (1*3-2*4) + (1*4+2*3)i = -5 + 10i
    Complex a(1.0, 2.0), b(3.0, 4.0);
    VerifyRI(a * b, -5.0, 10.0);
}

TEST(ComplexTests, MultiplyByImaginaryOne) {
    // i * i = -1
    Complex i = Complex::ImaginaryOne;
    VerifyRI(i * i, -1.0, 0.0);
}

TEST(ComplexTests, MultiplyByZero) {
    Complex a(3.0, 4.0);
    VerifyRI(a * Complex::Zero, 0.0, 0.0);
}

TEST(ComplexTests, MultiplyByOne) {
    Complex a(3.0, 4.0);
    VerifyRI(a * Complex::One, 3.0, 4.0);
}

TEST(ComplexTests, DivideBasic) {
    // (3+4i)/(1+2i) = (3+4i)(1-2i)/5 = (3+8 + (4-6)i)/5 = (11-2i)/5
    Complex a(3.0, 4.0), b(1.0, 2.0);
    VerifyRI(a / b, 11.0 / 5.0, -2.0 / 5.0);
}

TEST(ComplexTests, DivideByOne) {
    Complex a(3.0, 4.0);
    VerifyRI(a / Complex::One, 3.0, 4.0);
}

TEST(ComplexTests, UnaryMinus) {
    Complex a(3.0, -4.0);
    VerifyRI(-a, -3.0, 4.0);
}

// ---------------------------------------------------------------------------
// Comparison
// ---------------------------------------------------------------------------

TEST(ComplexTests, EqualityEqual) {
    EXPECT_EQ(Complex(1.0, 2.0), Complex(1.0, 2.0));
}

TEST(ComplexTests, EqualityNotEqual) {
    EXPECT_NE(Complex(1.0, 2.0), Complex(1.0, 3.0));
}

TEST(ComplexTests, EqualityZero) {
    EXPECT_EQ(Complex(0.0, 0.0), Complex::Zero);
}

// ---------------------------------------------------------------------------
// Conjugate
// ---------------------------------------------------------------------------

TEST(ComplexTests, ConjugateBasic) {
    // conj(3+4i) = 3-4i
    Complex a(3.0, 4.0);
    VerifyRI(a.Conjugate(), 3.0, -4.0);
}

TEST(ComplexTests, ConjugateOfRealIsReal) {
    Complex a(5.0, 0.0);
    VerifyRI(a.Conjugate(), 5.0, 0.0);
}

TEST(ComplexTests, ConjugateMagnitudePreserved) {
    // |z| == |conj(z)|
    Complex a(3.0, 4.0);
    EXPECT_NEAR(a.getMagnitudeProperty(), a.Conjugate().getMagnitudeProperty(), kEps);
}

TEST(ComplexTests, ConjugateTimesOriginalIsRealSquared) {
    // z * conj(z) = |z|^2 (real part only)
    Complex a(3.0, 4.0);
    Complex prod = a * a.Conjugate();
    EXPECT_NEAR(prod.getRealProperty(),      25.0, kEps);  // 3^2+4^2
    EXPECT_NEAR(prod.getImaginaryProperty(),  0.0, kEps);
}

// ---------------------------------------------------------------------------
// Abs
// ---------------------------------------------------------------------------

TEST(ComplexTests, AbsDThreeFourFive) {
    EXPECT_NEAR(Complex::AbsD(Complex(3.0, 4.0)), 5.0, kEps);
}

TEST(ComplexTests, AbsDPureReal) {
    EXPECT_NEAR(Complex::AbsD(Complex(-7.0, 0.0)), 7.0, kEps);
}

TEST(ComplexTests, AbsDZero) {
    EXPECT_NEAR(Complex::AbsD(Complex::Zero), 0.0, kEps);
}

// ---------------------------------------------------------------------------
// Sqrt — key .NET vector: sqrt(-1) == ImaginaryOne
// ---------------------------------------------------------------------------

TEST(ComplexTests, SqrtMinusOne) {
    // From .NET ComplexTests.cs line 70: Complex.Sqrt(-1.0) == Complex.ImaginaryOne
    Complex result = Complex::Sqrt(Complex(-1.0, 0.0));
    VerifyRI(result, 0.0, 1.0);
}

TEST(ComplexTests, SqrtPositiveReal) {
    // sqrt(4+0i) = 2+0i
    Complex result = Complex::Sqrt(Complex(4.0, 0.0));
    VerifyRI(result, 2.0, 0.0);
}

// ---------------------------------------------------------------------------
// Exp / Log
// ---------------------------------------------------------------------------

TEST(ComplexTests, ExpIpiIsMinusOne) {
    // Euler's identity: e^(iπ) = -1
    Complex result = Complex::Exp(Complex(0.0, std::numbers::pi));
    EXPECT_NEAR(result.getRealProperty(),      -1.0, 1e-9);
    EXPECT_NEAR(result.getImaginaryProperty(),  0.0, 1e-9);
}

TEST(ComplexTests, LogExpRoundtrip) {
    // log(exp(z)) == z  for z with |imag| < π
    Complex z(2.0, 1.0);
    Complex roundtrip = Complex::Log(Complex::Exp(z));
    VerifyRI(roundtrip, 2.0, 1.0);
}

// ---------------------------------------------------------------------------
// Sin / Cos
// ---------------------------------------------------------------------------

TEST(ComplexTests, SinZeroIsZero) {
    VerifyRI(Complex::Sin(Complex::Zero), 0.0, 0.0);
}

TEST(ComplexTests, CosZeroIsOne) {
    VerifyRI(Complex::Cos(Complex::Zero), 1.0, 0.0);
}

TEST(ComplexTests, SinSquaredPlusCosSquaredIsOne) {
    // sin²(z) + cos²(z) = 1  for z = (1, 0.5)
    Complex z(1.0, 0.5);
    Complex s = Complex::Sin(z), c = Complex::Cos(z);
    Complex sum = s * s + c * c;
    EXPECT_NEAR(sum.getRealProperty(),      1.0, kEps);
    EXPECT_NEAR(sum.getImaginaryProperty(), 0.0, kEps);
}

// ---------------------------------------------------------------------------
// ToString
// ---------------------------------------------------------------------------

// Ticket #2171 (SR-AUD-277, text subpart). The test that used to live here asserted
// s.find("1.") != npos -- it PINNED the defect, because only std::to_string's fixed six
// fractional digits put a '.' after an integral component. It is replaced, not deleted, the way
// #1828 replaced #1841's placeholder.

TEST(ComplexTests, ToStringRendersEachComponentLikeSystemDouble) {
    // The repair: each component goes through System::Double::ToString, this port's own renderer
    // for a .NET double. An integral value renders WITHOUT a spurious ".000000".
    EXPECT_EQ(Complex(1.0, 2.0).ToString(), "<1; 2>");
    EXPECT_EQ(Complex(0.0, 0.0).ToString(), "<0; 0>");
    EXPECT_EQ(Complex(0.1, 0.2).ToString(), "<0.1; 0.2>");
    EXPECT_EQ(Complex(26.1, 18.06).ToString(), "<26.1; 18.06>");
}

TEST(ComplexTests, ToStringNoLongerDestroysSmallOrLargeValues) {
    // Before #2171: 1e-9 rendered as "0.000000" -- the value was gone -- and 1e300 rendered as a
    // 309-digit decimal expansion, making the whole string 619 characters.
    EXPECT_EQ(Complex(1e-9, 0.0).ToString(), "<1e-09; 0>");
    const std::string big = Complex(1e300, -1e300).ToString();
    EXPECT_EQ(big, "<1e+300; -1e+300>");
    EXPECT_LT(big.size(), 40u);
    // Round-trip fidelity: the shortest form that reads back exactly.
    EXPECT_EQ(Complex(1.0 / 3.0, 0.0).ToString(), "<0.3333333333333333; 0>");
}

TEST(ComplexTests, ToStringUsesTheDotNetSpecialValueSpellings) {
    // Before #2171 these were the C library's "inf" / "-inf" / "nan".
    const double inf = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    EXPECT_EQ(Complex(inf, -inf).ToString(), "<Infinity; -Infinity>");
    EXPECT_EQ(Complex(nan, 1.0).ToString(), "<NaN; 1>");
}

TEST(ComplexTests, ToStringPreservesNegativeZero) {
    EXPECT_EQ(Complex(-0.0, -0.0).ToString(), "<-0; -0>");
}

TEST(PIN_ComplexTests, ToStringSkeletonIsUnchangedAndIsNotDecidedHere) {
    // DELIBERATELY PINNED, not repaired. SR-AUD-277 calls the "<a; b>" skeleton a divergence,
    // citing .NET's constructor doc example "(26.1, 18.06)"; the same audit report also links the
    // current .NET Complex source, which gives "<a; b>". The two citations disagree, /rv is
    // absent and no managed probe measured this member, so #2174 owns the question. If it is ever
    // answered, this is the test to change.
    const std::string s = Complex(3.0, 4.0).ToString();
    ASSERT_FALSE(s.empty());
    EXPECT_EQ(s.front(), '<');
    EXPECT_EQ(s.back(), '>');
    EXPECT_NE(s.find("; "), std::string::npos);
    EXPECT_EQ(s.find(','), std::string::npos);
    EXPECT_EQ(s, "<3; 4>");
}

// ---------------------------------------------------------------------------
// New methods: Asin/Acos/Atan/Sinh/Cosh/Tanh/Log(base)/Reciprocal/FromPolar
// ---------------------------------------------------------------------------

TEST(ComplexTests, Sinh_Zero_IsZero) {
    Complex r = Complex::Sinh(Complex::Zero);
    EXPECT_NEAR(r.getRealProperty(), 0.0, 1e-12);
}

TEST(ComplexTests, Cosh_Zero_IsOne) {
    Complex r = Complex::Cosh(Complex::Zero);
    EXPECT_NEAR(r.getRealProperty(), 1.0, 1e-12);
}

TEST(ComplexTests, Tanh_Zero_IsZero) {
    Complex r = Complex::Tanh(Complex::Zero);
    EXPECT_NEAR(r.getRealProperty(), 0.0, 1e-12);
}

TEST(ComplexTests, Asin_Zero_IsZero) {
    Complex r = Complex::Asin(Complex::Zero);
    EXPECT_NEAR(r.getRealProperty(), 0.0, 1e-12);
}

TEST(ComplexTests, Acos_One_IsZero) {
    Complex r = Complex::Acos(Complex::One);
    EXPECT_NEAR(r.getRealProperty(), 0.0, 1e-12);
}

TEST(ComplexTests, Atan_Zero_IsZero) {
    Complex r = Complex::Atan(Complex::Zero);
    EXPECT_NEAR(r.getRealProperty(), 0.0, 1e-12);
}

TEST(ComplexTests, Log_Base10) {
    Complex r = Complex::Log(Complex(100.0, 0.0), 10.0);
    EXPECT_NEAR(r.getRealProperty(), 2.0, 1e-10);
}

TEST(ComplexTests, Reciprocal_One_IsOne) {
    Complex r = Complex::Reciprocal(Complex::One);
    EXPECT_NEAR(r.getRealProperty(), 1.0, 1e-12);
}

TEST(ComplexTests, Reciprocal_Two_IsHalf) {
    Complex r = Complex::Reciprocal(Complex(2.0, 0.0));
    EXPECT_NEAR(r.getRealProperty(), 0.5, 1e-12);
}

TEST(ComplexTests, Reciprocal_Zero_IsZero) {
    Complex r = Complex::Reciprocal(Complex::Zero);
    EXPECT_EQ(r.getRealProperty(), 0.0);
    EXPECT_EQ(r.getImaginaryProperty(), 0.0);
}

TEST(ComplexTests, FromPolarCoordinates_UnitAngle) {
    Complex r = Complex::FromPolarCoordinates(1.0, 0.0);
    EXPECT_NEAR(r.getRealProperty(),      1.0, 1e-12);
    EXPECT_NEAR(r.getImaginaryProperty(), 0.0, 1e-12);
}
