// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Permanent regressions for the #2229 Core numeric special-value and rounding-contract family.
//
//   #2230 / SR-AUD-034  Single::IsPositive excluded a positive-sign NaN from the sign-bit contract.
//   #2231 / SR-AUD-037  Decimal::ToOACurrency truncated where .NET rounds to the nearest unit.
//   #2232 / SR-AUD-039  Math::Log(double, newBase) omitted .NET's base special cases.
//   #2233 / SR-AUD-040  Ties-to-even rounding followed the ambient fesetround() mode.
//
// Measured before the repair (build-probe/2229_probe1_before.log): 106 cases, **27 wrong**.
// After: 106 cases, **0 wrong**, with every control unchanged. Each suite pairs its corrected
// cases with the controls that keep the repair from moving ordinary values -- the failure a
// special-value fix is most likely to introduce.
//
// Three of the four findings have an in-repository sibling that was already correct
// (Double::IsPositive, MathF::Log, Math::roundToEvenImpl), so each suite asserts the pair AGREES
// after the repair, not merely that the broken half changed.

#include <gtest/gtest.h>

#include <cfenv>
#include <cmath>
#include <limits>

#include "System/ArgumentException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Decimal.hpp"
#include "System/Double.hpp"
#include "System/Math.hpp"
#include "System/MathF.hpp"
#include "System/MidpointRounding.hpp"
#include "System/OverflowException.hpp"
#include "System/Single.hpp"

using System::Decimal;
using System::Double;
using System::Math;
using System::MathF;
using System::MidpointRounding;
using System::Single;

namespace {

constexpr float  kFloatNaN  = std::numeric_limits<float>::quiet_NaN();
constexpr float  kFloatInf  = std::numeric_limits<float>::infinity();
constexpr double kDoubleNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kDoubleInf = std::numeric_limits<double>::infinity();

// RAII guard restoring the ambient FP rounding mode even if an assertion unwinds early. The
// #2233 suites deliberately mutate process-wide state, exactly as MathTests.cpp's own
// ticket-1723 guard does; mirroring that pattern rather than inventing a second one.
struct RoundingModeGuard {
    int saved = std::fegetround();
    ~RoundingModeGuard() { std::fesetround(saved); }
};

// Distinguishes -0 from +0, which EXPECT_FLOAT_EQ/EXPECT_DOUBLE_EQ do not.
template <typename T>
bool IsNegativeZero(T v) { return v == T(0) && std::signbit(v); }

} // namespace

// =============================================================================================
// #2230 / SR-AUD-034 -- IsPositive is the raw sign bit, so a positive-sign NaN is positive.
//
// .NET defines the generic-math predicate as `SingleToInt32Bits(value) >= 0`. Before the repair
// Single::IsPositive also required `!std::isnan(value)`, so IsPositive(+NaN) was false while the
// Double counterpart -- already the bare std::signbit -- said true.
// =============================================================================================

TEST(NumericSpecialValueTests, SingleIsPositiveFollowsTheSignBitForNaN) {
    const float positiveNaN = kFloatNaN;
    const float negativeNaN = std::copysign(kFloatNaN, -1.0f);

    ASSERT_TRUE(std::isnan(positiveNaN));
    ASSERT_TRUE(std::isnan(negativeNaN));
    ASSERT_FALSE(std::signbit(positiveNaN));
    ASSERT_TRUE(std::signbit(negativeNaN));

    EXPECT_TRUE(Single::IsPositive(positiveNaN));   // was false before #2230
    EXPECT_FALSE(Single::IsPositive(negativeNaN));
    EXPECT_FALSE(Single::IsNegative(positiveNaN));
    EXPECT_TRUE(Single::IsNegative(negativeNaN));
}

TEST(NumericSpecialValueTests, DoubleIsPositiveAgreesWithSingleOnNaNSign) {
    const double positiveNaN = kDoubleNaN;
    const double negativeNaN = std::copysign(kDoubleNaN, -1.0);

    // The sibling that was already correct; asserted so the pair is pinned as agreeing rather
    // than merely each being separately plausible.
    EXPECT_TRUE(Double::IsPositive(positiveNaN));
    EXPECT_FALSE(Double::IsPositive(negativeNaN));
    EXPECT_EQ(Single::IsPositive(kFloatNaN), Double::IsPositive(kDoubleNaN));
    EXPECT_EQ(Single::IsPositive(std::copysign(kFloatNaN, -1.0f)),
              Double::IsPositive(std::copysign(kDoubleNaN, -1.0)));
}

TEST(NumericSpecialValueTests, SingleIsPositiveOrdinaryValuesAreUnchanged) {
    EXPECT_TRUE(Single::IsPositive(0.0f));
    EXPECT_FALSE(Single::IsPositive(-0.0f));
    EXPECT_TRUE(Single::IsPositive(1.5f));
    EXPECT_FALSE(Single::IsPositive(-1.5f));
    EXPECT_TRUE(Single::IsPositive(kFloatInf));
    EXPECT_FALSE(Single::IsPositive(-kFloatInf));
    EXPECT_TRUE(Single::IsPositive(std::numeric_limits<float>::denorm_min()));
    EXPECT_FALSE(Single::IsPositive(-std::numeric_limits<float>::denorm_min()));
}

// =============================================================================================
// #2231 / SR-AUD-037 -- ToOACurrency rounds to the nearest currency unit.
//
// Before the repair the value was scaled by 10,000 and TRUNCATED, so every digit past the fourth
// decimal was discarded outright.
// =============================================================================================

TEST(NumericSpecialValueTests, DecimalToOACurrencyMatchesTheDocumentedDotNetValues) {
    // The two values the .NET documentation tabulates. Before #2231: 1234 and -792281.
    EXPECT_EQ(Decimal::Parse("0.123456789").ToOACurrency(), 1235LL);
    EXPECT_EQ(Decimal::Parse("-79.228162514264337593543950335").ToOACurrency(), -792282LL);

    // The static form must agree with the instance form.
    EXPECT_EQ(Decimal::ToOACurrency(Decimal::Parse("0.123456789")), 1235LL);
}

TEST(NumericSpecialValueTests, DecimalToOACurrencyRoundsRatherThanDiscarding) {
    // Below the fourth decimal the old truncation collapsed every one of these to zero.
    EXPECT_EQ(Decimal::Parse("0.00006").ToOACurrency(), 1LL);
    EXPECT_EQ(Decimal::Parse("-0.00006").ToOACurrency(), -1LL);
    EXPECT_EQ(Decimal::Parse("0.00004").ToOACurrency(), 0LL);
    EXPECT_EQ(Decimal::Parse("-0.00004").ToOACurrency(), 0LL);
    EXPECT_EQ(Decimal::Parse("1.23456").ToOACurrency(), 12346LL);
    EXPECT_EQ(Decimal::Parse("-1.23456").ToOACurrency(), -12346LL);
    EXPECT_EQ(Decimal::Parse("1.23454").ToOACurrency(), 12345LL);
}

// PINS the tie rule. Neither documented .NET value is a midpoint, so the examples cannot decide
// how a tie breaks. ToEven is implemented because .NET's DecCalc.VarCyFromDec reduces the scale
// through InternalRound(..., MidpointRounding.ToEven) and because every other rounding funnel in
// this port defaults to ToEven. Under AwayFromZero these three would be 10001, 10002 and -10001.
// Ticket #2234 is the deferred verification against the .NET reference.
TEST(NumericSpecialValueTests, DecimalToOACurrencyBreaksMidpointsToEven_PinnedBy2234) {
    EXPECT_EQ(Decimal::Parse("1.00005").ToOACurrency(), 10000LL);
    EXPECT_EQ(Decimal::Parse("1.00015").ToOACurrency(), 10002LL);
    EXPECT_EQ(Decimal::Parse("-1.00005").ToOACurrency(), -10000LL);
    EXPECT_EQ(Decimal::Parse("-1.00015").ToOACurrency(), -10002LL);
}

TEST(NumericSpecialValueTests, DecimalToOACurrencyExactlyRepresentableValuesAreUnchanged) {
    EXPECT_EQ(Decimal::Parse("0").ToOACurrency(), 0LL);
    EXPECT_EQ(Decimal::Parse("1").ToOACurrency(), 10000LL);
    EXPECT_EQ(Decimal::Parse("1.0000").ToOACurrency(), 10000LL);
    EXPECT_EQ(Decimal::Parse("-1.2345").ToOACurrency(), -12345LL);
    EXPECT_EQ(Decimal::Parse("100000000000000").ToOACurrency(), 1000000000000000000LL);

    // Round-trips through the untouched inverse conversion.
    EXPECT_EQ(Decimal::FromOACurrency(12345LL), Decimal::Parse("1.2345"));
    EXPECT_EQ(Decimal::FromOACurrency(Decimal::Parse("-1.2345").ToOACurrency()),
              Decimal::Parse("-1.2345"));
}

TEST(NumericSpecialValueTests, DecimalToOACurrencyReportsOverflowAsCurrency) {
    // Both doors already threw OverflowException before #2231; the type is unchanged and only the
    // message moves to .NET's SR.Overflow_Currency text.
    try {
        Decimal::MaxValue.ToOACurrency();
        FAIL() << "expected OverflowException";
    } catch (const System::OverflowException& e) {
        EXPECT_EQ(e.getMessageProperty(), "Value was either too large or too small for a Currency.");
    }

    // Scaling overflows Int64 rather than Decimal; the same message must come out of that door.
    try {
        Decimal::Parse("1000000000000000").ToOACurrency();
        FAIL() << "expected OverflowException";
    } catch (const System::OverflowException& e) {
        EXPECT_EQ(e.getMessageProperty(), "Value was either too large or too small for a Currency.");
    }

    EXPECT_THROW((void)Decimal::MinValue.ToOACurrency(), System::OverflowException);
}

// =============================================================================================
// #2232 / SR-AUD-039 -- Math::Log(double, newBase) special values.
//
// Before the repair the body was a bare std::log(a) / std::log(newBase), so Log(5, 1) was +Inf,
// Log(5, 0) was -0 and Log(5, +Inf) was +0 where .NET returns NaN for all three.
// =============================================================================================

TEST(NumericSpecialValueTests, MathLogWithBaseReturnsNaNForTheSpecialBases) {
    EXPECT_TRUE(std::isnan(Math::Log(5.0, 1.0)));        // was +Inf
    EXPECT_TRUE(std::isnan(Math::Log(0.5, 1.0)));
    EXPECT_TRUE(std::isnan(Math::Log(1.0, 1.0)));
    EXPECT_TRUE(std::isnan(Math::Log(5.0, 0.0)));        // was -0
    EXPECT_TRUE(std::isnan(Math::Log(5.0, kDoubleInf))); // was +0
    EXPECT_TRUE(std::isnan(Math::Log(0.0, 0.0)));
}

TEST(NumericSpecialValueTests, MathLogWithBasePropagatesNaNFromEitherPosition) {
    EXPECT_TRUE(std::isnan(Math::Log(kDoubleNaN, 2.0)));
    EXPECT_TRUE(std::isnan(Math::Log(2.0, kDoubleNaN)));
    EXPECT_TRUE(std::isnan(Math::Log(kDoubleNaN, kDoubleNaN)));
    // NaN in the argument wins over the base-one guard, matching .NET's evaluation order.
    EXPECT_TRUE(std::isnan(Math::Log(kDoubleNaN, 1.0)));
}

// The `a != 1` term of .NET's guard means an argument of exactly one falls THROUGH to the
// quotient, giving a signed zero rather than NaN. Both values were already correct before #2232
// and the repair must not disturb them -- this is the over-rejection control.
TEST(NumericSpecialValueTests, MathLogWithBaseKeepsTheSignedZeroFallThroughAtArgumentOne) {
    EXPECT_TRUE(IsNegativeZero(Math::Log(1.0, 0.0)));
    EXPECT_EQ(Math::Log(1.0, kDoubleInf), 0.0);
    EXPECT_FALSE(std::signbit(Math::Log(1.0, kDoubleInf)));
}

TEST(NumericSpecialValueTests, MathLogWithBaseOrdinaryValuesAreUnchanged) {
    EXPECT_DOUBLE_EQ(Math::Log(8.0, 2.0), 3.0);
    EXPECT_DOUBLE_EQ(Math::Log(100.0, 10.0), 2.0);
    EXPECT_DOUBLE_EQ(Math::Log(1.0, 10.0), 0.0);
    EXPECT_DOUBLE_EQ(Math::Log(kDoubleInf, 2.0), kDoubleInf);
    EXPECT_TRUE(std::isnan(Math::Log(-1.0, 2.0)));
}

TEST(NumericSpecialValueTests, MathLogWithBaseAgreesWithTheMathFSibling) {
    EXPECT_EQ(std::isnan(Math::Log(5.0, 1.0)), std::isnan(MathF::Log(5.0f, 1.0f)));
    EXPECT_EQ(std::isnan(Math::Log(5.0, 0.0)), std::isnan(MathF::Log(5.0f, 0.0f)));
    EXPECT_EQ(std::isnan(Math::Log(5.0, kDoubleInf)), std::isnan(MathF::Log(5.0f, kFloatInf)));
    EXPECT_EQ(IsNegativeZero(Math::Log(1.0, 0.0)), IsNegativeZero(MathF::Log(1.0f, 0.0f)));
    EXPECT_FLOAT_EQ(MathF::Log(8.0f, 2.0f), 3.0f);
}

// =============================================================================================
// #2233 / SR-AUD-040 -- ties-to-even independent of the ambient fesetround() mode.
//
// std::nearbyint follows the CURRENT rounding mode. Before the repair FE_UPWARD turned
// Round(2.5f) into 3 and FE_DOWNWARD/FE_TOWARDZERO turned Round(3.5f) into 3 and Round(-2.5f)
// into -3, at four production doors: MathF::Round(float), the ToEven arm of
// MathF::Round(float, MidpointRounding), Single::Round(float) and Double::Round(double).
// =============================================================================================

namespace {

void ExpectTiesToEvenAtEveryDoor(const char* mode) {
    SCOPED_TRACE(mode);

    EXPECT_FLOAT_EQ(MathF::Round(2.5f), 2.0f);
    EXPECT_FLOAT_EQ(MathF::Round(3.5f), 4.0f);
    EXPECT_FLOAT_EQ(MathF::Round(-2.5f), -2.0f);
    EXPECT_FLOAT_EQ(MathF::Round(-3.5f), -4.0f);
    EXPECT_FLOAT_EQ(MathF::Round(2.4f), 2.0f);
    EXPECT_FLOAT_EQ(MathF::Round(2.6f), 3.0f);

    EXPECT_FLOAT_EQ(MathF::Round(2.5f, MidpointRounding::ToEven), 2.0f);
    EXPECT_FLOAT_EQ(MathF::Round(0.5f, MidpointRounding::ToEven), 0.0f);
    EXPECT_FLOAT_EQ(MathF::Round(1.5f, MidpointRounding::ToEven), 2.0f);

    // The digits overloads funnel through the same arm, so they pick the same neighbour. Their
    // LAST operation is a division by the power of ten, which -- like every other C++ floating-
    // point operation -- observes the ambient mode, so the result can differ in the final ULP.
    // That is not this repair's residual: build-probe/2229_probe2_digits.log measures
    // Math::Round(2.25, 1), the sibling this family treats as the correct reference and which this
    // batch did not touch, deviating identically and matching a bare `22.0 / 10.0` to the bit.
    // EXPECT_FLOAT_EQ's 4-ULP tolerance is what makes these hold in every mode, deliberately.
    EXPECT_FLOAT_EQ(MathF::Round(2.25f, 1), 2.2f);
    EXPECT_FLOAT_EQ(MathF::Round(2.25f, 1, MidpointRounding::ToEven), 2.2f);
    EXPECT_NEAR(static_cast<double>(MathF::Round(2.25f, 1)), Math::Round(2.25, 1), 1e-6);
    // -2.25f and its scaled -22.5f are both exact in float, so this pins the negative tie without
    // depending on how a decimal literal lands.
    EXPECT_FLOAT_EQ(MathF::Round(-2.25f, 1), -2.2f);

    // The two wrapper doors the finding did not name.
    EXPECT_FLOAT_EQ(Single::Round(2.5f), 2.0f);
    EXPECT_FLOAT_EQ(Single::Round(3.5f), 4.0f);
    EXPECT_FLOAT_EQ(Single::Round(2.25f, 1), 2.2f);
    EXPECT_DOUBLE_EQ(Double::Round(2.5), 2.0);
    EXPECT_DOUBLE_EQ(Double::Round(3.5), 4.0);
    EXPECT_DOUBLE_EQ(Double::Round(2.25, 1), 2.2);

    // The sibling that was already mode-independent, asserted as the control.
    EXPECT_DOUBLE_EQ(Math::Round(2.5), 2.0);
    EXPECT_DOUBLE_EQ(Math::Round(3.5), 4.0);
    EXPECT_DOUBLE_EQ(Math::Round(2.5, MidpointRounding::ToEven), 2.0);
}

} // namespace

TEST(NumericSpecialValueTests, TiesToEvenIsIndependentOfTheAmbientRoundingMode) {
    RoundingModeGuard guard;

    std::fesetround(FE_TONEAREST);
    ExpectTiesToEvenAtEveryDoor("FE_TONEAREST");
    std::fesetround(FE_UPWARD);
    ExpectTiesToEvenAtEveryDoor("FE_UPWARD");
    std::fesetround(FE_DOWNWARD);
    ExpectTiesToEvenAtEveryDoor("FE_DOWNWARD");
    std::fesetround(FE_TOWARDZERO);
    ExpectTiesToEvenAtEveryDoor("FE_TOWARDZERO");
}

TEST(NumericSpecialValueTests, RoundPreservesSignedZeroAndNonFiniteValues) {
    RoundingModeGuard guard;
    std::fesetround(FE_TONEAREST);

    EXPECT_TRUE(IsNegativeZero(MathF::Round(-0.5f)));
    EXPECT_TRUE(IsNegativeZero(MathF::Round(-0.0f)));
    EXPECT_TRUE(IsNegativeZero(MathF::Round(-0.4f)));
    EXPECT_TRUE(IsNegativeZero(Single::Round(-0.5f)));
    EXPECT_TRUE(IsNegativeZero(Double::Round(-0.5)));
    EXPECT_FALSE(std::signbit(MathF::Round(0.5f)));

    EXPECT_EQ(MathF::Round(kFloatInf), kFloatInf);
    EXPECT_EQ(MathF::Round(-kFloatInf), -kFloatInf);
    EXPECT_TRUE(std::isnan(MathF::Round(kFloatNaN)));
    EXPECT_EQ(Double::Round(kDoubleInf), kDoubleInf);
    EXPECT_TRUE(std::isnan(Double::Round(kDoubleNaN)));

    // Magnitudes with no fractional part, including above the digits-overload round limits, must
    // come back bit-identical rather than through a scaling round trip.
    EXPECT_FLOAT_EQ(MathF::Round(1e30f), 1e30f);
    EXPECT_DOUBLE_EQ(Double::Round(1e300), 1e300);
    EXPECT_FLOAT_EQ(MathF::Round(16777216.0f), 16777216.0f);
}

// The repair must not touch the other MidpointRounding arms or the CCF-008 validation the
// switch default performs.
TEST(NumericSpecialValueTests, OtherMidpointRoundingModesAreUnchanged) {
    RoundingModeGuard guard;
    std::fesetround(FE_TONEAREST);

    EXPECT_FLOAT_EQ(MathF::Round(2.5f, MidpointRounding::AwayFromZero), 3.0f);
    EXPECT_FLOAT_EQ(MathF::Round(-2.5f, MidpointRounding::AwayFromZero), -3.0f);
    EXPECT_FLOAT_EQ(MathF::Round(2.7f, MidpointRounding::ToZero), 2.0f);
    EXPECT_FLOAT_EQ(MathF::Round(2.2f, MidpointRounding::ToNegativeInfinity), 2.0f);
    EXPECT_FLOAT_EQ(MathF::Round(2.2f, MidpointRounding::ToPositiveInfinity), 3.0f);

    EXPECT_THROW((void)MathF::Round(2.5f, static_cast<MidpointRounding>(9)),
                 System::ArgumentException);
    EXPECT_THROW((void)MathF::Round(2.5f, 7), System::ArgumentOutOfRangeException);
}
