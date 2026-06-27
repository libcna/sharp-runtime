// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/Buffers/Binary/BinaryPrimitives.hpp"

using System::Buffers::Binary::BinaryPrimitives;
using System::ReadOnlySpan;
using System::Span;

TEST(BinaryPrimitivesTest, ReadInt32LittleEndian) {
    uint8_t buf[] = { 0x01, 0x00, 0x00, 0x00 };
    ReadOnlySpan<uint8_t> span(buf, 4);
    EXPECT_EQ(BinaryPrimitives::ReadInt32LittleEndian(span), 1);
}

TEST(BinaryPrimitivesTest, ReadInt32BigEndian) {
    uint8_t buf[] = { 0x00, 0x00, 0x00, 0x01 };
    ReadOnlySpan<uint8_t> span(buf, 4);
    EXPECT_EQ(BinaryPrimitives::ReadInt32BigEndian(span), 1);
}

TEST(BinaryPrimitivesTest, WriteAndReadInt32RoundTrip) {
    uint8_t buf[4] = {};
    Span<uint8_t> dst(buf, 4);
    BinaryPrimitives::WriteInt32LittleEndian(dst, 0x12345678);
    ReadOnlySpan<uint8_t> src(buf, 4);
    EXPECT_EQ(BinaryPrimitives::ReadInt32LittleEndian(src), 0x12345678);
}

TEST(BinaryPrimitivesTest, WriteInt32BigEndian) {
    uint8_t buf[4] = {};
    Span<uint8_t> dst(buf, 4);
    BinaryPrimitives::WriteInt32BigEndian(dst, 0x01020304);
    EXPECT_EQ(buf[0], 0x01);
    EXPECT_EQ(buf[3], 0x04);
}

TEST(BinaryPrimitivesTest, ReadUInt16LittleEndian) {
    uint8_t buf[] = { 0xFF, 0x00 };
    ReadOnlySpan<uint8_t> span(buf, 2);
    EXPECT_EQ(BinaryPrimitives::ReadUInt16LittleEndian(span), 0x00FFu);
}

TEST(BinaryPrimitivesTest, ReadInt64LittleEndian) {
    uint8_t buf[] = { 1,0,0,0, 0,0,0,0 };
    ReadOnlySpan<uint8_t> span(buf, 8);
    EXPECT_EQ(BinaryPrimitives::ReadInt64LittleEndian(span), 1LL);
}

TEST(BinaryPrimitivesTest, ReverseEndiannessUInt16) {
    EXPECT_EQ(BinaryPrimitives::ReverseEndianness(uint16_t(0x1234)), uint16_t(0x3412));
}

TEST(BinaryPrimitivesTest, ReverseEndiannessUInt32) {
    EXPECT_EQ(BinaryPrimitives::ReverseEndianness(uint32_t(0x12345678)), uint32_t(0x78563412));
}

TEST(BinaryPrimitivesTest, ReverseEndiannessInt16) {
    int16_t in = 0x0102;
    int16_t expected;
    uint16_t u = BinaryPrimitives::ReverseEndianness(uint16_t(0x0102));
    std::memcpy(&expected, &u, 2);
    EXPECT_EQ(BinaryPrimitives::ReverseEndianness(in), expected);
}

TEST(BinaryPrimitivesTest, SpanTooSmallThrows) {
    uint8_t buf[2] = {};
    ReadOnlySpan<uint8_t> span(buf, 2);
    EXPECT_THROW(BinaryPrimitives::ReadInt32LittleEndian(span), std::out_of_range);
}
