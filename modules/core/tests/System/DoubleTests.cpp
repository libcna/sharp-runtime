// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <cmath>
#include <limits>
#include <string>
#include "System/Double.hpp"
#include "System/ArgumentException.hpp"
#include "System/ArithmeticException.hpp"
#include "System/FormatException.hpp"

using System::Double;
using System::ArithmeticException;

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

TEST(DoubleTests, Epsilon_MatchesDotNetValue) {
    // .NET Double.Epsilon = 4.9406564584124654E-324 (smallest denormal, not the smallest normal value).
    EXPECT_EQ(Double::Epsilon, std::numeric_limits<double>::denorm_min());
    EXPECT_LT(Double::Epsilon, std::numeric_limits<double>::min());
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

// SR-AUD-030 (#1860, CCF-007): subnormal powers of two must be recognised (one
// trailing-significand bit), matching .NET Double.IsPow2; the normal rule rejected them.
TEST(DoubleTests, IsPow2_SubnormalPowersOfTwo) {
    EXPECT_TRUE(Double::IsPow2(Double::Epsilon));           // smallest subnormal = 2^-1074
    EXPECT_TRUE(Double::IsPow2(Double::Epsilon * 2.0));     // 2^-1073, still one bit
    EXPECT_TRUE(Double::IsPow2(Double::Epsilon * 4.0));     // 2^-1072
    EXPECT_FALSE(Double::IsPow2(Double::Epsilon * 3.0));    // two bits -> not a power of two
    EXPECT_FALSE(Double::IsPow2(-Double::Epsilon));         // negative subnormal
    EXPECT_TRUE(Double::IsPow2(1.0));                       // normal power of two unaffected
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
    EXPECT_THROW(Double::Sign(Double::NaN), ArithmeticException);
}

TEST(DoubleTests, Clamp_MinGreaterThanMax_Throws) {
    EXPECT_THROW(Double::Clamp(1.0, 10.0, 0.0), System::ArgumentException);
}

TEST(DoubleTests, Max_PropagatesNaN) {
    EXPECT_TRUE(Double::IsNaN(Double::Max(Double::NaN, 1.0)));
    EXPECT_TRUE(Double::IsNaN(Double::Max(1.0, Double::NaN)));
}

TEST(DoubleTests, Min_PropagatesNaN) {
    EXPECT_TRUE(Double::IsNaN(Double::Min(Double::NaN, 1.0)));
    EXPECT_TRUE(Double::IsNaN(Double::Min(1.0, Double::NaN)));
}

TEST(DoubleTests, MaxMagnitude_TieBreaksToPositive) {
    EXPECT_FALSE(Double::IsNegative(Double::MaxMagnitude(-0.0, 0.0)));
}

TEST(DoubleTests, MinMagnitude_TieBreaksToNegative) {
    EXPECT_TRUE(Double::IsNegative(Double::MinMagnitude(0.0, -0.0)));
}

TEST(DoubleTests, RootN_NegativeBase_OddRoot) {
    EXPECT_NEAR(Double::RootN(-8.0, 3), -2.0, 0.0001);
}

TEST(DoubleTests, RootN_NegativeBase_EvenRoot_NaN) {
    EXPECT_TRUE(Double::IsNaN(Double::RootN(-8.0, 4)));
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
    EXPECT_THROW(Double::Parse("abc"), System::FormatException);
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

// Regression tests for a code-audit finding (ticket 245): TryParse/Parse only special-cased
// the exact-cased strings "NaN"/"Infinity"/"-Infinity" and fell through to std::from_chars for
// everything else. Per the C++ standard, from_chars's floating-point grammar recognizes
// "inf"/"infinity"/"nan"/"nan(n-char-seq)" case-insensitively regardless of chars_format, so
// abbreviated/miscased spellings like "inf" or "nan(123)" were silently ACCEPTED here even
// though real .NET's double.Parse (verified against Number.Parsing.cs's TryParseFloat) throws
// FormatException for all of them -- it only recognizes (case-insensitively) the exact tokens
// "Infinity"/"+Infinity"/"-Infinity" and "NaN"/"+NaN"/"-NaN".
TEST(DoubleTests, TryParse_AbbreviatedInfinitySpelling_ReturnsFalse) {
    double r;
    EXPECT_FALSE(Double::TryParse("inf", r));
    EXPECT_FALSE(Double::TryParse("-inf", r));
    EXPECT_FALSE(Double::TryParse("INF", r));
}

TEST(DoubleTests, TryParse_NanWithCharSequence_ReturnsFalse) {
    double r;
    EXPECT_FALSE(Double::TryParse("nan(123)", r));
}

TEST(DoubleTests, TryParse_CaseInsensitiveCanonicalSpellings_ReturnsTrue) {
    double r;
    EXPECT_TRUE(Double::TryParse("infinity", r));
    EXPECT_TRUE(std::isinf(r) && r > 0);
    EXPECT_TRUE(Double::TryParse("nan", r));
    EXPECT_TRUE(std::isnan(r));
    EXPECT_TRUE(Double::TryParse("+Infinity", r));
    EXPECT_TRUE(std::isinf(r) && r > 0);
}

TEST(DoubleTests, Parse_AbbreviatedInfinitySpelling_Throws) {
    EXPECT_THROW(Double::Parse("inf"), System::FormatException);
    EXPECT_THROW(Double::Parse("nan(123)"), System::FormatException);
}

// SR-AUD-033 whitespace slice (#1864, CCF-007): .NET's default NumberStyles.Float
// permits leading/trailing whitespace (AllowLeadingWhite | AllowTrailingWhite),
// but never interior whitespace or an empty/all-whitespace string.
TEST(DoubleTests, Parse_LeadingTrailingWhitespace_Accepted) {
    EXPECT_EQ(Double::Parse(" 1.5 "), 1.5);
    EXPECT_NEAR(Double::Parse("\t3.14\n"), 3.14, 1e-12);
    EXPECT_EQ(Double::Parse("  -2.5"), -2.5);
    EXPECT_EQ(Double::Parse("42\r\n"), 42.0);
}

TEST(DoubleTests, Parse_WhitespaceAroundSpecialTokens_Accepted) {
    EXPECT_TRUE(std::isnan(Double::Parse("  NaN  ")));
    double r = 0;
    EXPECT_TRUE(Double::TryParse(" Infinity ", r));
    EXPECT_TRUE(std::isinf(r) && r > 0);
    EXPECT_TRUE(Double::TryParse("\t-Infinity\t", r));
    EXPECT_TRUE(std::isinf(r) && r < 0);
}

TEST(DoubleTests, Parse_InteriorWhitespace_StillRejected) {
    double r = 0;
    EXPECT_FALSE(Double::TryParse("1 5", r));
    EXPECT_FALSE(Double::TryParse("1. 5", r));
    EXPECT_FALSE(Double::TryParse("N aN", r));
    EXPECT_THROW(Double::Parse("1 5"), System::FormatException);
}

TEST(DoubleTests, Parse_EmptyOrAllWhitespace_StillRejected) {
    double r = 0;
    EXPECT_FALSE(Double::TryParse("", r));
    EXPECT_FALSE(Double::TryParse("   ", r));
    EXPECT_FALSE(Double::TryParse("\t\n", r));
    EXPECT_THROW(Double::Parse("   "), System::FormatException);
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

// SR-AUD-021 float slice (#1849 / CCF-006): a malformed precision no longer leaks a
// std::stoi exception, and an unrecognised specifier is rejected loudly instead of
// silently round-tripping. Matches the integer wrappers (#1847) and .NET.
// SAMPLE-028 correction: "Fz" is NOT a malformed standard specifier to .NET. A standard
// numeric format string is one letter plus an optional integer precision; "Fz" fails that
// shape, so .NET reads it as a CUSTOM format string in which both characters are literals
// and returns "Fz". Measured directly against the reference implementation rather than
// assumed. A single unrecognised letter does still throw.
TEST(DoubleTests, ToString_MalformedPrecisionIsACustomFormatOfLiterals) {
    EXPECT_EQ(Double::ToString(1.0, "Fz"), "Fz");
}
TEST(DoubleTests, ToString_OversizedPrecision_ThrowsFormatException) {
    EXPECT_THROW(Double::ToString(1.0, "F99999999999"), System::FormatException);
}
TEST(DoubleTests, ToString_UnknownSpecifier_ThrowsFormatException) {
    EXPECT_THROW(Double::ToString(1.0, "Q"), System::FormatException);
}
TEST(DoubleTests, ToString_UnknownSpecifierWithDigits_ThrowsFormatException) {
    EXPECT_THROW(Double::ToString(1.0, "Z2"), System::FormatException);
}
TEST(DoubleTests, ToString_PercentSpecifier_ThrowsFormatException) {
    EXPECT_THROW(Double::ToString(1.0, "P2"), System::FormatException);  // P is unimplemented here
}
TEST(DoubleTests, ToString_ValidSpecifiers_StillWork) {
    EXPECT_EQ(Double::ToString(3.14159, "F2"), "3.14");
    EXPECT_FALSE(Double::ToString(1000.0, "E2").empty());
    EXPECT_FALSE(Double::ToString(3.14159, "G").empty());
    EXPECT_EQ(Double::ToString(1.5, "R"), Double::ToString(1.5));
    EXPECT_FALSE(Double::ToString(1.5, "N2").empty());
}

// ---------------------------------------------------------------------------
// SR-AUD-033 parse tail (#1865, approved 2026-07-31, CCF-007 item CCF7-6):
// the grammar Double::Parse accepts is NumberStyles.Float | AllowThousands,
// exactly as .NET's Double.Parse(string) declares it (Double.cs:388).
//
// Two pieces have no std::from_chars equivalent and are supplied by
// System/detail/FloatParseGrammar.hpp: group separators, and the direction of an
// out-of-range magnitude (from_chars uses one errc for overflow and underflow
// and leaves the output unwritten).
// ---------------------------------------------------------------------------

TEST(DoubleTests, Parse_GroupSeparators_Accepted) {
    EXPECT_EQ(Double::Parse("1,234.5"), 1234.5);
    EXPECT_EQ(Double::Parse("1,234,567"), 1234567.0);
    EXPECT_EQ(Double::Parse("-1,234.5"), -1234.5);
    EXPECT_EQ(Double::Parse(" 1,234.5 "), 1234.5);
    EXPECT_EQ(Double::Parse("1,234e2"), 123400.0);
    double r = 0;
    EXPECT_TRUE(Double::TryParse("1,234.5", r));
    EXPECT_EQ(r, 1234.5);
}

TEST(DoubleTests, Parse_GroupSizesAreNotValidated_MatchingDotNet) {
    // .NET's scanner never checks group SIZES -- group sizes are a formatting
    // concept -- so these parse, and the port must not be stricter.
    EXPECT_EQ(Double::Parse("1,2,3"), 123.0);
    EXPECT_EQ(Double::Parse("1,,2"), 12.0);
    EXPECT_EQ(Double::Parse("1,"), 1.0);
}

TEST(DoubleTests, Parse_GroupSeparatorInAnIllegalPosition_StillRejected) {
    // .NET accepts a group separator only after a digit and before the decimal
    // separator; anywhere else its scan stops and the parse fails.
    double r = 0;
    EXPECT_FALSE(Double::TryParse(",5", r));
    EXPECT_FALSE(Double::TryParse(",", r));
    EXPECT_FALSE(Double::TryParse("1.5,", r));
    EXPECT_FALSE(Double::TryParse("1.2,3", r));
    EXPECT_FALSE(Double::TryParse("1e2,3", r));
    EXPECT_FALSE(Double::TryParse("-,5", r));
    EXPECT_THROW(Double::Parse(",5"), System::FormatException);
}

TEST(DoubleTests, Parse_MagnitudeOverflow_SaturatesToInfinity) {
    // Number.NumberToFloat: a scale past MaxDecimalExponent yields
    // PositiveInfinity and the sign is applied afterwards. Not a FormatException.
    EXPECT_TRUE(std::isinf(Double::Parse("1e999")));
    EXPECT_GT(Double::Parse("1e999"), 0.0);
    EXPECT_TRUE(std::isinf(Double::Parse("-1e999")));
    EXPECT_LT(Double::Parse("-1e999"), 0.0);
    EXPECT_TRUE(std::isinf(Double::Parse("1e99999999999999999999")));
    double r = 0;
    EXPECT_TRUE(Double::TryParse("1e999", r));
    EXPECT_TRUE(std::isinf(r) && r > 0);
}

TEST(DoubleTests, Parse_MagnitudeUnderflow_CollapsesToSignedZero) {
    // The same from_chars errc covers the other direction, and .NET's
    // NumberToFloat yields Zero for a scale below MinDecimalExponent, sign
    // applied afterwards -- so "-1e-999" is negative zero, not a failure.
    EXPECT_EQ(Double::Parse("1e-999"), 0.0);
    EXPECT_FALSE(std::signbit(Double::Parse("1e-999")));
    EXPECT_EQ(Double::Parse("-1e-999"), 0.0);
    EXPECT_TRUE(std::signbit(Double::Parse("-1e-999")));
}

TEST(DoubleTests, Parse_RepresentableBoundaries_Unchanged) {
    EXPECT_EQ(Double::Parse("1e308"), 1e308);
    EXPECT_EQ(Double::Parse("1e-320"), 1e-320);   // subnormal, from_chars succeeds
    EXPECT_EQ(Double::Parse("0e999"), 0.0);       // zero significand is never out of range
    EXPECT_EQ(Double::Parse("1.5"), 1.5);
    EXPECT_EQ(Double::Parse("1e2"), 100.0);
    EXPECT_TRUE(std::signbit(Double::Parse("-0")));
}

TEST(DoubleTests, Parse_CSpellingOfInfinity_StillRejected) {
    // from_chars's general format also accepts the C spellings "inf"/"infinity";
    // .NET's NaNSymbol/PositiveInfinitySymbol are "NaN"/"Infinity", so "inf" is
    // not a .NET token and its rejection (the !isfinite guard on the success
    // path) is unchanged by #1865. The FULL words are accepted case-insensitively
    // in both -- .NET compares against NaNSymbol/PositiveInfinitySymbol with
    // SpanEqualsOrdinalIgnoreCase -- so "nan" and "infinity" are valid there too.
    double r = 0;
    EXPECT_FALSE(Double::TryParse("inf", r));
    EXPECT_TRUE(Double::TryParse("nan", r));
    EXPECT_TRUE(std::isnan(r));
    EXPECT_TRUE(Double::TryParse("infinity", r));   // case-insensitive, as .NET
    EXPECT_TRUE(std::isinf(r));
    EXPECT_TRUE(Double::TryParse("Infinity", r));
    EXPECT_TRUE(std::isinf(r));
}

// ---------------------------------------------------------------------------
// SR-AUD-033 format slice (#1863, approved 2026-07-31, CCF-007 item CCF7-5):
// the text ToString(value, format) emits.
//
// The std::ostringstream back end cannot emit a three-digit exponent, cannot
// insert group separators, and does not give a shortest round-trippable G. The
// first two are now post-processed (System/detail/FloatTextFormat.hpp) and the
// third is replaced by std::to_chars. The default/R fast path is the one thing
// that must NOT move, and is pinned below.
// ---------------------------------------------------------------------------

TEST(DoubleTests, Ccf7_5_ExponentIsSignedAndAtLeastThreeDigits) {
    EXPECT_EQ(Double::ToString(1.25, "E2"), "1.25E+000");
    EXPECT_EQ(Double::ToString(1.25, "e2"), "1.25e+000");
    EXPECT_EQ(Double::ToString(1.25, "E"), "1.250000E+000");   // default precision 6
    EXPECT_EQ(Double::ToString(0.000125, "E2"), "1.25E-004");
    EXPECT_EQ(Double::ToString(-1.25, "E2"), "-1.25E+000");
    EXPECT_EQ(Double::ToString(0.0, "E2"), "0.00E+000");
    // An exponent already three digits or wider is untouched.
    EXPECT_EQ(Double::ToString(1e100, "E2"), "1.00E+100");
    EXPECT_EQ(Double::ToString(1e-100, "E2"), "1.00E-100");
    EXPECT_EQ(Double::ToString(1e300, "E4"), "1.0000E+300");
}

TEST(DoubleTests, Ccf7_5_NumberFormatGroupsTheIntegerDigits) {
    EXPECT_EQ(Double::ToString(1234.5, "N2"), "1,234.50");
    EXPECT_EQ(Double::ToString(1234.5, "N"), "1,234.50");      // default two decimals
    EXPECT_EQ(Double::ToString(1234567.891, "N2"), "1,234,567.89");
    EXPECT_EQ(Double::ToString(-1234567.891, "N2"), "-1,234,567.89");
    EXPECT_EQ(Double::ToString(1000000.0, "N2"), "1,000,000.00");
    EXPECT_EQ(Double::ToString(1234.5, "N0"), "1,234");
    // Three integer digits or fewer need no separator at all.
    EXPECT_EQ(Double::ToString(123.0, "N2"), "123.00");
    EXPECT_EQ(Double::ToString(0.5, "N2"), "0.50");
    EXPECT_EQ(Double::ToString(-0.25, "N2"), "-0.25");
}

TEST(DoubleTests, Ccf7_5_GeneralFormatIsRoundTripCapable) {
    // G with no precision is the SHORTEST round-trippable text -- it used to be
    // std::ostringstream's default six significant digits, which is neither.
    EXPECT_EQ(Double::ToString(1.0 / 3.0, "G"), "0.3333333333333333");
    EXPECT_EQ(Double::ToString(1.5, "G"), "1.5");
    // G17 is double's round-trip width.
    EXPECT_EQ(Double::ToString(0.1, "G17"), "0.10000000000000001");
    for (double v : {0.1, 1.0 / 3.0, 1e-300, 1e300, 123456789.123456789,
                     2.2250738585072014e-308, 1.5, 3.141592653589793}) {
        double back = 0;
        ASSERT_TRUE(Double::TryParse(Double::ToString(v, "G17"), back)) << v;
        EXPECT_EQ(back, v);
        ASSERT_TRUE(Double::TryParse(Double::ToString(v, "G"), back)) << v;
        EXPECT_EQ(back, v);
    }
}

TEST(DoubleTests, Ccf7_5_TheRoundTripFastPathDidNotMove) {
    // The one non-negotiable constraint of the approval: R and the no-format
    // overload already produced shortest round-trip text through std::to_chars,
    // and must keep producing exactly the same text.
    EXPECT_EQ(Double::ToString(1.5, "R"), Double::ToString(1.5));
    EXPECT_EQ(Double::ToString(1.5, "R"), "1.5");
    EXPECT_EQ(Double::ToString(0.1, "R"), Double::ToString(0.1));
    EXPECT_EQ(Double::ToString(0.1, "R"), "0.1");
    EXPECT_EQ(Double::ToString(1e100, "R"), "1e+100");
    for (double v : {0.1, 1.0 / 3.0, 1e-300, 1e300, 1.5}) {
        double back = 0;
        ASSERT_TRUE(Double::TryParse(Double::ToString(v, "R"), back)) << v;
        EXPECT_EQ(back, v);
    }
}

TEST(DoubleTests, Ccf7_5_EverythingElseIsUnchanged) {
    EXPECT_EQ(Double::ToString(3.14159, "F2"), "3.14");
    EXPECT_EQ(Double::ToString(1234.5, "F2"), "1234.50");   // F does NOT group
    EXPECT_EQ(Double::ToString(1.0, ""), "1");
    EXPECT_EQ(Double::ToString(Double::NaN, "N2"), "NaN");
    EXPECT_EQ(Double::ToString(Double::PositiveInfinity, "E2"), "Infinity");
    EXPECT_EQ(Double::ToString(Double::NegativeInfinity, "N2"), "-Infinity");
    EXPECT_THROW(Double::ToString(1.0, "Q"), System::FormatException);
    EXPECT_THROW(Double::ToString(1.0, "F99999999999"), System::FormatException);
}

TEST(DoubleTests, CustomFormat_SharesSingleImplementationAndCannotDrift) {
    // Both types format through the same System::detail helpers, so this pins the double
    // side of that shared grammar.
    EXPECT_EQ(System::Double::ToString(3.14159, "0.00"), "3.14");
    EXPECT_EQ(System::Double::ToString(0.5, "0"), "1");
    EXPECT_EQ(System::Double::ToString(-1234.5, "#,##0.0"), "-1,234.5");
    EXPECT_EQ(System::Double::ToString(12.0, "F2"), "12.00");
}
