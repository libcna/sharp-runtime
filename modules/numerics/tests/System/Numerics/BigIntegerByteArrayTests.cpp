// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <vector>

#include "System/OverflowException.hpp"
#include "System/Numerics/BigInteger.hpp"

using System::Numerics::BigInteger;

TEST(BigIntegerByteArrayTest, ConstructorUsesSignedLittleEndianTwosComplementByDefault) {
    EXPECT_EQ(BigInteger(std::vector<uint8_t>{0x39, 0x30}).ToString(), "12345");
    EXPECT_EQ(BigInteger(std::vector<uint8_t>{0x7F}).ToString(), "127");
    EXPECT_EQ(BigInteger(std::vector<uint8_t>{0x80}).ToString(), "-128");
    EXPECT_EQ(BigInteger(std::vector<uint8_t>{0xFF}).ToString(), "-1");
    EXPECT_EQ(BigInteger(std::vector<uint8_t>{}).ToString(), "0");
}

TEST(BigIntegerByteArrayTest, ConstructorSupportsUnsignedAndBigEndianInput) {
    EXPECT_EQ(BigInteger(std::vector<uint8_t>{0xFF}, true).ToString(), "255");
    EXPECT_EQ(BigInteger(std::vector<uint8_t>{0x01, 0x00}, false, true).ToString(), "256");
    EXPECT_EQ(BigInteger(std::vector<uint8_t>{0x80, 0x00}, true, true).ToString(), "32768");
}

TEST(BigIntegerByteArrayTest, OutputIsMinimalAndPreservesRequiredSignByte) {
    EXPECT_EQ(BigInteger(128).ToByteArray(), std::vector<uint8_t>({0x80, 0x00}));
    EXPECT_EQ(BigInteger(128).ToByteArray(true), std::vector<uint8_t>({0x80}));
    EXPECT_EQ(BigInteger(-129).ToByteArray(), std::vector<uint8_t>({0x7F, 0xFF}));
    EXPECT_EQ(BigInteger(-129).ToByteArray(false, true), std::vector<uint8_t>({0xFF, 0x7F}));
    EXPECT_EQ(BigInteger::Zero.ToByteArray(), std::vector<uint8_t>({0x00}));
}

TEST(BigIntegerByteArrayTest, RoundTripsLargeValuesAndRejectsNegativeUnsignedOutput) {
    const BigInteger highBit = BigInteger::One << 80;
    const std::vector<uint8_t> littleUnsigned = highBit.ToByteArray(true);
    const std::vector<uint8_t> bigUnsigned = highBit.ToByteArray(true, true);

    EXPECT_EQ(littleUnsigned.size(), 11u);
    EXPECT_EQ(littleUnsigned.front(), 0);
    EXPECT_EQ(littleUnsigned.back(), 1);
    EXPECT_EQ(bigUnsigned.front(), 1);
    EXPECT_EQ(bigUnsigned.back(), 0);
    EXPECT_EQ(BigInteger(littleUnsigned, true), highBit);
    EXPECT_EQ(BigInteger(bigUnsigned, true, true), highBit);
    EXPECT_THROW(BigInteger(-1).ToByteArray(true), System::OverflowException);
}
