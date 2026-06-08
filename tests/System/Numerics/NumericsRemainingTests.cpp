// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Tests for: MathF (single-precision math), BitOperations (bit manipulation).
#include <gtest/gtest.h>
#include <cmath>
#include "System/MathF.hpp"
#include "System/Numerics/BitOperations.hpp"

using System::MathF;
using System::Numerics::BitOperations;

// ===========================================================================
// MathF
// ===========================================================================

TEST(MathFTests, Constants) {
    EXPECT_FLOAT_EQ(MathF::PI, 3.14159265f);
    EXPECT_FLOAT_EQ(MathF::E,  2.71828183f);
    EXPECT_FLOAT_EQ(MathF::Tau, MathF::PI * 2.0f);
}
TEST(MathFTests, Abs_NegativeToPositive) {
    EXPECT_FLOAT_EQ(MathF::Abs(-3.5f), 3.5f);
}
TEST(MathFTests, Ceiling_RoundsUp) {
    EXPECT_FLOAT_EQ(MathF::Ceiling(1.2f), 2.0f);
}
TEST(MathFTests, Floor_RoundsDown) {
    EXPECT_FLOAT_EQ(MathF::Floor(1.9f), 1.0f);
}
TEST(MathFTests, Round_MidpointToNearest) {
    EXPECT_FLOAT_EQ(MathF::Round(2.5f), 3.0f);
}
TEST(MathFTests, Truncate_DropsDecimal) {
    EXPECT_FLOAT_EQ(MathF::Truncate(3.9f), 3.0f);
    EXPECT_FLOAT_EQ(MathF::Truncate(-3.9f), -3.0f);
}
TEST(MathFTests, Sqrt_SquareRoot) {
    EXPECT_FLOAT_EQ(MathF::Sqrt(9.0f), 3.0f);
}
TEST(MathFTests, Pow_Power) {
    EXPECT_FLOAT_EQ(MathF::Pow(2.0f, 10.0f), 1024.0f);
}
TEST(MathFTests, Log2_PowerOfTwo) {
    EXPECT_FLOAT_EQ(MathF::Log2(8.0f), 3.0f);
}
TEST(MathFTests, Log10_PowerOfTen) {
    EXPECT_NEAR(MathF::Log10(100.0f), 2.0f, 1e-6f);
}
TEST(MathFTests, Sin_Cos) {
    EXPECT_NEAR(MathF::Sin(MathF::PI / 2.0f), 1.0f, 1e-6f);
    EXPECT_NEAR(MathF::Cos(0.0f), 1.0f, 1e-6f);
}
TEST(MathFTests, Atan2_FirstQuadrant) {
    EXPECT_NEAR(MathF::Atan2(1.0f, 1.0f), MathF::PI / 4.0f, 1e-6f);
}
TEST(MathFTests, Max_Min) {
    EXPECT_FLOAT_EQ(MathF::Max(3.0f, 7.0f), 7.0f);
    EXPECT_FLOAT_EQ(MathF::Min(3.0f, 7.0f), 3.0f);
}
TEST(MathFTests, Clamp_InRange) {
    EXPECT_FLOAT_EQ(MathF::Clamp(5.0f, 0.0f, 10.0f), 5.0f);
}
TEST(MathFTests, Clamp_BelowMin) {
    EXPECT_FLOAT_EQ(MathF::Clamp(-5.0f, 0.0f, 10.0f), 0.0f);
}
TEST(MathFTests, Clamp_AboveMax) {
    EXPECT_FLOAT_EQ(MathF::Clamp(15.0f, 0.0f, 10.0f), 10.0f);
}
TEST(MathFTests, Sign_Positive_Negative_Zero) {
    EXPECT_FLOAT_EQ(MathF::Sign(3.0f), 1.0f);
    EXPECT_FLOAT_EQ(MathF::Sign(-3.0f), -1.0f);
    EXPECT_FLOAT_EQ(MathF::Sign(0.0f), 0.0f);
}
TEST(MathFTests, IsNaN_IsInfinity) {
    EXPECT_TRUE(MathF::IsNaN(std::numeric_limits<float>::quiet_NaN()));
    EXPECT_FALSE(MathF::IsNaN(1.0f));
    EXPECT_TRUE(MathF::IsInfinity(std::numeric_limits<float>::infinity()));
    EXPECT_FALSE(MathF::IsInfinity(1.0f));
}
TEST(MathFTests, IsPositiveInfinity_IsNegativeInfinity) {
    float pos_inf = std::numeric_limits<float>::infinity();
    EXPECT_TRUE(MathF::IsPositiveInfinity(pos_inf));
    EXPECT_TRUE(MathF::IsNegativeInfinity(-pos_inf));
    EXPECT_FALSE(MathF::IsPositiveInfinity(-pos_inf));
}

// ===========================================================================
// BitOperations
// ===========================================================================

TEST(BitOperationsTests, IsPow2_True_ForPowers) {
    EXPECT_TRUE(BitOperations::IsPow2(uint32_t(1)));
    EXPECT_TRUE(BitOperations::IsPow2(uint32_t(16)));
    EXPECT_TRUE(BitOperations::IsPow2(uint32_t(1024)));
}
TEST(BitOperationsTests, IsPow2_False_ForNonPowers) {
    EXPECT_FALSE(BitOperations::IsPow2(uint32_t(0)));
    EXPECT_FALSE(BitOperations::IsPow2(uint32_t(3)));
    EXPECT_FALSE(BitOperations::IsPow2(uint32_t(6)));
}
TEST(BitOperationsTests, IsPow2_Int32) {
    EXPECT_TRUE(BitOperations::IsPow2(int32_t(8)));
    EXPECT_FALSE(BitOperations::IsPow2(int32_t(0)));
    EXPECT_FALSE(BitOperations::IsPow2(int32_t(-4)));
}
TEST(BitOperationsTests, RoundUpToPowerOf2_Zero_ReturnsOne) {
    EXPECT_EQ(BitOperations::RoundUpToPowerOf2(uint32_t(0)), 1u);
}
TEST(BitOperationsTests, RoundUpToPowerOf2_AlreadyPow2_Unchanged) {
    EXPECT_EQ(BitOperations::RoundUpToPowerOf2(uint32_t(8)), 8u);
}
TEST(BitOperationsTests, RoundUpToPowerOf2_NotPow2_RoundsUp) {
    EXPECT_EQ(BitOperations::RoundUpToPowerOf2(uint32_t(5)), 8u);
    EXPECT_EQ(BitOperations::RoundUpToPowerOf2(uint32_t(9)), 16u);
}
TEST(BitOperationsTests, LeadingZeroCount_32) {
    EXPECT_EQ(BitOperations::LeadingZeroCount(uint32_t(0)), 32);
    EXPECT_EQ(BitOperations::LeadingZeroCount(uint32_t(1)), 31);
    EXPECT_EQ(BitOperations::LeadingZeroCount(uint32_t(0x80000000u)), 0);
}
TEST(BitOperationsTests, Log2_Values) {
    EXPECT_EQ(BitOperations::Log2(uint32_t(1)),  0);
    EXPECT_EQ(BitOperations::Log2(uint32_t(2)),  1);
    EXPECT_EQ(BitOperations::Log2(uint32_t(8)),  3);
    EXPECT_EQ(BitOperations::Log2(uint32_t(255)), 7);
}
TEST(BitOperationsTests, PopCount_32) {
    EXPECT_EQ(BitOperations::PopCount(uint32_t(0)), 0);
    EXPECT_EQ(BitOperations::PopCount(uint32_t(0xFFFFFFFFu)), 32);
    EXPECT_EQ(BitOperations::PopCount(uint32_t(0b10110101)), 5);
}
TEST(BitOperationsTests, TrailingZeroCount_32) {
    EXPECT_EQ(BitOperations::TrailingZeroCount(uint32_t(0)), 32);
    EXPECT_EQ(BitOperations::TrailingZeroCount(uint32_t(1)), 0);
    EXPECT_EQ(BitOperations::TrailingZeroCount(uint32_t(8)), 3);
}
TEST(BitOperationsTests, RotateLeft_32) {
    uint32_t val = 0x80000000u;
    EXPECT_EQ(BitOperations::RotateLeft(val, 1), 1u);
}
TEST(BitOperationsTests, RotateRight_32) {
    uint32_t val = 1u;
    EXPECT_EQ(BitOperations::RotateRight(val, 1), 0x80000000u);
}
TEST(BitOperationsTests, ReverseBits_Zero_One) {
    EXPECT_EQ(BitOperations::ReverseBits(uint32_t(0)), 0u);
    EXPECT_EQ(BitOperations::ReverseBits(uint32_t(1)), 0x80000000u);
    EXPECT_EQ(BitOperations::ReverseBits(uint32_t(0x80000000u)), 1u);
}
