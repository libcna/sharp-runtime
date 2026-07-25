// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/Numerics/BigInteger.hpp"

using System::Numerics::BigInteger;

TEST(BigIntegerBitwiseTest, PositiveOperandsProduceExpectedValues) {
    const BigInteger left(5);
    const BigInteger right(3);

    EXPECT_EQ((left & right).ToString(), "1");
    EXPECT_EQ((left | right).ToString(), "7");
    EXPECT_EQ((left ^ right).ToString(), "6");
    EXPECT_EQ((~left).ToString(), "-6");
}

TEST(BigIntegerBitwiseTest, NegativeOperandsUseTwosComplementSemantics) {
    const BigInteger negativeFive(-5);
    const BigInteger three(3);

    EXPECT_EQ((negativeFive & three).ToString(), "3");
    EXPECT_EQ((negativeFive | three).ToString(), "-5");
    EXPECT_EQ((negativeFive ^ three).ToString(), "-8");
    EXPECT_EQ((~negativeFive).ToString(), "4");
}

TEST(BigIntegerBitwiseTest, NegativePairsAndZeroRetainInfiniteSignExtension) {
    const BigInteger negativeFive(-5);
    const BigInteger negativeThree(-3);

    EXPECT_EQ((negativeFive & negativeThree).ToString(), "-7");
    EXPECT_EQ((negativeFive | negativeThree).ToString(), "-1");
    EXPECT_EQ((negativeFive ^ negativeThree).ToString(), "6");
    EXPECT_EQ((BigInteger::Zero & negativeFive).ToString(), "0");
    EXPECT_EQ((BigInteger::Zero | negativeFive).ToString(), "-5");
    EXPECT_EQ((~BigInteger::Zero).ToString(), "-1");
}

TEST(BigIntegerBitwiseTest, OperatesBeyondNativeIntegerWidthAndSupportsAssignments) {
    const BigInteger highBit = BigInteger::Parse("18446744073709551616");
    const BigInteger lowBits = BigInteger::Parse("4294967295");

    EXPECT_EQ((highBit & lowBits).ToString(), "0");
    EXPECT_EQ((highBit | lowBits).ToString(), "18446744078004518911");
    EXPECT_EQ((highBit ^ lowBits).ToString(), "18446744078004518911");

    BigInteger assigned = highBit;
    assigned |= lowBits;
    EXPECT_EQ(assigned.ToString(), "18446744078004518911");
    assigned &= highBit;
    EXPECT_EQ(assigned.ToString(), "18446744073709551616");
    assigned ^= highBit;
    EXPECT_EQ(assigned.ToString(), "0");
}
