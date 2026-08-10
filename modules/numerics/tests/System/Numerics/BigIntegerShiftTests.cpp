// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <limits>
#include "System/Numerics/BigInteger.hpp"

using System::Numerics::BigInteger;

TEST(BigIntegerShiftTest, LeftShiftPreservesSignAndSupportsLargeValues) {
    EXPECT_EQ((BigInteger(3) << 4).ToString(), "48");
    EXPECT_EQ((BigInteger(-3) << 4).ToString(), "-48");
    EXPECT_EQ((BigInteger::One << 80).ToString(), "1208925819614629174706176");
}

TEST(BigIntegerShiftTest, RightShiftIsArithmeticForNegativeValues) {
    EXPECT_EQ((BigInteger(20) >> 2).ToString(), "5");
    EXPECT_EQ((BigInteger(-20) >> 2).ToString(), "-5");
    EXPECT_EQ((BigInteger(-5) >> 1).ToString(), "-3");
    EXPECT_EQ((BigInteger(-1) >> 100).ToString(), "-1");
    EXPECT_EQ(((BigInteger::One << 80) >> 64).ToString(), "65536");
}

TEST(BigIntegerShiftTest, NegativeShiftCountReversesDirection) {
    EXPECT_EQ((BigInteger(8) << -2).ToString(), "2");
    EXPECT_EQ((BigInteger(8) >> -2).ToString(), "32");
    EXPECT_EQ((BigInteger(-5) << -1).ToString(), "-3");
    EXPECT_EQ((BigInteger(-5) >> -1).ToString(), "-10");
    EXPECT_EQ((BigInteger(5) << std::numeric_limits<SharpRuntime::intcs>::min()).ToString(), "0");
    EXPECT_EQ((BigInteger(-5) << std::numeric_limits<SharpRuntime::intcs>::min()).ToString(), "-1");
}

TEST(BigIntegerShiftTest, CompoundAssignmentsProduceShiftedValues) {
    BigInteger value(7);
    value <<= 5;
    EXPECT_EQ(value.ToString(), "224");
    value >>= 6;
    EXPECT_EQ(value.ToString(), "3");

    BigInteger negative(-7);
    negative >>= 1;
    EXPECT_EQ(negative.ToString(), "-4");
}
