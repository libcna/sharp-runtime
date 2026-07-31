// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <cmath>
#include <limits>
#include <string>

#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Double.hpp"

using System::Double;

// Suite name uses a Tests2 suffix per NEXT.md's documented duplicate-suite-name
// policy: DoubleTests already exists in DoubleTests.cpp / PrimitiveTypeTests2.cpp.

// ---------------------------------------------------------------------------
// MaxMagnitude / MinMagnitude
// ---------------------------------------------------------------------------

TEST(DoubleTests2, MaxMagnitude_PicksLargerAbsoluteValue) {
    EXPECT_EQ(Double::MaxMagnitude(-5.0, 3.0), -5.0);
    EXPECT_EQ(Double::MaxMagnitude(-5.0, 5.0), 5.0); // tie -> prefers the positive value
}

TEST(DoubleTests2, MinMagnitude_PicksSmallerAbsoluteValue) {
    EXPECT_EQ(Double::MinMagnitude(-5.0, 3.0), 3.0);
    EXPECT_EQ(Double::MinMagnitude(-5.0, 5.0), -5.0); // tie -> prefers the negative value
}

// ---------------------------------------------------------------------------
// Rounding
// ---------------------------------------------------------------------------

TEST(DoubleTests2, Ceiling) { EXPECT_EQ(Double::Ceiling(1.2), 2.0); }
TEST(DoubleTests2, Floor)   { EXPECT_EQ(Double::Floor(1.8), 1.0); }
TEST(DoubleTests2, Truncate_NegativeTowardZero) { EXPECT_EQ(Double::Truncate(-1.8), -1.0); }
TEST(DoubleTests2, Round_TiesToEven) {
    EXPECT_EQ(Double::Round(2.5), 2.0);
    EXPECT_EQ(Double::Round(3.5), 4.0);
}
TEST(DoubleTests2, Round_WithDigits) {
    EXPECT_NEAR(Double::Round(3.14159, 2), 3.14, 1e-9);
}

// ---------------------------------------------------------------------------
// Exponential / logarithmic
// ---------------------------------------------------------------------------

TEST(DoubleTests2, Exp_Zero_IsOne)  { EXPECT_DOUBLE_EQ(Double::Exp(0.0), 1.0); }
TEST(DoubleTests2, Exp2_Three)      { EXPECT_DOUBLE_EQ(Double::Exp2(3.0), 8.0); }
TEST(DoubleTests2, Exp10_Two)       { EXPECT_NEAR(Double::Exp10(2.0), 100.0, 1e-9); }
TEST(DoubleTests2, Log_E_IsOne)     { EXPECT_NEAR(Double::Log(Double::E), 1.0, 1e-12); }
TEST(DoubleTests2, Log_Base8_2Is3)  { EXPECT_NEAR(Double::Log(8.0, 2.0), 3.0, 1e-9); }
TEST(DoubleTests2, Log2_Eight)      { EXPECT_DOUBLE_EQ(Double::Log2(8.0), 3.0); }
TEST(DoubleTests2, Log10_Hundred)   { EXPECT_DOUBLE_EQ(Double::Log10(100.0), 2.0); }
TEST(DoubleTests2, Pow_TwoCubed)    { EXPECT_DOUBLE_EQ(Double::Pow(2.0, 3.0), 8.0); }

// ---------------------------------------------------------------------------
// Root / reciprocal
// ---------------------------------------------------------------------------

TEST(DoubleTests2, Sqrt_Nine)          { EXPECT_DOUBLE_EQ(Double::Sqrt(9.0), 3.0); }
TEST(DoubleTests2, Cbrt_TwentySeven)   { EXPECT_NEAR(Double::Cbrt(27.0), 3.0, 1e-12); }
TEST(DoubleTests2, RootN_FourthRoot)   { EXPECT_NEAR(Double::RootN(16.0, 4), 2.0, 1e-9); }
TEST(DoubleTests2, ReciprocalEstimate) { EXPECT_NEAR(Double::ReciprocalEstimate(4.0), 0.25, 1e-9); }
TEST(DoubleTests2, ReciprocalSqrtEstimate) { EXPECT_NEAR(Double::ReciprocalSqrtEstimate(4.0), 0.5, 1e-9); }

// ---------------------------------------------------------------------------
// Trigonometric
// ---------------------------------------------------------------------------

TEST(DoubleTests2, Sin_Zero)   { EXPECT_NEAR(Double::Sin(0.0), 0.0, 1e-12); }
TEST(DoubleTests2, Cos_Zero)   { EXPECT_NEAR(Double::Cos(0.0), 1.0, 1e-12); }
TEST(DoubleTests2, Tan_Zero)   { EXPECT_NEAR(Double::Tan(0.0), 0.0, 1e-12); }
TEST(DoubleTests2, Asin_One)   { EXPECT_NEAR(Double::Asin(1.0), Double::Pi / 2.0, 1e-9); }
TEST(DoubleTests2, Acos_One)   { EXPECT_NEAR(Double::Acos(1.0), 0.0, 1e-12); }
TEST(DoubleTests2, Atan_One)   { EXPECT_NEAR(Double::Atan(1.0), Double::Pi / 4.0, 1e-9); }
TEST(DoubleTests2, Atan2_Basic) { EXPECT_NEAR(Double::Atan2(1.0, 1.0), Double::Pi / 4.0, 1e-9); }
TEST(DoubleTests2, SinPi_Half)  { EXPECT_NEAR(Double::SinPi(0.5), 1.0, 1e-9); }
TEST(DoubleTests2, CosPi_Zero)  { EXPECT_NEAR(Double::CosPi(0.0), 1.0, 1e-12); }
TEST(DoubleTests2, TanPi_Quarter) { EXPECT_NEAR(Double::TanPi(0.25), 1.0, 1e-9); }
TEST(DoubleTests2, AcosPi_One)  { EXPECT_NEAR(Double::AcosPi(1.0), 0.0, 1e-12); }
TEST(DoubleTests2, AsinPi_One)  { EXPECT_NEAR(Double::AsinPi(1.0), 0.5, 1e-9); }
TEST(DoubleTests2, AtanPi_One)  { EXPECT_NEAR(Double::AtanPi(1.0), 0.25, 1e-9); }
TEST(DoubleTests2, Atan2Pi_Basic) { EXPECT_NEAR(Double::Atan2Pi(1.0, 1.0), 0.25, 1e-9); }

TEST(DoubleTests2, SinCos_MatchesIndividual) {
    auto r = Double::SinCos(0.7);
    EXPECT_NEAR(r.Sin, Double::Sin(0.7), 1e-12);
    EXPECT_NEAR(r.Cos, Double::Cos(0.7), 1e-12);
}

TEST(DoubleTests2, SinCosPi_MatchesIndividual) {
    auto r = Double::SinCosPi(0.3);
    EXPECT_NEAR(r.SinPi, Double::SinPi(0.3), 1e-12);
    EXPECT_NEAR(r.CosPi, Double::CosPi(0.3), 1e-12);
}

// ---------------------------------------------------------------------------
// Pi-scaled trig turn-boundary fidelity (SR-AUD-032, CCF-007, ticket #1861).
// The naive std::sin(x*Pi)/std::cos(x*Pi)/std::tan(x*Pi) implementation leaked
// tiny nonzero residues (and the wrong sign of zero) at integer and
// half-integer turns; the .NET-shaped reduction returns exact, sign-correct
// results there. Runtime (volatile) operands defeat constant folding.
// ---------------------------------------------------------------------------

TEST(DoubleTests2, SinPi_IntegerTurns_SignedZero) {
    volatile double one = 1.0, mone = -1.0, two = 2.0, mzero = -0.0;
    EXPECT_EQ(Double::SinPi(one), 0.0);
    EXPECT_FALSE(std::signbit(Double::SinPi(one)));   // SinPi(+1) == +0
    EXPECT_EQ(Double::SinPi(mone), 0.0);
    EXPECT_TRUE(std::signbit(Double::SinPi(mone)));    // SinPi(-1) == -0
    EXPECT_FALSE(std::signbit(Double::SinPi(two)));    // SinPi(+2) == +0
    EXPECT_TRUE(std::signbit(Double::SinPi(mzero)));   // SinPi(-0) == -0
}

TEST(DoubleTests2, SinPi_HalfTurns_ExactUnit) {
    volatile double half = 0.5, threehalf = 1.5, fivehalf = 2.5;
    EXPECT_EQ(Double::SinPi(half), 1.0);
    EXPECT_EQ(Double::SinPi(threehalf), -1.0);
    EXPECT_EQ(Double::SinPi(fivehalf), 1.0);
}

TEST(DoubleTests2, CosPi_HalfTurns_ExactZero) {
    volatile double half = 0.5, threehalf = 1.5;
    EXPECT_EQ(Double::CosPi(half), 0.0);
    EXPECT_FALSE(std::signbit(Double::CosPi(half)));   // CosPi(0.5) == +0
    EXPECT_EQ(Double::CosPi(threehalf), 0.0);
}

TEST(DoubleTests2, CosPi_IntegerTurns_ExactUnit) {
    volatile double one = 1.0, two = 2.0, three = 3.0;
    EXPECT_EQ(Double::CosPi(one), -1.0);
    EXPECT_EQ(Double::CosPi(two), 1.0);
    EXPECT_EQ(Double::CosPi(three), -1.0);
}

TEST(DoubleTests2, TanPi_IntegerTurns_SignedZero) {
    // .NET convention: TanPi(k) carries sign(x) * (odd(k) ? -0 : +0).
    volatile double one = 1.0, mone = -1.0, two = 2.0;
    EXPECT_EQ(Double::TanPi(one), 0.0);
    EXPECT_TRUE(std::signbit(Double::TanPi(one)));     // TanPi(+1) == -0
    EXPECT_EQ(Double::TanPi(mone), 0.0);
    EXPECT_FALSE(std::signbit(Double::TanPi(mone)));   // TanPi(-1) == +0
    EXPECT_FALSE(std::signbit(Double::TanPi(two)));    // TanPi(+2) == +0
}

TEST(DoubleTests2, TanPi_HalfTurns_SignedInfinity) {
    volatile double half = 0.5, threehalf = 1.5;
    EXPECT_TRUE(Double::IsPositiveInfinity(Double::TanPi(half)));
    EXPECT_TRUE(Double::IsNegativeInfinity(Double::TanPi(threehalf)));
}

TEST(DoubleTests2, PiTrig_NonFinite_ReturnsNaN) {
    EXPECT_TRUE(std::isnan(Double::SinPi(Double::NaN)));
    EXPECT_TRUE(std::isnan(Double::CosPi(Double::NaN)));
    EXPECT_TRUE(std::isnan(Double::TanPi(Double::NaN)));
    EXPECT_TRUE(std::isnan(Double::SinPi(Double::PositiveInfinity)));
    EXPECT_TRUE(std::isnan(Double::CosPi(Double::NegativeInfinity)));
    EXPECT_TRUE(std::isnan(Double::TanPi(Double::PositiveInfinity)));
    auto sc = Double::SinCosPi(Double::PositiveInfinity);
    EXPECT_TRUE(std::isnan(sc.SinPi));
    EXPECT_TRUE(std::isnan(sc.CosPi));
}

TEST(DoubleTests2, PiTrig_OrdinaryValues_MatchLibm) {
    // Quarter turn and a spread of ordinary fractions stay within double ULPs
    // of the libm reference (property check, not a brittle digit string).
    volatile double q = 0.25;
    EXPECT_NEAR(Double::SinPi(q), std::sin(M_PI * 0.25), 1e-12);
    EXPECT_NEAR(Double::CosPi(q), std::cos(M_PI * 0.25), 1e-12);
    EXPECT_NEAR(Double::TanPi(q), 1.0, 1e-12);
    for (double f = 0.05; f < 3.0; f += 0.17) {
        volatile double vf = f;
        EXPECT_NEAR(Double::SinPi(vf), std::sin(M_PI * f), 1e-9) << "SinPi @ " << f;
        EXPECT_NEAR(Double::CosPi(vf), std::cos(M_PI * f), 1e-9) << "CosPi @ " << f;
    }
}

TEST(DoubleTests2, SinCosPi_TurnBoundaries_MatchScalar) {
    volatile double one = 1.0, half = 0.5;
    auto a = Double::SinCosPi(one);
    EXPECT_EQ(a.SinPi, 0.0);
    EXPECT_FALSE(std::signbit(a.SinPi));
    EXPECT_EQ(a.CosPi, -1.0);
    auto b = Double::SinCosPi(half);
    EXPECT_EQ(b.SinPi, 1.0);
    EXPECT_EQ(b.CosPi, 0.0);
}

TEST(DoubleTests2, PiTrig_LargeIntegers_BeyondMantissa) {
    // |x| in [2^52, 2^53): every value is an integer; the raw-bit parity decides
    // Cos. |x| >= 2^53: x is necessarily an even integer.
    volatile double oddMid = 4503599627370497.0;   // 2^52 + 1, an odd integer < 2^53
    volatile double evenBig = 18014398509481984.0; // 2^54, an even integer >= 2^53
    EXPECT_EQ(Double::SinPi(oddMid), 0.0);
    EXPECT_EQ(Double::CosPi(oddMid), -1.0);        // odd -> -1
    EXPECT_EQ(Double::SinPi(evenBig), 0.0);
    EXPECT_EQ(Double::CosPi(evenBig), 1.0);        // even integer -> +1
}

// ---------------------------------------------------------------------------
// Hyperbolic
// ---------------------------------------------------------------------------

TEST(DoubleTests2, Sinh_Zero)  { EXPECT_NEAR(Double::Sinh(0.0), 0.0, 1e-12); }
TEST(DoubleTests2, Cosh_Zero)  { EXPECT_NEAR(Double::Cosh(0.0), 1.0, 1e-12); }
TEST(DoubleTests2, Tanh_Zero)  { EXPECT_NEAR(Double::Tanh(0.0), 0.0, 1e-12); }
TEST(DoubleTests2, Asinh_Zero) { EXPECT_NEAR(Double::Asinh(0.0), 0.0, 1e-12); }
TEST(DoubleTests2, Acosh_One)  { EXPECT_NEAR(Double::Acosh(1.0), 0.0, 1e-12); }
TEST(DoubleTests2, Atanh_Zero) { EXPECT_NEAR(Double::Atanh(0.0), 0.0, 1e-12); }

// ---------------------------------------------------------------------------
// IEEE 754 utilities
// ---------------------------------------------------------------------------

TEST(DoubleTests2, Hypot_3_4_Is5) { EXPECT_NEAR(Double::Hypot(3.0, 4.0), 5.0, 1e-12); }

TEST(DoubleTests2, FusedMultiplyAdd) {
    EXPECT_DOUBLE_EQ(Double::FusedMultiplyAdd(2.0, 3.0, 1.0), 7.0);
}

TEST(DoubleTests2, Ieee754Remainder) {
    EXPECT_NEAR(Double::Ieee754Remainder(5.0, 3.0), -1.0, 1e-12);
}

TEST(DoubleTests2, BitIncrement_IncreasesValue) {
    EXPECT_GT(Double::BitIncrement(1.0), 1.0);
}

TEST(DoubleTests2, BitDecrement_DecreasesValue) {
    EXPECT_LT(Double::BitDecrement(1.0), 1.0);
}

TEST(DoubleTests2, ILogB_PowerOfTwo) {
    EXPECT_EQ(Double::ILogB(8.0), 3);
}

// SR-AUD-031 (#1859, CCF-007): .NET reserves int.MinValue for zero and returns
// int.MaxValue for NaN and both infinities; std::ilogb collided NaN with zero (INT_MIN).
TEST(DoubleTests2, ILogB_SpecialValues) {
    EXPECT_EQ(Double::ILogB(0.0),  std::numeric_limits<int>::min());
    EXPECT_EQ(Double::ILogB(-0.0), std::numeric_limits<int>::min());
    EXPECT_EQ(Double::ILogB(Double::PositiveInfinity), std::numeric_limits<int>::max());
    EXPECT_EQ(Double::ILogB(Double::NegativeInfinity), std::numeric_limits<int>::max());
    EXPECT_EQ(Double::ILogB(Double::NaN), std::numeric_limits<int>::max());
    EXPECT_EQ(Double::ILogB(Double::Epsilon), -1074);   // smallest subnormal, 2^-1074
    EXPECT_EQ(Double::ILogB(1.0), 0);
}

TEST(DoubleTests2, ScaleB_MultipliesByPowerOfTwo) {
    EXPECT_DOUBLE_EQ(Double::ScaleB(1.0, 3), 8.0);
}

// ---------------------------------------------------------------------------
// Angle conversion
// ---------------------------------------------------------------------------

TEST(DoubleTests2, DegreesToRadians_180IsPi) {
    EXPECT_NEAR(Double::DegreesToRadians(180.0), Double::Pi, 1e-12);
}

TEST(DoubleTests2, RadiansToDegrees_PiIs180) {
    EXPECT_NEAR(Double::RadiansToDegrees(Double::Pi), 180.0, 1e-9);
}

// ---------------------------------------------------------------------------
// Comparison: CompareTo / Equals / GetHashCode
// (.NET parity: NaN sorts below all values and equals itself in CompareTo/Equals,
// but == still returns false for NaN == NaN.)
// ---------------------------------------------------------------------------

TEST(DoubleTests2, CompareTo_Ordering) {
    EXPECT_LT(Double::CompareTo(1.0, 2.0), 0);
    EXPECT_GT(Double::CompareTo(2.0, 1.0), 0);
    EXPECT_EQ(Double::CompareTo(1.0, 1.0), 0);
}

TEST(DoubleTests2, CompareTo_NaNSortsBelowEverything) {
    EXPECT_LT(Double::CompareTo(Double::NaN, Double::NegativeInfinity), 0);
    EXPECT_GT(Double::CompareTo(Double::NegativeInfinity, Double::NaN), 0);
}

TEST(DoubleTests2, CompareTo_NaNEqualsNaN) {
    EXPECT_EQ(Double::CompareTo(Double::NaN, Double::NaN), 0);
}

TEST(DoubleTests2, Equals_NaNEqualsNaN) {
    EXPECT_TRUE(Double::Equals(Double::NaN, Double::NaN));
    // Contrast with operator==, which follows IEEE 754 (NaN != NaN).
    EXPECT_FALSE(Double::NaN == Double::NaN);
}

TEST(DoubleTests2, Equals_RegularValues) {
    EXPECT_TRUE(Double::Equals(1.5, 1.5));
    EXPECT_FALSE(Double::Equals(1.5, 2.5));
}

TEST(DoubleTests2, GetHashCode_EqualValuesHaveSameHash) {
    EXPECT_EQ(Double::GetHashCode(1.5), Double::GetHashCode(1.5));
}

TEST(DoubleTests2, GetHashCode_PositiveAndNegativeZeroMatch) {
    EXPECT_EQ(Double::GetHashCode(0.0), Double::GetHashCode(Double::NegativeZero));
}

TEST(DoubleTests2, GetHashCode_AllNaNsMatch) {
    double nan1 = Double::NaN;
    double nan2 = -Double::NaN;
    EXPECT_EQ(Double::GetHashCode(nan1), Double::GetHashCode(nan2));
}

// ---------------------------------------------------------------------------
// SR-AUD-029 (#1862, approved 2026-07-31): Round(x, digits) validates digits.
//
// .NET's Double.Round(x,digits) forwards to Math.Round(x,digits,ToEven), whose
// funnel rejects `(uint)digits > 15` -- so a negative count throws too. The port
// used to be noexcept and fed digits straight to std::pow, silently ignoring an
// oversized count and returning NaN for INTCS_MIN. Adding the check required
// dropping noexcept; that is a source-level change only (an Itanium mangled name
// does not encode noexcept), so no exported symbol or layout moved.
// ---------------------------------------------------------------------------

TEST(DoubleTests2, RoundDigits_ValidRange_UnchangedValues) {
    EXPECT_DOUBLE_EQ(Double::Round(1.2345, 0), 1.0);
    EXPECT_NEAR(Double::Round(3.14159, 2), 3.14, 1e-9);
    EXPECT_NEAR(Double::Round(1.2345, 15), 1.2345, 1e-12);
}

TEST(DoubleTests2, RoundDigits_AboveMaximum_ThrowsAOORE) {
    EXPECT_THROW(Double::Round(1.2345, 16), System::ArgumentOutOfRangeException);
    EXPECT_THROW(Double::Round(1.2345, 99), System::ArgumentOutOfRangeException);
    EXPECT_THROW(Double::Round(1.2345, SharpRuntime::INTCS_MAX),
                 System::ArgumentOutOfRangeException);
}

TEST(DoubleTests2, RoundDigits_Negative_ThrowsAOORE) {
    EXPECT_THROW(Double::Round(1.2345, -3), System::ArgumentOutOfRangeException);
    EXPECT_THROW(Double::Round(1.2345, SharpRuntime::INTCS_MIN),
                 System::ArgumentOutOfRangeException);
}

TEST(DoubleTests2, RoundDigits_ThrowCarriesDotNetParamNameAndMessage) {
    try {
        (void)Double::Round(1.2345, 16);
        FAIL() << "expected ArgumentOutOfRangeException";
    } catch (const System::ArgumentOutOfRangeException& e) {
        EXPECT_EQ(e.getParamNameProperty(), "digits");
        // .NET SR.ArgumentOutOfRange_RoundingDigits, verbatim.
        EXPECT_NE(std::string(e.what()).find(
                      "Rounding digits must be between 0 and 15, inclusive."),
                  std::string::npos)
            << e.what();
    }
}

TEST(DoubleTests2, RoundDigits_LimitIsFifteenNotSix) {
    // The double limit is Math's 15, deliberately wider than MathF's 6.
    EXPECT_NO_THROW((void)Double::Round(1.2345, 7));
    EXPECT_NO_THROW((void)Double::Round(1.2345, 15));
    EXPECT_THROW(Double::Round(1.2345, 16), System::ArgumentOutOfRangeException);
}

TEST(DoubleTests2, RoundDigits_ExceptionSpecificationIsTheApprovedOne) {
    double x = 1.0;
    static_assert(!noexcept(Double::Round(x, 2)),
                  "#1862: Round(double,intcs) must be able to throw");
    static_assert(noexcept(Double::Round(x)),
                  "#1862: Round(double) must stay noexcept");
    EXPECT_DOUBLE_EQ(Double::Round(2.5), 2.0);  // ties-to-even, unchanged
    EXPECT_DOUBLE_EQ(Double::Round(3.5), 4.0);
}
