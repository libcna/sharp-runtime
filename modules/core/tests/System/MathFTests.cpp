// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <cmath>
#include "System/ArgumentException.hpp"
#include "System/MathF.hpp"
#include "System/MidpointRounding.hpp"

using System::MathF;

TEST(MathFTest, Constants) {
    EXPECT_NEAR(MathF::E,   2.71828f, 1e-4f);
    EXPECT_NEAR(MathF::PI,  3.14159f, 1e-4f);
    EXPECT_NEAR(MathF::Tau, 6.28318f, 1e-4f);
}

TEST(MathFTest, Abs) {
    EXPECT_FLOAT_EQ(MathF::Abs(-3.5f), 3.5f);
    EXPECT_FLOAT_EQ(MathF::Abs(3.5f),  3.5f);
    EXPECT_FLOAT_EQ(MathF::Abs(0.0f),  0.0f);
}

TEST(MathFTest, CeilingFloor) {
    EXPECT_FLOAT_EQ(MathF::Ceiling(1.2f), 2.0f);
    EXPECT_FLOAT_EQ(MathF::Floor(1.9f),   1.0f);
}

TEST(MathFTest, Round) {
    // .NET's MathF.Round() default is round-to-even (banker's rounding), not
    // away-from-zero - verified against MathF.cs. 1.5 -> 2 (even) and 2.5 -> 2
    // (even), not 3.
    EXPECT_FLOAT_EQ(MathF::Round(1.5f), 2.0f);
    EXPECT_FLOAT_EQ(MathF::Round(2.5f), 2.0f);
    EXPECT_FLOAT_EQ(MathF::Round(1.456f, 2), 1.46f);
}

// SR-AUD-036 / CCF-008 (ticket #1855): an out-of-range MidpointRounding must throw
// System::ArgumentException(paramName "mode"), matching .NET MathF.Round's switch default
// (ThrowHelper.ThrowArgumentException_InvalidEnumValue), rather than silently rounding to even.
TEST(MathFTest, Round_InvalidMidpointRounding_Throws) {
    EXPECT_THROW(MathF::Round(1.9f, static_cast<System::MidpointRounding>(99)),
                 System::ArgumentException);
    EXPECT_THROW(MathF::Round(1.9f, static_cast<System::MidpointRounding>(-1)),
                 System::ArgumentException);
    EXPECT_THROW(MathF::Round(1.9f, static_cast<System::MidpointRounding>(5)),
                 System::ArgumentException);
    // Digits overload funnels through Round(x, mode) for in-range magnitudes.
    EXPECT_THROW(MathF::Round(1.456f, 2, static_cast<System::MidpointRounding>(99)),
                 System::ArgumentException);
}

TEST(MathFTest, Round_InvalidMidpointRounding_ParamNameIsMode) {
    try {
        MathF::Round(1.9f, static_cast<System::MidpointRounding>(99));
        FAIL() << "expected ArgumentException";
    } catch (const System::ArgumentException& e) {
        EXPECT_EQ(e.getParamNameProperty(), "mode");
    }
}

TEST(MathFTest, Round_EveryNamedMode_StillReturnsExactValue) {
    EXPECT_FLOAT_EQ(MathF::Round(2.5f, System::MidpointRounding::ToEven), 2.0f);
    EXPECT_FLOAT_EQ(MathF::Round(2.5f, System::MidpointRounding::AwayFromZero), 3.0f);
    EXPECT_FLOAT_EQ(MathF::Round(2.9f, System::MidpointRounding::ToZero), 2.0f);
    EXPECT_FLOAT_EQ(MathF::Round(2.9f, System::MidpointRounding::ToNegativeInfinity), 2.0f);
    EXPECT_FLOAT_EQ(MathF::Round(2.1f, System::MidpointRounding::ToPositiveInfinity), 3.0f);
}

// Ticket #1946: the five rounding branches used the C99 float-suffixed <cmath> spellings
// (std::nearbyintf/roundf/truncf/floorf/ceilf). libstdc++ declares the TR1/C99 set inside
// namespace std but NOT std::floorf or std::ceilf, so on GCC 13 / libstdc++ 13 -- and on Clang
// using the same standard library -- two of those lines were a hard compile error and the whole
// repository stopped building at the seventh object file. The repair moves all five to the
// unsuffixed names, whose float overloads take and return float, matching what Ceiling/Floor/
// Truncate in the same header already do.
//
// These cases exist to pin the RESULTS across that substitution: floor/ceil/trunc/round/
// nearbyint are exact on every finite float, so a correct repair cannot change any of them,
// and a wrong one (picking a different mode, or losing the sign of a negative) shows up here.
TEST(MathFTest, Round_UnsuffixedCmathSubstitution_PreservesEveryModeOnNegativesAndIntegrals) {
    // Negative midpoints exercise the sign-dependent difference between the five modes.
    EXPECT_FLOAT_EQ(MathF::Round(-2.5f, System::MidpointRounding::ToEven), -2.0f);
    EXPECT_FLOAT_EQ(MathF::Round(-2.5f, System::MidpointRounding::AwayFromZero), -3.0f);
    EXPECT_FLOAT_EQ(MathF::Round(-2.5f, System::MidpointRounding::ToZero), -2.0f);
    EXPECT_FLOAT_EQ(MathF::Round(-2.5f, System::MidpointRounding::ToNegativeInfinity), -3.0f);
    EXPECT_FLOAT_EQ(MathF::Round(-2.5f, System::MidpointRounding::ToPositiveInfinity), -2.0f);

    // A negative non-midpoint separates floor from trunc and ceil from trunc.
    EXPECT_FLOAT_EQ(MathF::Round(-2.1f, System::MidpointRounding::ToZero), -2.0f);
    EXPECT_FLOAT_EQ(MathF::Round(-2.1f, System::MidpointRounding::ToNegativeInfinity), -3.0f);
    EXPECT_FLOAT_EQ(MathF::Round(-2.1f, System::MidpointRounding::ToPositiveInfinity), -2.0f);

    // An already-integral value is a fixed point of all five modes, in both signs.
    for (const auto mode : {System::MidpointRounding::ToEven,
                            System::MidpointRounding::AwayFromZero,
                            System::MidpointRounding::ToZero,
                            System::MidpointRounding::ToNegativeInfinity,
                            System::MidpointRounding::ToPositiveInfinity}) {
        EXPECT_FLOAT_EQ(MathF::Round(3.0f, mode), 3.0f);
        EXPECT_FLOAT_EQ(MathF::Round(-3.0f, mode), -3.0f);
        EXPECT_FLOAT_EQ(MathF::Round(0.0f, mode), 0.0f);
    }

    // The parameterless overload is round-to-even and shares the same substitution.
    EXPECT_FLOAT_EQ(MathF::Round(-2.5f), -2.0f);
    EXPECT_FLOAT_EQ(MathF::Round(-1.5f), -2.0f);
    EXPECT_FLOAT_EQ(MathF::Round(-0.5f), -0.0f);
    EXPECT_FLOAT_EQ(MathF::Round(0.5f), 0.0f);
    EXPECT_FLOAT_EQ(MathF::Round(3.0f), 3.0f);

    // Every overload still returns float, not a promoted double narrowed on the way out.
    static_assert(std::is_same_v<decltype(MathF::Round(1.0f)), float>);
    static_assert(std::is_same_v<
                  decltype(MathF::Round(1.0f, System::MidpointRounding::ToEven)), float>);
}

TEST(MathFTest, Sqrt) {
    EXPECT_NEAR(MathF::Sqrt(4.0f), 2.0f, 1e-6f);
    EXPECT_NEAR(MathF::Sqrt(9.0f), 3.0f, 1e-6f);
}

TEST(MathFTest, Pow) {
    EXPECT_NEAR(MathF::Pow(2.0f, 10.0f), 1024.0f, 1e-3f);
}

TEST(MathFTest, Log) {
    EXPECT_NEAR(MathF::Log(MathF::E), 1.0f, 1e-5f);
    EXPECT_NEAR(MathF::Log(100.0f, 10.0f), 2.0f, 1e-5f);
    EXPECT_NEAR(MathF::Log2(8.0f),  3.0f, 1e-5f);
    EXPECT_NEAR(MathF::Log10(1000.0f), 3.0f, 1e-5f);
}

TEST(MathFTest, Log_BaseOne_IsAlwaysNaN) {
    // .NET's MathF.Log(x, y) special-cases y == 1 to always return NaN, since a
    // naive log(x)/log(y) would divide by log(1) == 0 and give +-infinity or NaN
    // depending on x - verified against MathF.cs.
    EXPECT_TRUE(std::isnan(MathF::Log(5.0f, 1.0f)));
    EXPECT_TRUE(std::isnan(MathF::Log(1.0f, 1.0f)));
}

TEST(MathFTest, Log_BaseZeroOrInfinity_NaNUnlessXIsOne) {
    EXPECT_TRUE(std::isnan(MathF::Log(5.0f, 0.0f)));
    EXPECT_TRUE(std::isnan(MathF::Log(5.0f, std::numeric_limits<float>::infinity())));
    EXPECT_NEAR(MathF::Log(1.0f, 0.0f), 0.0f, 1e-6f);
}

TEST(MathFTest, Log_NaNInputs_Propagate) {
    float nan = std::numeric_limits<float>::quiet_NaN();
    EXPECT_TRUE(std::isnan(MathF::Log(nan, 2.0f)));
    EXPECT_TRUE(std::isnan(MathF::Log(2.0f, nan)));
}

TEST(MathFTest, Trig) {
    EXPECT_NEAR(MathF::Sin(0.0f), 0.0f, 1e-6f);
    EXPECT_NEAR(MathF::Cos(0.0f), 1.0f, 1e-6f);
    EXPECT_NEAR(MathF::Sin(MathF::PI / 2.0f), 1.0f, 1e-5f);
    EXPECT_NEAR(MathF::Tan(MathF::PI / 4.0f), 1.0f, 1e-5f);
}

TEST(MathFTest, InverseTrig) {
    EXPECT_NEAR(MathF::Asin(1.0f), MathF::PI / 2.0f, 1e-5f);
    EXPECT_NEAR(MathF::Acos(1.0f), 0.0f, 1e-5f);
    EXPECT_NEAR(MathF::Atan(1.0f), MathF::PI / 4.0f, 1e-5f);
    EXPECT_NEAR(MathF::Atan2(1.0f, 1.0f), MathF::PI / 4.0f, 1e-5f);
}

TEST(MathFTest, MinMax) {
    EXPECT_FLOAT_EQ(MathF::Min(2.0f, 5.0f), 2.0f);
    EXPECT_FLOAT_EQ(MathF::Max(2.0f, 5.0f), 5.0f);
}

TEST(MathFTest, MinMax_PropagatesNaN) {
    // .NET's MathF.Max/Min propagate NaN if either argument is NaN (IEEE 754-2019
    // `maximum`/`minimum`), unlike std::fmax/fmin which return the non-NaN side -
    // verified against Math.cs.
    float nan = std::numeric_limits<float>::quiet_NaN();
    EXPECT_TRUE(std::isnan(MathF::Max(nan, 1.0f)));
    EXPECT_TRUE(std::isnan(MathF::Max(1.0f, nan)));
    EXPECT_TRUE(std::isnan(MathF::Min(nan, 1.0f)));
    EXPECT_TRUE(std::isnan(MathF::Min(1.0f, nan)));
}

TEST(MathFTest, MinMax_PositiveZeroBeatsNegativeZero) {
    // Matches .NET: ties between +0 and -0 are broken with +0 > -0.
    float posZero = 0.0f, negZero = -0.0f;
    EXPECT_TRUE(std::signbit(negZero));
    EXPECT_EQ(MathF::Max(posZero, negZero), posZero);
    EXPECT_FALSE(std::signbit(MathF::Max(posZero, negZero)));
    EXPECT_EQ(MathF::Min(posZero, negZero), negZero);
    EXPECT_TRUE(std::signbit(MathF::Min(posZero, negZero)));
}

TEST(MathFTest, Clamp) {
    EXPECT_FLOAT_EQ(MathF::Clamp(5.0f, 0.0f, 3.0f), 3.0f);
    EXPECT_FLOAT_EQ(MathF::Clamp(-1.0f, 0.0f, 3.0f), 0.0f);
    EXPECT_FLOAT_EQ(MathF::Clamp(2.0f, 0.0f, 3.0f), 2.0f);
}
// SR-AUD-022 (#1846): an inverted interval must throw (was silently returning a bound).
TEST(MathFTest, Clamp_MinGreaterThanMax_Throws) {
    EXPECT_THROW(MathF::Clamp(5.0f, 10.0f, 0.0f), System::ArgumentException);
    EXPECT_FLOAT_EQ(MathF::Clamp(5.0f, 7.0f, 7.0f), 7.0f);
}

TEST(MathFTest, Sign) {
    EXPECT_EQ(MathF::Sign(-5.0f), -1);
    EXPECT_EQ(MathF::Sign(0.0f),   0);
    EXPECT_EQ(MathF::Sign(5.0f),   1);
}

TEST(MathFTest, Sign_NaN_Throws) {
    // .NET's Math.Sign(float) throws ArithmeticException for NaN - verified
    // against Math.cs. A naive comparison-based Sign() would silently return 0
    // instead, since all relational comparisons against NaN are false.
    EXPECT_THROW(MathF::Sign(std::numeric_limits<float>::quiet_NaN()), System::ArithmeticException);
}

TEST(MathFTest, IsNaNInfinity) {
    EXPECT_TRUE(MathF::IsNaN(std::numeric_limits<float>::quiet_NaN()));
    EXPECT_FALSE(MathF::IsNaN(1.0f));
    EXPECT_TRUE(MathF::IsInfinity(std::numeric_limits<float>::infinity()));
    EXPECT_TRUE(MathF::IsPositiveInfinity(std::numeric_limits<float>::infinity()));
    EXPECT_TRUE(MathF::IsNegativeInfinity(-std::numeric_limits<float>::infinity()));
    EXPECT_TRUE(MathF::IsFinite(1.0f));
    EXPECT_FALSE(MathF::IsFinite(std::numeric_limits<float>::infinity()));
}

TEST(MathFTest, Truncate) {
    EXPECT_FLOAT_EQ(MathF::Truncate(3.9f),  3.0f);
    EXPECT_FLOAT_EQ(MathF::Truncate(-3.9f), -3.0f);
}

TEST(MathFTest, IEEERemainder) {
    EXPECT_NEAR(MathF::IEEERemainder(3.0f, 2.0f), -1.0f, 1e-5f);
}

TEST(MathFTest, SinCos) {
    auto result = MathF::SinCos(0.0f);
    EXPECT_NEAR(result.Sin, 0.0f, 1e-6f);
    EXPECT_NEAR(result.Cos, 1.0f, 1e-6f);

    auto r2 = MathF::SinCos(MathF::PI / 2.0f);
    EXPECT_NEAR(r2.Sin, 1.0f, 1e-5f);
    EXPECT_NEAR(r2.Cos, 0.0f, 1e-5f);
}

TEST(MathFTest, CopySign) {
    EXPECT_FLOAT_EQ(MathF::CopySign(3.0f, -1.0f), -3.0f);
    EXPECT_FLOAT_EQ(MathF::CopySign(-3.0f, 1.0f),  3.0f);
}

TEST(MathFTest, FusedMultiplyAdd) {
    EXPECT_NEAR(MathF::FusedMultiplyAdd(2.0f, 3.0f, 4.0f), 10.0f, 1e-5f);
}

TEST(MathFTest, ScaleB) {
    EXPECT_FLOAT_EQ(MathF::ScaleB(1.0f, 3), 8.0f);
}

TEST(MathFTest, MaxMinMagnitude) {
    EXPECT_FLOAT_EQ(MathF::MaxMagnitude(-5.0f, 3.0f), -5.0f);
    EXPECT_FLOAT_EQ(MathF::MinMagnitude(-5.0f, 3.0f),  3.0f);
}
