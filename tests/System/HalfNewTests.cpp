// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/Half.hpp"
#include <cmath>

using System::Half;

// Additional Half tests (basic FromSingle/comparison already in Task40Tests.cpp)

TEST(HalfNewTests, IsNaN_NaN_True) { EXPECT_TRUE(Half::IsNaN(Half::NaN)); }
TEST(HalfNewTests, IsNaN_Zero_False) { EXPECT_FALSE(Half::IsNaN(Half::Zero)); }
TEST(HalfNewTests, IsNaN_PositiveInfinity_False) {
    EXPECT_FALSE(Half::IsNaN(Half::PositiveInfinity));
}

TEST(HalfNewTests, IsInfinity_PositiveInfinity_True) {
    EXPECT_TRUE(Half::IsInfinity(Half::PositiveInfinity));
}
TEST(HalfNewTests, IsInfinity_NegativeInfinity_True) {
    EXPECT_TRUE(Half::IsInfinity(Half::NegativeInfinity));
}
TEST(HalfNewTests, IsInfinity_One_False) {
    EXPECT_FALSE(Half::IsInfinity(Half::FromSingle(1.0f)));
}

TEST(HalfNewTests, IsPositiveInfinity_True) {
    EXPECT_TRUE(Half::IsPositiveInfinity(Half::PositiveInfinity));
}
TEST(HalfNewTests, IsPositiveInfinity_NegInf_False) {
    EXPECT_FALSE(Half::IsPositiveInfinity(Half::NegativeInfinity));
}

TEST(HalfNewTests, IsNegativeInfinity_True) {
    EXPECT_TRUE(Half::IsNegativeInfinity(Half::NegativeInfinity));
}
TEST(HalfNewTests, IsNegativeInfinity_PosInf_False) {
    EXPECT_FALSE(Half::IsNegativeInfinity(Half::PositiveInfinity));
}

TEST(HalfNewTests, IsFinite_One_True) {
    EXPECT_TRUE(Half::IsFinite(Half::FromSingle(1.0f)));
}
TEST(HalfNewTests, IsFinite_Infinity_False) {
    EXPECT_FALSE(Half::IsFinite(Half::PositiveInfinity));
}
TEST(HalfNewTests, IsFinite_NaN_False) {
    EXPECT_FALSE(Half::IsFinite(Half::NaN));
}

TEST(HalfNewTests, IsNegative_NegInf_True) {
    EXPECT_TRUE(Half::IsNegative(Half::NegativeInfinity));
}
TEST(HalfNewTests, IsNegative_Zero_False) {
    EXPECT_FALSE(Half::IsNegative(Half::Zero));
}
TEST(HalfNewTests, IsNegative_NegativeOne_True) {
    EXPECT_TRUE(Half::IsNegative(Half::FromSingle(-1.0f)));
}

TEST(HalfNewTests, GetHashCode_SameValue_SameHash) {
    Half a = Half::FromSingle(3.14f);
    Half b = Half::FromSingle(3.14f);
    EXPECT_EQ(a.GetHashCode(), b.GetHashCode());
}

TEST(HalfNewTests, ToString_NonEmpty) {
    Half h = Half::FromSingle(1.5f);
    EXPECT_FALSE(h.ToString().empty());
}

TEST(HalfNewTests, MaxValue_RoundTrip_GreaterThanZero) {
    EXPECT_GT(Half::MaxValue.ToSingle(), 0.0f);
}
TEST(HalfNewTests, MinValue_RoundTrip_LessThanZero) {
    EXPECT_LT(Half::MinValue.ToSingle(), 0.0f);
}
TEST(HalfNewTests, Epsilon_RoundTrip_GreaterThanZero) {
    EXPECT_GT(Half::Epsilon.ToSingle(), 0.0f);
}
