// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <string>

#include "System/ArgumentException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Decimal.hpp"
#include "System/FormatException.hpp"
#include "System/MidpointRounding.hpp"
#include "System/OverflowException.hpp"

using System::Decimal;
using System::MidpointRounding;
using SharpRuntime::intcs;

// Suite name uses a Tests2 suffix per NEXT.md's documented duplicate-suite-name
// policy: DecimalTests already exists in DecimalTests.cpp / DecimalNewTests.cpp.

// ---------------------------------------------------------------------------
// New constructors
// ---------------------------------------------------------------------------

TEST(DecimalTests2, CtorFromUnsignedInt) {
    Decimal d(SharpRuntime::uintcs(42));
    EXPECT_EQ(d, Decimal(42));
    EXPECT_FALSE(d.getIsNegativeProperty());
}

TEST(DecimalTests2, CtorFromUnsignedLong) {
    Decimal d(SharpRuntime::ulongcs(123456789012345ULL));
    EXPECT_EQ(d.ToUInt64(), 123456789012345ULL);
}

TEST(DecimalTests2, CtorFromFloat) {
    Decimal d(1.5f);
    EXPECT_NEAR(d.ToDouble(), 1.5, 1e-6);
}

TEST(DecimalTests2, CtorFromBits_RoundTrips) {
    Decimal original = Decimal::Parse("12345.6789");
    intcs lo, mid, hi, flags;
    Decimal::GetBits(original, lo, mid, hi, flags);
    Decimal rebuilt(lo, mid, hi, flags < 0, SharpRuntime::bytecs((uint32_t(flags) >> 16) & 0xFF));
    EXPECT_EQ(rebuilt, original);
    EXPECT_EQ(rebuilt.getScaleProperty(), original.getScaleProperty());
}

TEST(DecimalTests2, CtorFromBits_Negative) {
    Decimal d(5, 0, 0, true, 1); // -0.5
    EXPECT_TRUE(d.getIsNegativeProperty());
    EXPECT_EQ(d, Decimal::Parse("-0.5"));
}

TEST(DecimalTests2, CtorFromBits_ScaleTooLarge_Throws) {
    EXPECT_THROW(Decimal(1, 0, 0, false, 29), System::ArgumentOutOfRangeException);
}

// ---------------------------------------------------------------------------
// GetBits
// ---------------------------------------------------------------------------

TEST(DecimalTests2, GetBits_Zero) {
    intcs lo, mid, hi, flags;
    Decimal::GetBits(Decimal::Zero, lo, mid, hi, flags);
    EXPECT_EQ(lo, 0);
    EXPECT_EQ(mid, 0);
    EXPECT_EQ(hi, 0);
    EXPECT_EQ(flags, 0);
}

TEST(DecimalTests2, GetBits_ScaleAndSignEncoded) {
    Decimal d = Decimal::Parse("-1.23");
    intcs lo, mid, hi, flags;
    Decimal::GetBits(d, lo, mid, hi, flags);
    EXPECT_EQ(lo, 123);
    EXPECT_EQ(mid, 0);
    EXPECT_EQ(hi, 0);
    EXPECT_EQ((uint32_t(flags) >> 16) & 0xFF, 2u); // scale = 2
    EXPECT_NE(uint32_t(flags) & 0x80000000u, 0u);  // sign bit set
}

// ---------------------------------------------------------------------------
// SR-AUD-038 (#1856): negative zero is a distinct, observable representation.
// The raw ctor, CopySign, and the parser preserve the sign of a zero magnitude
// (matching .NET's Decimal(int,int,int,bool,byte)/CopySign/parser), yet -0 still
// compares and hashes equal to +0.
// ---------------------------------------------------------------------------

TEST(DecimalTests2, RawCtor_NegativeZero_SignObservable) {
    Decimal negZero(0, 0, 0, /*isNegative=*/true, 0);
    intcs lo, mid, hi, flags;
    Decimal::GetBits(negZero, lo, mid, hi, flags);
    EXPECT_EQ(lo, 0);
    EXPECT_EQ(mid, 0);
    EXPECT_EQ(hi, 0);
    EXPECT_EQ(uint32_t(flags), 0x80000000u);   // sign bit set, scale 0
    EXPECT_TRUE(negZero.getIsNegativeProperty());
    EXPECT_TRUE(Decimal::IsNegative(negZero));
    EXPECT_FALSE(Decimal::IsPositive(negZero));
}

TEST(DecimalTests2, RawCtor_NegativeZero_WithScale) {
    Decimal negZero(0, 0, 0, /*isNegative=*/true, 4);
    intcs lo, mid, hi, flags;
    Decimal::GetBits(negZero, lo, mid, hi, flags);
    EXPECT_EQ(uint32_t(flags), 0x80000000u | (4u << 16));  // sign + scale 4
    EXPECT_TRUE(negZero.getIsNegativeProperty());
}

TEST(DecimalTests2, PositiveZero_HasNoSignBit) {
    intcs lo, mid, hi, flags;
    Decimal::GetBits(Decimal::Zero, lo, mid, hi, flags);
    EXPECT_EQ(uint32_t(flags), 0u);
    EXPECT_FALSE(Decimal::Zero.getIsNegativeProperty());
    // Default-constructed and explicitly-built positive zero both stay unsigned.
    EXPECT_FALSE(Decimal(0, 0, 0, false, 0).getIsNegativeProperty());
}

TEST(DecimalTests2, CopySign_ProducesNegativeZero) {
    Decimal r = Decimal::CopySign(Decimal(0), Decimal(-1));
    EXPECT_TRUE(r.getIsNegativeProperty());
    EXPECT_TRUE(Decimal::IsNegative(r));
    intcs lo, mid, hi, flags;
    Decimal::GetBits(r, lo, mid, hi, flags);
    EXPECT_EQ(uint32_t(flags) & 0x80000000u, 0x80000000u);
    // CopySign(0, +1) stays positive zero.
    Decimal p = Decimal::CopySign(Decimal(0), Decimal(1));
    EXPECT_FALSE(p.getIsNegativeProperty());
}

TEST(DecimalTests2, NegativeZero_ComparesAndHashesEqualToPositiveZero) {
    Decimal negZero(0, 0, 0, true, 0);
    Decimal posZero(0, 0, 0, false, 0);
    EXPECT_TRUE(negZero == posZero);
    EXPECT_TRUE(posZero == negZero);
    EXPECT_FALSE(negZero < posZero);
    EXPECT_FALSE(posZero < negZero);
    EXPECT_EQ(negZero.CompareTo(posZero), 0);
    EXPECT_EQ(negZero.GetHashCode(), posZero.GetHashCode());
    EXPECT_EQ(negZero.GetHashCode(), Decimal::Zero.GetHashCode());
}

TEST(DecimalTests2, Parse_NegativeZero_PreservesSign) {
    // .NET's decimal parser produces a negative zero for "-0" (the Decimal-kind
    // buffer does not clear IsNegative for a zero value).
    Decimal d = Decimal::Parse("-0");
    EXPECT_TRUE(d.getIsNegativeProperty());
    intcs lo, mid, hi, flags;
    Decimal::GetBits(d, lo, mid, hi, flags);
    EXPECT_EQ(uint32_t(flags) & 0x80000000u, 0x80000000u);
    EXPECT_TRUE(d == Decimal::Zero);   // still equal to +0

    Decimal d2 = Decimal::Parse("-0.00");
    EXPECT_TRUE(d2.getIsNegativeProperty());

    // A plain "0" and "+0" remain positive zero.
    EXPECT_FALSE(Decimal::Parse("0").getIsNegativeProperty());
    EXPECT_FALSE(Decimal::Parse("+0").getIsNegativeProperty());
}

// ---------------------------------------------------------------------------
// SR-AUD-035 (#1857): parser — compatible sub-defects landed.
//   * leading/trailing whitespace is now skipped (pure widening);
//   * excess fractional precision beyond scale 28 is rounded half-to-even
//     instead of being silently discarded.
// The comma-as-group-separator value change and the FormatException ->
// OverflowException exception-type change stay BLOCKED on approval (#1858);
// the two documentation tests below pin the CURRENT (pre-#1858) behaviour.
// ---------------------------------------------------------------------------

TEST(DecimalTests2, Parse_SkipsLeadingAndTrailingWhitespace) {
    EXPECT_EQ(Decimal::Parse(" 3.14 "), Decimal::Parse("3.14"));
    EXPECT_EQ(Decimal::Parse("\t42\n"), Decimal(42));
    EXPECT_EQ(Decimal::Parse("  -7  "), Decimal(-7));
    Decimal d;
    EXPECT_TRUE(Decimal::TryParse("\r\n 100 \t", d));
    EXPECT_EQ(d, Decimal(100));
    // All-whitespace / empty still fail.
    EXPECT_FALSE(Decimal::TryParse("   ", d));
    EXPECT_FALSE(Decimal::TryParse("", d));
    // Interior whitespace is still rejected (only leading/trailing is allowed).
    EXPECT_FALSE(Decimal::TryParse("1 2", d));
}

TEST(DecimalTests2, Parse_RoundsExcessFractionalPrecisionHalfToEven) {
    // Build fractional strings with exact digit counts (scale caps at 28, so the 29th
    // fractional digit is the first dropped one and drives the rounding).
    const std::string z27(27, '0');
    const std::string z28(28, '0');
    // The audit's exact case: 6e-29 must round up to 1e-28, not truncate to 0.
    EXPECT_EQ(Decimal::Parse("0." + z28 + "6"), Decimal::Parse("0." + z27 + "1"));
    EXPECT_EQ(Decimal::Parse("0." + z28 + "6").getScaleProperty(), 28);
    // A dropped digit < 5 rounds down to zero.
    EXPECT_EQ(Decimal::Parse("0." + z28 + "4"), Decimal::Zero);
    // Tie (dropped 5, empty tail): retained mantissa ...2 is even -> stays ...2.
    EXPECT_EQ(Decimal::Parse("0." + z27 + "25"), Decimal::Parse("0." + z27 + "2"));
    // Tie (dropped 5, empty tail): retained mantissa ...3 is odd -> rounds to even ...4.
    EXPECT_EQ(Decimal::Parse("0." + z27 + "35"), Decimal::Parse("0." + z27 + "4"));
    // A nonzero tail after the dropped 5 always rounds up, even from an even mantissa.
    EXPECT_EQ(Decimal::Parse("0." + z27 + "251"), Decimal::Parse("0." + z27 + "3"));
    // A long in-range integer part with trailing excess precision still rounds.
    EXPECT_EQ(Decimal::Parse("1." + z27 + "55"), Decimal::Parse("1." + z27 + "6"));
}

// DOCUMENTATION (pre-#1858): ',' is still interpreted as a decimal point, NOT a .NET
// group separator. #1858 (approval-blocked) would make Parse("1,5") == 15 instead.
TEST(DecimalTests2, Parse_CommaIsStillDecimalPoint_PendingApproval) {
    EXPECT_EQ(Decimal::Parse("1,5"), Decimal::Parse("1.5"));
    // Two separators are rejected, so ".NET grouped" input like "1,234.5" still fails.
    Decimal d;
    EXPECT_FALSE(Decimal::TryParse("1,234.5", d));
}

// INVERTED by #1858 (approved 2026-07-31), which is exactly what the decision
// packet §B.7 required of this test: a value past decimal's range is now .NET's
// OverflowException, not a FormatException. The original pre-approval assertion
// is preserved in the git history of this file and in
// docs/DecimalBoundaryFamilyPlan.md §12.
TEST(DecimalTests2, Parse_OverflowIsAnOverflowException) {
    EXPECT_THROW(Decimal::Parse("79228162514264337593543950336"),
                 System::OverflowException);
    Decimal d;
    EXPECT_FALSE(Decimal::TryParse("79228162514264337593543950336", d));
    // A well-formed in-range boundary still parses.
    EXPECT_EQ(Decimal::Parse("79228162514264337593543950335"), Decimal::MaxValue);
}

// ---------------------------------------------------------------------------
// Static To* overloads (mirroring .NET's static method surface)
// ---------------------------------------------------------------------------

TEST(DecimalTests2, StaticToInt32_MatchesInstance) {
    Decimal d(42);
    EXPECT_EQ(Decimal::ToInt32(d), d.ToInt32());
}

TEST(DecimalTests2, ToByte_Valid) {
    EXPECT_EQ(Decimal::ToByte(Decimal(200)), 200);
}

TEST(DecimalTests2, ToByte_OutOfRange_Throws) {
    EXPECT_THROW(Decimal::ToByte(Decimal(-1)), System::OverflowException);
    EXPECT_THROW(Decimal::ToByte(Decimal(256)), System::OverflowException);
}

TEST(DecimalTests2, ToSByte_Valid) {
    EXPECT_EQ(Decimal::ToSByte(Decimal(-100)), -100);
}

TEST(DecimalTests2, ToSByte_OutOfRange_Throws) {
    EXPECT_THROW(Decimal::ToSByte(Decimal(200)), System::OverflowException);
}

TEST(DecimalTests2, ToInt16_Valid) {
    EXPECT_EQ(Decimal::ToInt16(Decimal(-30000)), -30000);
}

TEST(DecimalTests2, ToInt16_OutOfRange_Throws) {
    EXPECT_THROW(Decimal::ToInt16(Decimal(100000)), System::OverflowException);
}

TEST(DecimalTests2, ToUInt16_Valid) {
    EXPECT_EQ(Decimal::ToUInt16(Decimal(60000)), 60000);
}

TEST(DecimalTests2, ToUInt16_OutOfRange_Throws) {
    EXPECT_THROW(Decimal::ToUInt16(Decimal(-1)), System::OverflowException);
    EXPECT_THROW(Decimal::ToUInt16(Decimal(70000)), System::OverflowException);
}

// ---------------------------------------------------------------------------
// ToInt32/ToInt64/ToUInt32/ToUInt64 overflow parity (regression: previously
// silently wrapped instead of throwing, and ToUInt64 incorrectly threw for
// negative values that truncate to zero, e.g. -0.5).
// ---------------------------------------------------------------------------

TEST(DecimalTests2, ToInt32_Overflow_Throws) {
    EXPECT_THROW(Decimal::MaxValue.ToInt32(), System::OverflowException);
}

TEST(DecimalTests2, ToInt64_Overflow_Throws) {
    EXPECT_THROW(Decimal::MaxValue.ToInt64(), System::OverflowException);
}

TEST(DecimalTests2, ToUInt64_NegativeTruncatingToZero_ReturnsZero) {
    EXPECT_EQ(Decimal::Parse("-0.5").ToUInt64(), 0ULL);
}

TEST(DecimalTests2, ToUInt64_NegativeNonZero_Throws) {
    EXPECT_THROW(Decimal::Parse("-3.5").ToUInt64(), System::OverflowException);
}

TEST(DecimalTests2, ToUInt32_NegativeTruncatingToZero_ReturnsZero) {
    EXPECT_EQ(Decimal::Parse("-0.5").ToUInt32(), 0u);
}

// ---------------------------------------------------------------------------
// Round: MidpointRounding overloads and multi-digit correctness
// (regression: previous digit-by-digit rounding produced wrong results when
// dropping more than one decimal place, e.g. Round(12.49, 0) used to yield 13)
// ---------------------------------------------------------------------------

TEST(DecimalTests2, Round_MultiDigit_RoundsDownCorrectly) {
    EXPECT_EQ(Decimal::Round(Decimal::Parse("12.49"), 0), Decimal(12));
}

TEST(DecimalTests2, Round_MultiDigit_RoundsUpCorrectly) {
    EXPECT_EQ(Decimal::Round(Decimal::Parse("12.50"), 0), Decimal(12)); // ToEven: 12 is even
    EXPECT_EQ(Decimal::Round(Decimal::Parse("12.51"), 0), Decimal(13));
}

TEST(DecimalTests2, Round_DefaultIsToEven) {
    EXPECT_EQ(Decimal::Round(Decimal::Parse("0.5")), Decimal(0));
    EXPECT_EQ(Decimal::Round(Decimal::Parse("1.5")), Decimal(2));
    EXPECT_EQ(Decimal::Round(Decimal::Parse("2.5")), Decimal(2));
}

TEST(DecimalTests2, Round_AwayFromZero) {
    EXPECT_EQ(Decimal::Round(Decimal::Parse("2.5"), MidpointRounding::AwayFromZero), Decimal(3));
    EXPECT_EQ(Decimal::Round(Decimal::Parse("0.5"), MidpointRounding::AwayFromZero), Decimal(1));
}

TEST(DecimalTests2, Round_ToZero_Truncates) {
    EXPECT_EQ(Decimal::Round(Decimal::Parse("2.9"), 0, MidpointRounding::ToZero), Decimal(2));
    EXPECT_EQ(Decimal::Round(Decimal::Parse("-2.9"), 0, MidpointRounding::ToZero), Decimal(-2));
}

TEST(DecimalTests2, Round_ToNegativeInfinity) {
    EXPECT_EQ(Decimal::Round(Decimal::Parse("2.1"), 0, MidpointRounding::ToNegativeInfinity), Decimal(2));
    EXPECT_EQ(Decimal::Round(Decimal::Parse("-2.1"), 0, MidpointRounding::ToNegativeInfinity), Decimal(-3));
}

TEST(DecimalTests2, Round_ToPositiveInfinity) {
    EXPECT_EQ(Decimal::Round(Decimal::Parse("2.1"), 0, MidpointRounding::ToPositiveInfinity), Decimal(3));
    EXPECT_EQ(Decimal::Round(Decimal::Parse("-2.1"), 0, MidpointRounding::ToPositiveInfinity), Decimal(-2));
}

TEST(DecimalTests2, Round_MultiDecimalPlace) {
    EXPECT_EQ(Decimal::Round(Decimal::Parse("3.14159"), 2), Decimal::Parse("3.14"));
    EXPECT_EQ(Decimal::Round(Decimal::Parse("3.145"), 2), Decimal::Parse("3.14")); // tie -> even
    EXPECT_EQ(Decimal::Round(Decimal::Parse("3.155"), 2), Decimal::Parse("3.16")); // tie -> even
}

// SR-AUD-036 / CCF-008 (ticket #1855): an out-of-range MidpointRounding must throw
// System::ArgumentException(paramName "mode"), matching .NET Decimal.Round's
// `(uint)mode > (uint)MidpointRounding.ToPositiveInfinity` guard, rather than silently
// truncating. Decimal::Round funnels every overload through Round(d, decimals, mode), and the
// guard sits before the scale<=decimals early-out so even a no-op round validates the mode.
TEST(DecimalTests2, Round_InvalidMidpointRounding_Throws) {
    EXPECT_THROW(Decimal::Round(Decimal::Parse("1.9"), static_cast<MidpointRounding>(99)),
                 System::ArgumentException);
    EXPECT_THROW(Decimal::Round(Decimal::Parse("1.9"), static_cast<MidpointRounding>(-1)),
                 System::ArgumentException);
    EXPECT_THROW(Decimal::Round(Decimal::Parse("1.9"), static_cast<MidpointRounding>(5)),
                 System::ArgumentException);
    EXPECT_THROW(Decimal::Round(Decimal::Parse("1.9"), 0, static_cast<MidpointRounding>(99)),
                 System::ArgumentException);
    // Validated even when no rounding is required (scale already <= decimals).
    EXPECT_THROW(Decimal::Round(Decimal(5), 0, static_cast<MidpointRounding>(99)),
                 System::ArgumentException);
}

TEST(DecimalTests2, Round_InvalidMidpointRounding_ParamNameIsMode) {
    try {
        Decimal::Round(Decimal::Parse("1.9"), static_cast<MidpointRounding>(99));
        FAIL() << "expected ArgumentException";
    } catch (const System::ArgumentException& e) {
        EXPECT_EQ(e.getParamNameProperty(), "mode");
    }
}

TEST(DecimalTests2, Round_EveryNamedMode_StillReturnsExactValue) {
    EXPECT_EQ(Decimal::Round(Decimal::Parse("2.5"), 0, MidpointRounding::ToEven), Decimal(2));
    EXPECT_EQ(Decimal::Round(Decimal::Parse("2.5"), 0, MidpointRounding::AwayFromZero), Decimal(3));
    EXPECT_EQ(Decimal::Round(Decimal::Parse("2.9"), 0, MidpointRounding::ToZero), Decimal(2));
    EXPECT_EQ(Decimal::Round(Decimal::Parse("2.1"), 0, MidpointRounding::ToNegativeInfinity), Decimal(2));
    EXPECT_EQ(Decimal::Round(Decimal::Parse("2.1"), 0, MidpointRounding::ToPositiveInfinity), Decimal(3));
}

// ---------------------------------------------------------------------------
// CopySign / MaxMagnitude / MinMagnitude
// ---------------------------------------------------------------------------

TEST(DecimalTests2, CopySign) {
    EXPECT_EQ(Decimal::CopySign(Decimal(5), Decimal(-1)), Decimal(-5));
    EXPECT_EQ(Decimal::CopySign(Decimal(-5), Decimal(1)), Decimal(5));
}

TEST(DecimalTests2, MaxMagnitude) {
    EXPECT_EQ(Decimal::MaxMagnitude(Decimal(-5), Decimal(3)), Decimal(-5));
    EXPECT_EQ(Decimal::MaxMagnitude(Decimal(-5), Decimal(5)), Decimal(5)); // tie -> non-negative wins
}

TEST(DecimalTests2, MinMagnitude) {
    EXPECT_EQ(Decimal::MinMagnitude(Decimal(-5), Decimal(3)), Decimal(3));
    EXPECT_EQ(Decimal::MinMagnitude(Decimal(-5), Decimal(5)), Decimal(-5)); // tie -> negative wins
}

// ---------------------------------------------------------------------------
// IsInteger / IsNegative / IsPositive / IsEvenInteger / IsOddInteger / IsCanonical
// ---------------------------------------------------------------------------

TEST(DecimalTests2, IsInteger) {
    EXPECT_TRUE(Decimal::IsInteger(Decimal(5)));
    EXPECT_FALSE(Decimal::IsInteger(Decimal::Parse("5.5")));
}

TEST(DecimalTests2, IsNegativeIsPositive) {
    EXPECT_TRUE(Decimal::IsNegative(Decimal(-1)));
    EXPECT_FALSE(Decimal::IsPositive(Decimal(-1)));
    EXPECT_TRUE(Decimal::IsPositive(Decimal(1)));
    EXPECT_TRUE(Decimal::IsPositive(Decimal::Zero));
}

TEST(DecimalTests2, IsEvenOddInteger) {
    EXPECT_TRUE(Decimal::IsEvenInteger(Decimal(4)));
    EXPECT_FALSE(Decimal::IsEvenInteger(Decimal(5)));
    EXPECT_TRUE(Decimal::IsOddInteger(Decimal(5)));
    EXPECT_FALSE(Decimal::IsOddInteger(Decimal(4)));
    EXPECT_FALSE(Decimal::IsEvenInteger(Decimal::Parse("4.5")));
    EXPECT_FALSE(Decimal::IsOddInteger(Decimal::Parse("4.5")));
}

TEST(DecimalTests2, IsCanonical) {
    EXPECT_TRUE(Decimal::IsCanonical(Decimal(5)));
    EXPECT_TRUE(Decimal::IsCanonical(Decimal::Parse("5.5")));
    // 5.50 has a trailing zero in scale 2, so it's not canonical.
    Decimal nonCanonical(550, 0, 0, false, 2);
    EXPECT_FALSE(Decimal::IsCanonical(nonCanonical));
}

// ---------------------------------------------------------------------------
// Unary +, ++, --
// ---------------------------------------------------------------------------

TEST(DecimalTests2, UnaryPlus) {
    Decimal d(5);
    EXPECT_EQ(+d, Decimal(5));
}

TEST(DecimalTests2, PreIncrement) {
    Decimal d(5);
    Decimal& r = ++d;
    EXPECT_EQ(d, Decimal(6));
    EXPECT_EQ(&r, &d);
}

TEST(DecimalTests2, PostIncrement) {
    Decimal d(5);
    Decimal old = d++;
    EXPECT_EQ(old, Decimal(5));
    EXPECT_EQ(d, Decimal(6));
}

TEST(DecimalTests2, PreDecrement) {
    Decimal d(5);
    --d;
    EXPECT_EQ(d, Decimal(4));
}

TEST(DecimalTests2, PostDecrement) {
    Decimal d(5);
    Decimal old = d--;
    EXPECT_EQ(old, Decimal(5));
    EXPECT_EQ(d, Decimal(4));
}

// ---------------------------------------------------------------------------
// Explicit conversion operators
// ---------------------------------------------------------------------------

TEST(DecimalTests2, ExplicitConversionOperators) {
    Decimal d(42);
    EXPECT_EQ(static_cast<SharpRuntime::intcs>(d), 42);
    EXPECT_EQ(static_cast<SharpRuntime::longcs>(d), 42);
    EXPECT_EQ(static_cast<SharpRuntime::uintcs>(d), 42u);
    EXPECT_EQ(static_cast<SharpRuntime::ulongcs>(d), 42u);
    EXPECT_NEAR(static_cast<double>(d), 42.0, 1e-9);
    EXPECT_NEAR(static_cast<float>(d), 42.0f, 1e-6f);
    EXPECT_EQ(static_cast<SharpRuntime::bytecs>(d), 42);
    EXPECT_EQ(static_cast<SharpRuntime::sbytecs>(d), 42);
    EXPECT_EQ(static_cast<SharpRuntime::shortcs>(d), 42);
    EXPECT_EQ(static_cast<SharpRuntime::ushortcs>(d), 42);
}

// ---------------------------------------------------------------------------
// SR-AUD-035 overflow taxonomy (#1858, approved 2026-07-31, packet §B.8 item 2):
// a well-formed string whose magnitude is past decimal's range is an
// OverflowException in .NET (SR.Overflow_Decimal), not a FormatException.
// TryParse still returns false for both and still leaves the caller's value
// untouched -- the distinction exists only on the Parse path.
// ---------------------------------------------------------------------------

TEST(DecimalTests2, Parse_MagnitudePastRange_ThrowsOverflowException) {
    EXPECT_THROW(Decimal::Parse("79228162514264337593543950336"),
                 System::OverflowException);
    EXPECT_THROW(Decimal::Parse("-79228162514264337593543950336"),
                 System::OverflowException);
    EXPECT_THROW(Decimal::Parse("792281625142643375935439503350"),
                 System::OverflowException);
}

TEST(DecimalTests2, Parse_OverflowCarriesTheDotNetMessage) {
    try {
        (void)Decimal::Parse("79228162514264337593543950336");
        FAIL() << "expected OverflowException";
    } catch (const System::OverflowException& e) {
        EXPECT_NE(std::string(e.what()).find(
                      "Value was either too large or too small for a Decimal."),
                  std::string::npos)
            << e.what();
    }
}

TEST(DecimalTests2, Parse_MalformedTextIsStillAFormatException) {
    // The taxonomy split must not turn a genuinely malformed string into an
    // OverflowException -- that is the half the change could plausibly break.
    EXPECT_THROW(Decimal::Parse("abc"), System::FormatException);
    EXPECT_THROW(Decimal::Parse(""), System::FormatException);
    EXPECT_THROW(Decimal::Parse("   "), System::FormatException);
    EXPECT_THROW(Decimal::Parse("1.2.3"), System::FormatException);
    EXPECT_THROW(Decimal::Parse("1x"), System::FormatException);
}

TEST(DecimalTests2, TryParse_StillFalseForBothAndLeavesTheOutputUntouched) {
    Decimal poison(999);
    EXPECT_FALSE(Decimal::TryParse("79228162514264337593543950336", poison));
    EXPECT_EQ(poison, Decimal(999));
    EXPECT_FALSE(Decimal::TryParse("abc", poison));
    EXPECT_EQ(poison, Decimal(999));
}

TEST(DecimalTests2, Parse_InRangeBoundariesStillParse) {
    EXPECT_EQ(Decimal::Parse("79228162514264337593543950335"), Decimal::MaxValue);
    EXPECT_EQ(Decimal::Parse("-79228162514264337593543950335"), Decimal::MinValue);
}
