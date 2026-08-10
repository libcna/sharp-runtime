// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <bit>
#include <cstdint>
#include "System/Half.hpp"
#include "System/MathF.hpp"
#include "System/Single.hpp"
#include "System/ArgumentException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/ArithmeticException.hpp"
#include "System/FormatException.hpp"
#include <cmath>
#include <limits>
#include <string>

using System::Single;
using System::ArithmeticException;

// Constants
TEST(SingleTest, MaxValue_Positive)      { EXPECT_GT(Single::MaxValue, 0.0f); }
TEST(SingleTest, MinValue_Negative)      { EXPECT_LT(Single::MinValue, 0.0f); }
TEST(SingleTest, Epsilon_Positive)       { EXPECT_GT(Single::Epsilon, 0.0f); }
TEST(SingleTest, NaN_IsNaN)             { EXPECT_TRUE(std::isnan(Single::NaN)); }
TEST(SingleTest, PositiveInfinity)       { EXPECT_TRUE(std::isinf(Single::PositiveInfinity)); EXPECT_GT(Single::PositiveInfinity, 0.0f); }
TEST(SingleTest, NegativeInfinity)       { EXPECT_TRUE(std::isinf(Single::NegativeInfinity)); EXPECT_LT(Single::NegativeInfinity, 0.0f); }
TEST(SingleTest, Pi_Approx)             { EXPECT_NEAR(Single::Pi, 3.14159f, 0.0001f); }
TEST(SingleTest, E_Approx)              { EXPECT_NEAR(Single::E, 2.71828f, 0.0001f); }
TEST(SingleTest, Tau_Is2Pi)             { EXPECT_NEAR(Single::Tau, 2.0f * Single::Pi, 0.0001f); }

// Classification
TEST(SingleTest, IsNaN_True)            { EXPECT_TRUE(Single::IsNaN(Single::NaN)); }
TEST(SingleTest, IsNaN_False)           { EXPECT_FALSE(Single::IsNaN(1.0f)); }
TEST(SingleTest, IsInfinity_PosInf)     { EXPECT_TRUE(Single::IsInfinity(Single::PositiveInfinity)); }
TEST(SingleTest, IsInfinity_NegInf)     { EXPECT_TRUE(Single::IsInfinity(Single::NegativeInfinity)); }
TEST(SingleTest, IsInfinity_False)      { EXPECT_FALSE(Single::IsInfinity(1.0f)); }
TEST(SingleTest, IsPositiveInfinity)    { EXPECT_TRUE(Single::IsPositiveInfinity(Single::PositiveInfinity)); }
TEST(SingleTest, IsNegativeInfinity)    { EXPECT_TRUE(Single::IsNegativeInfinity(Single::NegativeInfinity)); }
TEST(SingleTest, IsFinite_True)         { EXPECT_TRUE(Single::IsFinite(1.0f)); }
TEST(SingleTest, IsFinite_Inf_False)    { EXPECT_FALSE(Single::IsFinite(Single::PositiveInfinity)); }
TEST(SingleTest, IsNormal_True)         { EXPECT_TRUE(Single::IsNormal(1.0f)); }
TEST(SingleTest, IsNegative_Neg)        { EXPECT_TRUE(Single::IsNegative(-1.0f)); }
TEST(SingleTest, IsNegative_Pos_False)  { EXPECT_FALSE(Single::IsNegative(1.0f)); }
TEST(SingleTest, IsPositive_Pos)        { EXPECT_TRUE(Single::IsPositive(1.0f)); }
TEST(SingleTest, IsInteger_True)        { EXPECT_TRUE(Single::IsInteger(3.0f)); }
TEST(SingleTest, IsInteger_False)       { EXPECT_FALSE(Single::IsInteger(3.5f)); }
TEST(SingleTest, IsEvenInteger)         { EXPECT_TRUE(Single::IsEvenInteger(4.0f)); EXPECT_FALSE(Single::IsEvenInteger(3.0f)); }
TEST(SingleTest, IsOddInteger)          { EXPECT_TRUE(Single::IsOddInteger(3.0f)); EXPECT_FALSE(Single::IsOddInteger(4.0f)); }
TEST(SingleTest, IsRealNumber_True)     { EXPECT_TRUE(Single::IsRealNumber(1.0f)); EXPECT_TRUE(Single::IsRealNumber(Single::PositiveInfinity)); }
TEST(SingleTest, IsRealNumber_NaN_False){ EXPECT_FALSE(Single::IsRealNumber(Single::NaN)); }
TEST(SingleTest, IsPow2_True)           { EXPECT_TRUE(Single::IsPow2(8.0f)); }
TEST(SingleTest, IsPow2_False)          { EXPECT_FALSE(Single::IsPow2(6.0f)); EXPECT_FALSE(Single::IsPow2(0.0f)); EXPECT_FALSE(Single::IsPow2(-8.0f)); }

// SR-AUD-030 (#1860, CCF-007): subnormal powers of two must be recognised (one
// trailing-significand bit), matching .NET Single.IsPow2; the normal rule rejected them.
TEST(SingleTest, IsPow2_SubnormalPowersOfTwo) {
    EXPECT_TRUE(Single::IsPow2(Single::Epsilon));            // smallest subnormal = 2^-149
    EXPECT_TRUE(Single::IsPow2(Single::Epsilon * 2.0f));     // 2^-148, still one bit
    EXPECT_TRUE(Single::IsPow2(Single::Epsilon * 4.0f));     // 2^-147
    EXPECT_FALSE(Single::IsPow2(Single::Epsilon * 3.0f));    // two bits -> not a power of two
    EXPECT_FALSE(Single::IsPow2(-Single::Epsilon));          // negative subnormal
    EXPECT_TRUE(Single::IsPow2(1.0f));                       // normal power of two unaffected
}

// Arithmetic
TEST(SingleTest, Abs_Negative)          { EXPECT_EQ(Single::Abs(-5.0f), 5.0f); }
TEST(SingleTest, Abs_Positive)          { EXPECT_EQ(Single::Abs(5.0f), 5.0f); }
TEST(SingleTest, Sign_Positive)         { EXPECT_EQ(Single::Sign(3.0f), 1); }
TEST(SingleTest, Sign_Negative)         { EXPECT_EQ(Single::Sign(-3.0f), -1); }
TEST(SingleTest, Sign_Zero)             { EXPECT_EQ(Single::Sign(0.0f), 0); }
TEST(SingleTest, Sign_NaN_Throws)       { EXPECT_THROW(Single::Sign(Single::NaN), ArithmeticException); }
TEST(SingleTest, Clamp_Within)          { EXPECT_EQ(Single::Clamp(5.0f, 0.0f, 10.0f), 5.0f); }
TEST(SingleTest, Clamp_Below)           { EXPECT_EQ(Single::Clamp(-1.0f, 0.0f, 10.0f), 0.0f); }
TEST(SingleTest, Clamp_Above)           { EXPECT_EQ(Single::Clamp(11.0f, 0.0f, 10.0f), 10.0f); }
TEST(SingleTest, Clamp_MinGreaterThanMax_Throws) { EXPECT_THROW(Single::Clamp(1.0f, 10.0f, 0.0f), System::ArgumentException); }
TEST(SingleTest, Max)                   { EXPECT_EQ(Single::Max(3.0f, 7.0f), 7.0f); }
TEST(SingleTest, Min)                   { EXPECT_EQ(Single::Min(3.0f, 7.0f), 3.0f); }
TEST(SingleTest, Max_PropagatesNaN)     { EXPECT_TRUE(Single::IsNaN(Single::Max(Single::NaN, 1.0f))); EXPECT_TRUE(Single::IsNaN(Single::Max(1.0f, Single::NaN))); }
TEST(SingleTest, Min_PropagatesNaN)     { EXPECT_TRUE(Single::IsNaN(Single::Min(Single::NaN, 1.0f))); EXPECT_TRUE(Single::IsNaN(Single::Min(1.0f, Single::NaN))); }
TEST(SingleTest, Max_PositiveZeroBeatsNegativeZero) { EXPECT_TRUE(Single::IsNegative(Single::Max(-0.0f, -0.0f))); EXPECT_FALSE(Single::IsNegative(Single::Max(0.0f, -0.0f))); }
TEST(SingleTest, MaxMagnitude_TieBreaksToPositive)  { EXPECT_EQ(Single::MaxMagnitude(-0.0f, 0.0f), 0.0f); EXPECT_FALSE(Single::IsNegative(Single::MaxMagnitude(-0.0f, 0.0f))); }
TEST(SingleTest, MinMagnitude_TieBreaksToNegative)  { EXPECT_TRUE(Single::IsNegative(Single::MinMagnitude(0.0f, -0.0f))); }
TEST(SingleTest, MaxMagnitude_Basic)    { EXPECT_EQ(Single::MaxMagnitude(-7.0f, 3.0f), -7.0f); }
TEST(SingleTest, MinMagnitude_Basic)    { EXPECT_EQ(Single::MinMagnitude(-7.0f, 3.0f), 3.0f); }
TEST(SingleTest, CopySign_Neg)          { EXPECT_EQ(Single::CopySign(3.0f, -1.0f), -3.0f); }
TEST(SingleTest, CopySign_Pos)          { EXPECT_EQ(Single::CopySign(-3.0f, 1.0f), 3.0f); }

// Rounding
TEST(SingleTest, Ceiling)               { EXPECT_EQ(Single::Ceiling(1.2f), 2.0f); }
TEST(SingleTest, Floor)                 { EXPECT_EQ(Single::Floor(1.9f), 1.0f); }
TEST(SingleTest, Truncate)              { EXPECT_EQ(Single::Truncate(1.9f), 1.0f); EXPECT_EQ(Single::Truncate(-1.9f), -1.0f); }
TEST(SingleTest, Round_Half)            { EXPECT_EQ(Single::Round(0.5f), 0.0f); } // ties to even
TEST(SingleTest, Round_Digits)          { EXPECT_NEAR(Single::Round(1.456f, 2), 1.46f, 0.001f); }

// Exponential / log
TEST(SingleTest, Exp)                   { EXPECT_NEAR(Single::Exp(1.0f), Single::E, 0.001f); }
TEST(SingleTest, Log_E)                 { EXPECT_NEAR(Single::Log(Single::E), 1.0f, 0.001f); }
TEST(SingleTest, Log2_8)                { EXPECT_NEAR(Single::Log2(8.0f), 3.0f, 0.001f); }
TEST(SingleTest, Log10_100)             { EXPECT_NEAR(Single::Log10(100.0f), 2.0f, 0.001f); }
TEST(SingleTest, Pow)                   { EXPECT_NEAR(Single::Pow(2.0f, 10.0f), 1024.0f, 0.01f); }

// Root
TEST(SingleTest, Sqrt)                  { EXPECT_NEAR(Single::Sqrt(9.0f), 3.0f, 0.001f); }
TEST(SingleTest, Cbrt)                  { EXPECT_NEAR(Single::Cbrt(27.0f), 3.0f, 0.001f); }
TEST(SingleTest, Hypot)                 { EXPECT_NEAR(Single::Hypot(3.0f, 4.0f), 5.0f, 0.001f); }
TEST(SingleTest, RootN_PositiveBase)    { EXPECT_NEAR(Single::RootN(8.0f, 3), 2.0f, 0.001f); }
TEST(SingleTest, RootN_NegativeBase_OddRoot) { EXPECT_NEAR(Single::RootN(-8.0f, 3), -2.0f, 0.001f); }
TEST(SingleTest, RootN_NegativeBase_EvenRoot_NaN) { EXPECT_TRUE(Single::IsNaN(Single::RootN(-8.0f, 4))); }
TEST(SingleTest, RootN_NegativeN)       { EXPECT_NEAR(Single::RootN(8.0f, -3), 0.5f, 0.001f); }

// Trig
TEST(SingleTest, Sin)                   { EXPECT_NEAR(Single::Sin(0.0f), 0.0f, 0.001f); }
TEST(SingleTest, Cos)                   { EXPECT_NEAR(Single::Cos(0.0f), 1.0f, 0.001f); }
TEST(SingleTest, Atan2)                 { EXPECT_NEAR(Single::Atan2(1.0f, 1.0f), Single::Pi / 4.0f, 0.001f); }
TEST(SingleTest, SinCos)               {
    auto r = Single::SinCos(0.0f);
    EXPECT_NEAR(r.Sin, 0.0f, 0.001f);
    EXPECT_NEAR(r.Cos, 1.0f, 0.001f);
}

// ---------------------------------------------------------------------------
// Pi-scaled trig turn-boundary fidelity (SR-AUD-032, CCF-007, ticket #1861).
// The naive std::sin(x*Pi)/std::cos(x*Pi)/std::tan(x*Pi) implementation leaked
// tiny nonzero residues (and the wrong sign of zero) at integer and
// half-integer turns; the .NET-shaped reduction returns exact, sign-correct
// results there. Runtime (volatile) operands defeat constant folding.
// ---------------------------------------------------------------------------

TEST(SingleTest, SinPi_IntegerTurns_SignedZero) {
    volatile float one = 1.0f, mone = -1.0f, two = 2.0f, mzero = -0.0f;
    EXPECT_EQ(Single::SinPi(one), 0.0f);
    EXPECT_FALSE(std::signbit(Single::SinPi(one)));    // SinPi(+1) == +0
    EXPECT_EQ(Single::SinPi(mone), 0.0f);
    EXPECT_TRUE(std::signbit(Single::SinPi(mone)));     // SinPi(-1) == -0
    EXPECT_FALSE(std::signbit(Single::SinPi(two)));     // SinPi(+2) == +0
    EXPECT_TRUE(std::signbit(Single::SinPi(mzero)));    // SinPi(-0) == -0
}

TEST(SingleTest, SinPi_HalfTurns_ExactUnit) {
    volatile float half = 0.5f, threehalf = 1.5f, fivehalf = 2.5f;
    EXPECT_EQ(Single::SinPi(half), 1.0f);
    EXPECT_EQ(Single::SinPi(threehalf), -1.0f);
    EXPECT_EQ(Single::SinPi(fivehalf), 1.0f);
}

TEST(SingleTest, CosPi_HalfTurns_ExactZero) {
    volatile float half = 0.5f, threehalf = 1.5f;
    EXPECT_EQ(Single::CosPi(half), 0.0f);
    EXPECT_FALSE(std::signbit(Single::CosPi(half)));    // CosPi(0.5) == +0
    EXPECT_EQ(Single::CosPi(threehalf), 0.0f);
}

TEST(SingleTest, CosPi_IntegerTurns_ExactUnit) {
    volatile float one = 1.0f, two = 2.0f, three = 3.0f;
    EXPECT_EQ(Single::CosPi(one), -1.0f);
    EXPECT_EQ(Single::CosPi(two), 1.0f);
    EXPECT_EQ(Single::CosPi(three), -1.0f);
}

TEST(SingleTest, TanPi_IntegerTurns_SignedZero) {
    // .NET convention: TanPi(k) carries sign(x) * (odd(k) ? -0 : +0).
    volatile float one = 1.0f, mone = -1.0f, two = 2.0f;
    EXPECT_EQ(Single::TanPi(one), 0.0f);
    EXPECT_TRUE(std::signbit(Single::TanPi(one)));      // TanPi(+1) == -0
    EXPECT_EQ(Single::TanPi(mone), 0.0f);
    EXPECT_FALSE(std::signbit(Single::TanPi(mone)));    // TanPi(-1) == +0
    EXPECT_FALSE(std::signbit(Single::TanPi(two)));     // TanPi(+2) == +0
}

TEST(SingleTest, TanPi_HalfTurns_SignedInfinity) {
    volatile float half = 0.5f, threehalf = 1.5f;
    EXPECT_TRUE(Single::IsPositiveInfinity(Single::TanPi(half)));
    EXPECT_TRUE(Single::IsNegativeInfinity(Single::TanPi(threehalf)));
}

TEST(SingleTest, PiTrig_NonFinite_ReturnsNaN) {
    EXPECT_TRUE(std::isnan(Single::SinPi(Single::NaN)));
    EXPECT_TRUE(std::isnan(Single::CosPi(Single::NaN)));
    EXPECT_TRUE(std::isnan(Single::TanPi(Single::NaN)));
    EXPECT_TRUE(std::isnan(Single::SinPi(Single::PositiveInfinity)));
    EXPECT_TRUE(std::isnan(Single::CosPi(Single::NegativeInfinity)));
    EXPECT_TRUE(std::isnan(Single::TanPi(Single::PositiveInfinity)));
    auto sc = Single::SinCosPi(Single::PositiveInfinity);
    EXPECT_TRUE(std::isnan(sc.SinPi));
    EXPECT_TRUE(std::isnan(sc.CosPi));
}

TEST(SingleTest, PiTrig_OrdinaryValues_MatchLibm) {
    volatile float q = 0.25f;
    EXPECT_NEAR(Single::SinPi(q), std::sin(static_cast<float>(M_PI) * 0.25f), 1e-6f);
    EXPECT_NEAR(Single::CosPi(q), std::cos(static_cast<float>(M_PI) * 0.25f), 1e-6f);
    EXPECT_NEAR(Single::TanPi(q), 1.0f, 1e-6f);
    for (float f = 0.05f; f < 3.0f; f += 0.17f) {
        volatile float vf = f;
        EXPECT_NEAR(Single::SinPi(vf), std::sin(static_cast<float>(M_PI) * f), 1e-5f) << "SinPi @ " << f;
        EXPECT_NEAR(Single::CosPi(vf), std::cos(static_cast<float>(M_PI) * f), 1e-5f) << "CosPi @ " << f;
    }
}

TEST(SingleTest, SinCosPi_TurnBoundaries_MatchScalar) {
    volatile float one = 1.0f, half = 0.5f;
    auto a = Single::SinCosPi(one);
    EXPECT_EQ(a.SinPi, 0.0f);
    EXPECT_FALSE(std::signbit(a.SinPi));
    EXPECT_EQ(a.CosPi, -1.0f);
    auto b = Single::SinCosPi(half);
    EXPECT_EQ(b.SinPi, 1.0f);
    EXPECT_EQ(b.CosPi, 0.0f);
}

TEST(SingleTest, PiTrig_LargeIntegers_BeyondMantissa) {
    // |x| in [2^23, 2^24): every value is an integer; raw-bit parity decides Cos.
    // |x| >= 2^24: x is necessarily an even integer.
    volatile float oddMid = 8388609.0f;   // 2^23 + 1, an odd integer < 2^24
    volatile float evenBig = 33554432.0f; // 2^25, an even integer >= 2^24
    EXPECT_EQ(Single::SinPi(oddMid), 0.0f);
    EXPECT_EQ(Single::CosPi(oddMid), -1.0f);   // odd -> -1
    EXPECT_EQ(Single::SinPi(evenBig), 0.0f);
    EXPECT_EQ(Single::CosPi(evenBig), 1.0f);   // even integer -> +1
}

// Angle conversion
TEST(SingleTest, DegreesToRadians)      { EXPECT_NEAR(Single::DegreesToRadians(180.0f), Single::Pi, 0.001f); }
TEST(SingleTest, RadiansToDegrees)      { EXPECT_NEAR(Single::RadiansToDegrees(Single::Pi), 180.0f, 0.01f); }

// IEEE utilities
TEST(SingleTest, FusedMultiplyAdd)      { EXPECT_NEAR(Single::FusedMultiplyAdd(2.0f, 3.0f, 1.0f), 7.0f, 0.001f); }
TEST(SingleTest, ScaleB)               { EXPECT_EQ(Single::ScaleB(1.0f, 3), 8.0f); }
TEST(SingleTest, ILogB)                { EXPECT_EQ(Single::ILogB(8.0f), 3); }

// SR-AUD-031 (#1859, CCF-007): .NET reserves int.MinValue for zero and returns
// int.MaxValue for NaN and both infinities; std::ilogb collided NaN with zero (INT_MIN).
TEST(SingleTest, ILogB_SpecialValues) {
    EXPECT_EQ(Single::ILogB(0.0f),  std::numeric_limits<int>::min());
    EXPECT_EQ(Single::ILogB(-0.0f), std::numeric_limits<int>::min());
    EXPECT_EQ(Single::ILogB(Single::PositiveInfinity), std::numeric_limits<int>::max());
    EXPECT_EQ(Single::ILogB(Single::NegativeInfinity), std::numeric_limits<int>::max());
    EXPECT_EQ(Single::ILogB(Single::NaN), std::numeric_limits<int>::max());
    EXPECT_EQ(Single::ILogB(Single::Epsilon), -149);   // smallest subnormal, 2^-149
    EXPECT_EQ(Single::ILogB(1.0f), 0);
}
TEST(SingleTest, BitDecrement)          { EXPECT_LT(Single::BitDecrement(1.0f), 1.0f); }
TEST(SingleTest, BitIncrement)          { EXPECT_GT(Single::BitIncrement(1.0f), 1.0f); }
TEST(SingleTest, ReciprocalEstimate)    { EXPECT_NEAR(Single::ReciprocalEstimate(4.0f), 0.25f, 0.001f); }
TEST(SingleTest, ReciprocalSqrtEstimate){ EXPECT_NEAR(Single::ReciprocalSqrtEstimate(4.0f), 0.5f, 0.001f); }

// Comparison / hash
TEST(SingleTest, CompareTo_Less)        { EXPECT_LT(Single::CompareTo(1.0f, 2.0f), 0); }
TEST(SingleTest, CompareTo_Equal)       { EXPECT_EQ(Single::CompareTo(1.0f, 1.0f), 0); }
TEST(SingleTest, CompareTo_Greater)     { EXPECT_GT(Single::CompareTo(2.0f, 1.0f), 0); }
TEST(SingleTest, Equals_True)           { EXPECT_TRUE(Single::Equals(1.0f, 1.0f)); }
TEST(SingleTest, Equals_False)          { EXPECT_FALSE(Single::Equals(1.0f, 2.0f)); }
TEST(SingleTest, Equals_NaN_EqualsNaN)  { EXPECT_TRUE(Single::Equals(Single::NaN, Single::NaN)); }
TEST(SingleTest, GetHashCode_AllNaNsMatch) {
    float nan2 = -Single::NaN;
    EXPECT_EQ(Single::GetHashCode(Single::NaN), Single::GetHashCode(nan2));
}
TEST(SingleTest, GetHashCode_BothZerosMatch) { EXPECT_EQ(Single::GetHashCode(0.0f), Single::GetHashCode(-0.0f)); }

// Parse / ToString
TEST(SingleTest, Parse_Normal)          { EXPECT_NEAR(Single::Parse("3.14"), 3.14f, 0.001f); }
TEST(SingleTest, Parse_NaN)             { EXPECT_TRUE(std::isnan(Single::Parse("NaN"))); }
TEST(SingleTest, Parse_PosInf)          { EXPECT_TRUE(std::isinf(Single::Parse("Infinity"))); }
TEST(SingleTest, Parse_NegInf)          { EXPECT_TRUE(std::isinf(Single::Parse("-Infinity"))); }
TEST(SingleTest, Parse_Invalid_Throws)  { EXPECT_THROW(Single::Parse("abc"), System::FormatException); }
TEST(SingleTest, TryParse_Valid)        { float r = 0; EXPECT_TRUE(Single::TryParse("1.5", r)); EXPECT_NEAR(r, 1.5f, 0.001f); }
TEST(SingleTest, TryParse_Invalid)      { float r = 0; EXPECT_FALSE(Single::TryParse("xyz", r)); }

// Regression tests for a code-audit finding (ticket 249, same bug class as ticket 245's
// System::Double fix): Parse/TryParse only special-cased the exact-cased strings
// "NaN"/"Infinity"/"-Infinity" and fell through to std::from_chars for everything else, which
// per the C++ standard still recognizes "inf"/"infinity"/"nan"/"nan(n-char-seq)"
// case-insensitively regardless of chars_format -- so abbreviated/miscased spellings like "inf"
// or "nan(123)" were silently ACCEPTED even though real .NET's float.Parse throws
// FormatException for all of them (verified against Number.Parsing.cs's TryParseFloat, which
// only recognizes Infinity/+Infinity/-Infinity/NaN/+NaN/-NaN case-insensitively).
TEST(SingleTest, TryParse_AbbreviatedInfinitySpelling_ReturnsFalse) {
    float r = 0;
    EXPECT_FALSE(Single::TryParse("inf", r));
    EXPECT_FALSE(Single::TryParse("-inf", r));
    EXPECT_FALSE(Single::TryParse("INF", r));
}

TEST(SingleTest, TryParse_NanWithCharSequence_ReturnsFalse) {
    float r = 0;
    EXPECT_FALSE(Single::TryParse("nan(123)", r));
}

TEST(SingleTest, TryParse_CaseInsensitiveCanonicalSpellings_ReturnsTrue) {
    float r = 0;
    EXPECT_TRUE(Single::TryParse("infinity", r));
    EXPECT_TRUE(std::isinf(r) && r > 0);
    EXPECT_TRUE(Single::TryParse("nan", r));
    EXPECT_TRUE(std::isnan(r));
    EXPECT_TRUE(Single::TryParse("+Infinity", r));
    EXPECT_TRUE(std::isinf(r) && r > 0);
}

TEST(SingleTest, Parse_AbbreviatedInfinitySpelling_Throws) {
    EXPECT_THROW(Single::Parse("inf"), System::FormatException);
    EXPECT_THROW(Single::Parse("nan(123)"), System::FormatException);
}

// SR-AUD-033 whitespace slice (#1864, CCF-007): .NET's default NumberStyles.Float
// permits leading/trailing whitespace (AllowLeadingWhite | AllowTrailingWhite),
// but never interior whitespace or an empty/all-whitespace string.
TEST(SingleTest, Parse_LeadingTrailingWhitespace_Accepted) {
    EXPECT_NEAR(Single::Parse(" 1.5 "), 1.5f, 0.0f);
    EXPECT_NEAR(Single::Parse("\t3.14\n"), 3.14f, 0.001f);
    EXPECT_NEAR(Single::Parse("  -2.5"), -2.5f, 0.0f);
    EXPECT_NEAR(Single::Parse("42\r\n"), 42.0f, 0.0f);
}

TEST(SingleTest, Parse_WhitespaceAroundSpecialTokens_Accepted) {
    EXPECT_TRUE(std::isnan(Single::Parse("  NaN  ")));
    EXPECT_TRUE(std::isinf(Single::Parse(" Infinity ")) && Single::Parse(" Infinity ") > 0);
    float r = 0;
    EXPECT_TRUE(Single::TryParse("\t-Infinity\t", r));
    EXPECT_TRUE(std::isinf(r) && r < 0);
}

TEST(SingleTest, Parse_InteriorWhitespace_StillRejected) {
    float r = 0;
    EXPECT_FALSE(Single::TryParse("1 5", r));
    EXPECT_FALSE(Single::TryParse("1. 5", r));
    EXPECT_FALSE(Single::TryParse("N aN", r));
    EXPECT_THROW(Single::Parse("1 5"), System::FormatException);
}

TEST(SingleTest, Parse_EmptyOrAllWhitespace_StillRejected) {
    float r = 0;
    EXPECT_FALSE(Single::TryParse("", r));
    EXPECT_FALSE(Single::TryParse("   ", r));
    EXPECT_FALSE(Single::TryParse("\t\n", r));
    EXPECT_THROW(Single::Parse("   "), System::FormatException);
}
TEST(SingleTest, ToString_Normal)       { EXPECT_EQ(Single::ToString(Single::NaN), "NaN"); }
TEST(SingleTest, ToString_Format_F2)    { EXPECT_EQ(Single::ToString(3.14159f, "F2"), "3.14"); }

// SR-AUD-021 float slice (#1849 / CCF-006): a malformed precision no longer leaks a
// std::stoi exception, and an unrecognised specifier is rejected loudly instead of
// silently round-tripping. Matches the integer wrappers (#1847) and .NET.
TEST(SingleTest, ToString_MalformedPrecision_ThrowsFormatException) {
    EXPECT_THROW(Single::ToString(1.0f, "Fx"), System::FormatException);
}
TEST(SingleTest, ToString_OversizedPrecision_ThrowsFormatException) {
    EXPECT_THROW(Single::ToString(1.0f, "F99999999999"), System::FormatException);
}
TEST(SingleTest, ToString_UnknownSpecifier_ThrowsFormatException) {
    EXPECT_THROW(Single::ToString(1.0f, "Q"), System::FormatException);
}
TEST(SingleTest, ToString_UnknownSpecifierWithDigits_ThrowsFormatException) {
    EXPECT_THROW(Single::ToString(1.0f, "Z2"), System::FormatException);
}
TEST(SingleTest, ToString_CurrencySpecifier_ThrowsFormatException) {
    EXPECT_THROW(Single::ToString(1.0f, "C2"), System::FormatException);  // C is unimplemented here
}
TEST(SingleTest, ToString_ValidSpecifiers_StillWork) {
    EXPECT_EQ(Single::ToString(3.14159f, "F2"), "3.14");
    EXPECT_FALSE(Single::ToString(1000.0f, "E3").empty());
    EXPECT_FALSE(Single::ToString(3.14159f, "G").empty());
    EXPECT_EQ(Single::ToString(2.5f, "R"), Single::ToString(2.5f));
    EXPECT_FALSE(Single::ToString(1.5f, "N2").empty());
}

// ---------------------------------------------------------------------------
// SR-AUD-029 (#1862, approved 2026-07-31): Round(x, digits) validates digits.
//
// .NET's Single.Round(x,digits) forwards to MathF.Round(x,digits,ToEven), whose
// funnel rejects `(uint)digits > 6` -- so a negative count throws too. The port
// used to be noexcept and fed digits straight to std::pow, returning a spurious
// value or NaN with no diagnostic. Adding the check required dropping noexcept;
// that is a source-level change only (an Itanium mangled name does not encode
// noexcept), so no exported symbol, parameter list, return type or layout moved.
// ---------------------------------------------------------------------------

TEST(SingleTest, RoundDigits_ValidRange_UnchangedValues) {
    EXPECT_FLOAT_EQ(Single::Round(1.2345f, 0), 1.0f);
    EXPECT_NEAR(Single::Round(1.2345f, 2), 1.23f, 1e-6f);
    EXPECT_NEAR(Single::Round(1.456f, 2), 1.46f, 1e-3f);
    EXPECT_NEAR(Single::Round(1.2345f, 6), 1.2345f, 1e-6f);
}

TEST(SingleTest, RoundDigits_AboveMaximum_ThrowsAOORE) {
    EXPECT_THROW(Single::Round(1.2345f, 7), System::ArgumentOutOfRangeException);
    EXPECT_THROW(Single::Round(1.2345f, 99), System::ArgumentOutOfRangeException);
    EXPECT_THROW(Single::Round(1.2345f, SharpRuntime::INTCS_MAX),
                 System::ArgumentOutOfRangeException);
}

TEST(SingleTest, RoundDigits_Negative_ThrowsAOORE) {
    EXPECT_THROW(Single::Round(1.2345f, -1), System::ArgumentOutOfRangeException);
    EXPECT_THROW(Single::Round(1.2345f, SharpRuntime::INTCS_MIN),
                 System::ArgumentOutOfRangeException);
}

TEST(SingleTest, RoundDigits_ThrowCarriesDotNetParamNameAndMessage) {
    try {
        (void)Single::Round(1.2345f, 7);
        FAIL() << "expected ArgumentOutOfRangeException";
    } catch (const System::ArgumentOutOfRangeException& e) {
        EXPECT_EQ(e.getParamNameProperty(), "digits");
        // .NET SR.ArgumentOutOfRange_RoundingDigits_MathF, verbatim.
        EXPECT_NE(std::string(e.what()).find(
                      "Rounding digits must be between 0 and 6, inclusive."),
                  std::string::npos)
            << e.what();
    }
}

TEST(SingleTest, RoundDigits_LimitIsSixNotFifteen) {
    // The float limit is MathF's 6, deliberately distinct from Math's 15 --
    // float carries only ~7 significant decimal digits.
    EXPECT_NO_THROW((void)Single::Round(1.2345f, 6));
    EXPECT_THROW(Single::Round(1.2345f, 15), System::ArgumentOutOfRangeException);
}

TEST(SingleTest, RoundDigits_ExceptionSpecificationIsTheApprovedOne) {
    float x = 1.0f;
    // The two-argument overload gave up noexcept so it can reject digits (#1862).
    static_assert(!noexcept(Single::Round(x, 2)),
                  "#1862: Round(float,intcs) must be able to throw");
    // The one-argument overload validates nothing and keeps its noexcept.
    static_assert(noexcept(Single::Round(x)),
                  "#1862: Round(float) must stay noexcept");
    EXPECT_FLOAT_EQ(Single::Round(0.5f), 0.0f);  // ties-to-even, unchanged
    EXPECT_FLOAT_EQ(Single::Round(1.5f), 2.0f);
}

TEST(SingleTest, RoundDigits_ApprovedDelegateCoversBoundariesAndSpecialValues) {
    const float below = std::nextafter(1e8f, 0.0f);
    const float above = std::nextafter(1e8f, std::numeric_limits<float>::infinity());
    const float values[] = {
        0.0f, -0.0f, Single::Epsilon, -Single::Epsilon,
        std::numeric_limits<float>::min(), -std::numeric_limits<float>::min(),
        123.4567f, -123.4567f, below, -below, 1e8f, -1e8f, above, -above,
        3e38f, -3e38f, Single::PositiveInfinity, Single::NegativeInfinity,
    };
    for (float value : values) {
        for (int digits = 0; digits <= 6; ++digits) {
            SCOPED_TRACE(::testing::Message() << "value=" << value << " digits=" << digits);
            EXPECT_EQ(std::bit_cast<std::uint32_t>(Single::Round(value, digits)),
                      std::bit_cast<std::uint32_t>(
                          System::MathF::Round(value, digits, System::MidpointRounding::ToEven)));
        }
    }
    EXPECT_TRUE(std::isfinite(Single::Round(3e38f, 6)));
    EXPECT_FLOAT_EQ(Single::Round(3e38f, 6), 3e38f);
}

// ---------------------------------------------------------------------------
// SR-AUD-033 parse tail (#1865, approved 2026-07-31, CCF-007 item CCF7-6).
// Single mirrors Double exactly -- same shared System::detail grammar helpers,
// same .NET NumberStyles.Float | AllowThousands contract -- so the same matrix
// is pinned here rather than assumed to follow.
// ---------------------------------------------------------------------------

TEST(SingleTest, Parse_GroupSeparators_Accepted) {
    EXPECT_FLOAT_EQ(Single::Parse("1,234.5"), 1234.5f);
    EXPECT_FLOAT_EQ(Single::Parse("1,234,567"), 1234567.0f);
    EXPECT_FLOAT_EQ(Single::Parse("-1,234.5"), -1234.5f);
    EXPECT_FLOAT_EQ(Single::Parse(" 1,234.5 "), 1234.5f);
    float r = 0;
    EXPECT_TRUE(Single::TryParse("1,234.5", r));
    EXPECT_FLOAT_EQ(r, 1234.5f);
}

TEST(SingleTest, Parse_GroupSizesAreNotValidated_MatchingDotNet) {
    EXPECT_FLOAT_EQ(Single::Parse("1,2,3"), 123.0f);
    EXPECT_FLOAT_EQ(Single::Parse("1,,2"), 12.0f);
    EXPECT_FLOAT_EQ(Single::Parse("1,"), 1.0f);
}

TEST(SingleTest, Parse_GroupSeparatorInAnIllegalPosition_StillRejected) {
    float r = 0;
    EXPECT_FALSE(Single::TryParse(",5", r));
    EXPECT_FALSE(Single::TryParse("1.5,", r));
    EXPECT_FALSE(Single::TryParse("1.2,3", r));
    EXPECT_FALSE(Single::TryParse("1e2,3", r));
    EXPECT_THROW(Single::Parse(",5"), System::FormatException);
}

TEST(SingleTest, Parse_MagnitudeOverflow_SaturatesToInfinity) {
    EXPECT_TRUE(std::isinf(Single::Parse("1e999")) && Single::Parse("1e999") > 0);
    EXPECT_TRUE(std::isinf(Single::Parse("-1e999")) && Single::Parse("-1e999") < 0);
    // Past float's range but well inside double's -- the limit is per type.
    EXPECT_TRUE(std::isinf(Single::Parse("3.5e38")));
    float r = 0;
    EXPECT_TRUE(Single::TryParse("1e999", r));
    EXPECT_TRUE(std::isinf(r) && r > 0);
}

TEST(SingleTest, Parse_MagnitudeUnderflow_CollapsesToSignedZero) {
    EXPECT_FLOAT_EQ(Single::Parse("1e-999"), 0.0f);
    EXPECT_FALSE(std::signbit(Single::Parse("1e-999")));
    EXPECT_TRUE(std::signbit(Single::Parse("-1e-999")));
}

TEST(SingleTest, Parse_RepresentableValues_Unchanged) {
    EXPECT_FLOAT_EQ(Single::Parse("1.5"), 1.5f);
    EXPECT_FLOAT_EQ(Single::Parse("3.4e38"), 3.4e38f);
    EXPECT_TRUE(std::isnan(Single::Parse("NaN")));
    float r = 0;
    EXPECT_FALSE(Single::TryParse("inf", r));  // C spelling still rejected
}

// Half delegates its parse to Single, so it inherits the widened grammar with
// no edit of its own -- pinned so a future Half rewrite cannot silently drop it.
TEST(SingleTest, HalfInheritsTheWidenedParseGrammarFromSingle) {
    System::Half h;
    EXPECT_TRUE(System::Half::TryParse("1,024", h));
    EXPECT_FLOAT_EQ(h.ToSingle(), 1024.0f);
    EXPECT_TRUE(System::Half::TryParse("1e999", h));
    EXPECT_TRUE(std::isinf(h.ToSingle()));
}

// ---------------------------------------------------------------------------
// SR-AUD-033 format slice (#1863, approved 2026-07-31, CCF-007 item CCF7-5).
// Single shares the emitted-text helpers with Double, so the same matrix is
// pinned here rather than assumed to follow. Half delegates its ToString to
// Single and inherits the change with no edit of its own.
// ---------------------------------------------------------------------------

TEST(SingleTest, Ccf7_5_ExponentIsSignedAndAtLeastThreeDigits) {
    EXPECT_EQ(Single::ToString(1.25f, "E2"), "1.25E+000");
    EXPECT_EQ(Single::ToString(1.25f, "e2"), "1.25e+000");
    EXPECT_EQ(Single::ToString(-1.25f, "E2"), "-1.25E+000");
    EXPECT_EQ(Single::ToString(1e30f, "E2"), "1.00E+030");
}

TEST(SingleTest, Ccf7_5_NumberFormatGroupsTheIntegerDigits) {
    EXPECT_EQ(Single::ToString(1234.5f, "N2"), "1,234.50");
    EXPECT_EQ(Single::ToString(-1234.5f, "N2"), "-1,234.50");
    EXPECT_EQ(Single::ToString(123.0f, "N2"), "123.00");
}

TEST(SingleTest, Ccf7_5_GeneralFormatIsRoundTripCapable) {
    EXPECT_EQ(Single::ToString(0.1f, "G9"), "0.100000001");   // float's round-trip width
    for (float v : {0.1f, 1.0f / 3.0f, 1e-30f, 1e30f, 12345.6789f, 1.5f, 3.14159265f}) {
        float back = 0;
        ASSERT_TRUE(Single::TryParse(Single::ToString(v, "G9"), back)) << v;
        EXPECT_EQ(back, v);
        ASSERT_TRUE(Single::TryParse(Single::ToString(v, "G"), back)) << v;
        EXPECT_EQ(back, v);
    }
}

TEST(SingleTest, Ccf7_5_TheRoundTripFastPathDidNotMove) {
    EXPECT_EQ(Single::ToString(0.1f, "R"), Single::ToString(0.1f));
    EXPECT_EQ(Single::ToString(0.1f, "R"), "0.1");
    EXPECT_EQ(Single::ToString(2.5f, "R"), Single::ToString(2.5f));
}

TEST(SingleTest, Ccf7_5_HalfInheritsTheEmittedTextByDelegation) {
    EXPECT_EQ(System::Half::FromSingle(1234.5f).ToString("N2"), "1,234.00");
    EXPECT_EQ(System::Half::FromSingle(1.25f).ToString("E2"), "1.25E+000");
}

TEST(SingleTest, Ccf7_5_EverythingElseIsUnchanged) {
    EXPECT_EQ(Single::ToString(3.14159f, "F2"), "3.14");
    EXPECT_EQ(Single::ToString(1234.5f, "F2"), "1234.50");   // F does NOT group
    EXPECT_EQ(Single::ToString(Single::NaN, "N2"), "NaN");
    EXPECT_THROW(Single::ToString(1.0f, "Q"), System::FormatException);
}
