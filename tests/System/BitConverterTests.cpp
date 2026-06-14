// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <vector>
#include "System/BitConverter.hpp"

using System::BitConverter;
using System::bytecs;
using SharpRuntime::ushortcs;
using SharpRuntime::uintcs;
using SharpRuntime::ulongcs;
using SharpRuntime::charcs;

// ---------------------------------------------------------------------------
// IsLittleEndian
// ---------------------------------------------------------------------------

TEST(BitConverterTests, IsLittleEndian_IsTrue) {
    EXPECT_TRUE(BitConverter::IsLittleEndian);
}

// ---------------------------------------------------------------------------
// GetBytes — size checks
// ---------------------------------------------------------------------------

TEST(BitConverterTests, GetBytes_Int16_SizeIsTwo) {
    EXPECT_EQ(BitConverter::GetBytes(static_cast<SharpRuntime::shortcs>(0)).size(), 2u);
}

TEST(BitConverterTests, GetBytes_Int32_SizeIsFour) {
    EXPECT_EQ(BitConverter::GetBytes(static_cast<SharpRuntime::intcs>(0)).size(), 4u);
}

TEST(BitConverterTests, GetBytes_Int64_SizeIsEight) {
    EXPECT_EQ(BitConverter::GetBytes(static_cast<SharpRuntime::longcs>(0)).size(), 8u);
}

TEST(BitConverterTests, GetBytes_Float_SizeIsFour) {
    EXPECT_EQ(BitConverter::GetBytes(0.0f).size(), 4u);
}

TEST(BitConverterTests, GetBytes_Double_SizeIsEight) {
    EXPECT_EQ(BitConverter::GetBytes(0.0).size(), 8u);
}

TEST(BitConverterTests, GetBytes_Bool_SizeIsOne) {
    EXPECT_EQ(BitConverter::GetBytes(true).size(), 1u);
}

// ---------------------------------------------------------------------------
// GetBytes(bool) — known values
// ---------------------------------------------------------------------------

TEST(BitConverterTests, GetBytes_BoolTrue_IsOne) {
    EXPECT_EQ(BitConverter::GetBytes(true)[0], 1u);
}

TEST(BitConverterTests, GetBytes_BoolFalse_IsZero) {
    EXPECT_EQ(BitConverter::GetBytes(false)[0], 0u);
}

// ---------------------------------------------------------------------------
// GetBytes — little-endian known values
// ---------------------------------------------------------------------------

TEST(BitConverterTests, GetBytes_Int32_One_LittleEndian) {
    auto b = BitConverter::GetBytes(static_cast<SharpRuntime::intcs>(1));
    EXPECT_EQ(b[0], 1u);
    EXPECT_EQ(b[1], 0u);
    EXPECT_EQ(b[2], 0u);
    EXPECT_EQ(b[3], 0u);
}

TEST(BitConverterTests, GetBytes_Int16_256_LittleEndian) {
    auto b = BitConverter::GetBytes(static_cast<SharpRuntime::shortcs>(256));
    EXPECT_EQ(b[0], 0u);
    EXPECT_EQ(b[1], 1u);
}

// ---------------------------------------------------------------------------
// Round-trip via raw pointer overloads
// ---------------------------------------------------------------------------

TEST(BitConverterTests, RoundTrip_Int16) {
    SharpRuntime::shortcs val = -1234;
    auto b = BitConverter::GetBytes(val);
    EXPECT_EQ(BitConverter::ToInt16(b.data(), 0), val);
}

TEST(BitConverterTests, RoundTrip_Int32) {
    SharpRuntime::intcs val = 0x12345678;
    auto b = BitConverter::GetBytes(val);
    EXPECT_EQ(BitConverter::ToInt32(b.data(), 0), val);
}

TEST(BitConverterTests, RoundTrip_Int64) {
    SharpRuntime::longcs val = 0x0123456789ABCDEFLL;
    auto b = BitConverter::GetBytes(val);
    EXPECT_EQ(BitConverter::ToInt64(b.data(), 0), val);
}

TEST(BitConverterTests, RoundTrip_Float) {
    float val = 3.14f;
    auto b = BitConverter::GetBytes(val);
    EXPECT_FLOAT_EQ(BitConverter::ToSingle(b.data(), 0), val);
}

TEST(BitConverterTests, RoundTrip_Double) {
    double val = 2.718281828;
    auto b = BitConverter::GetBytes(val);
    EXPECT_DOUBLE_EQ(BitConverter::ToDouble(b.data(), 0), val);
}

// ---------------------------------------------------------------------------
// ToBoolean
// ---------------------------------------------------------------------------

TEST(BitConverterTests, ToBoolean_ZeroByte_ReturnsFalse) {
    bytecs buf[] = {0};
    EXPECT_FALSE(BitConverter::ToBoolean(buf, 0));
}

TEST(BitConverterTests, ToBoolean_NonZeroByte_ReturnsTrue) {
    bytecs buf[] = {42};
    EXPECT_TRUE(BitConverter::ToBoolean(buf, 0));
}

// ---------------------------------------------------------------------------
// Vector overloads
// ---------------------------------------------------------------------------

TEST(BitConverterTests, ToInt32_VectorOverload_MatchesRawPtr) {
    SharpRuntime::intcs val = 999;
    auto arr = BitConverter::GetBytes(val);
    std::vector<bytecs> vec(arr.begin(), arr.end());
    EXPECT_EQ(BitConverter::ToInt32(vec, 0), val);
}

TEST(BitConverterTests, ToInt64_VectorOverload_MatchesRawPtr) {
    SharpRuntime::longcs val = -1LL;
    auto arr = BitConverter::GetBytes(val);
    std::vector<bytecs> vec(arr.begin(), arr.end());
    EXPECT_EQ(BitConverter::ToInt64(vec, 0), val);
}

// ---------------------------------------------------------------------------
// ToString (hex formatting)
// ---------------------------------------------------------------------------

TEST(BitConverterTests, ToString_SingleByte_TwoHexChars) {
    bytecs buf[] = {0xAB};
    EXPECT_EQ(BitConverter::ToString(buf, 0, 1), "AB");
}

TEST(BitConverterTests, ToString_TwoBytes_DashSeparated) {
    bytecs buf[] = {0x01, 0x02};
    EXPECT_EQ(BitConverter::ToString(buf, 0, 2), "01-02");
}

TEST(BitConverterTests, ToString_Vector_AllBytes) {
    std::vector<bytecs> v = {0x0F, 0xFF};
    EXPECT_EQ(BitConverter::ToString(v), "0F-FF");
}

// ---------------------------------------------------------------------------
// Bit reinterpretation — DoubleToInt64Bits / Int64BitsToDouble
// ---------------------------------------------------------------------------

TEST(BitConverterTests, DoubleToInt64Bits_RoundTrip) {
    double val = 3.14159265358979;
    SharpRuntime::longcs bits = BitConverter::DoubleToInt64Bits(val);
    double back = BitConverter::Int64BitsToDouble(bits);
    EXPECT_DOUBLE_EQ(back, val);
}

TEST(BitConverterTests, DoubleToInt64Bits_Zero) {
    EXPECT_EQ(BitConverter::DoubleToInt64Bits(0.0), 0LL);
}

TEST(BitConverterTests, DoubleToInt64Bits_One) {
    // IEEE 754: 1.0 = 0x3FF0000000000000
    EXPECT_EQ(BitConverter::DoubleToInt64Bits(1.0), static_cast<SharpRuntime::longcs>(0x3FF0000000000000LL));
}

TEST(BitConverterTests, Int64BitsToDouble_One) {
    EXPECT_DOUBLE_EQ(BitConverter::Int64BitsToDouble(0x3FF0000000000000LL), 1.0);
}

// ---------------------------------------------------------------------------
// Bit reinterpretation — SingleToInt32Bits / Int32BitsToSingle
// ---------------------------------------------------------------------------

TEST(BitConverterTests, SingleToInt32Bits_RoundTrip) {
    float val = 2.5f;
    SharpRuntime::intcs bits = BitConverter::SingleToInt32Bits(val);
    float back = BitConverter::Int32BitsToSingle(bits);
    EXPECT_FLOAT_EQ(back, val);
}

TEST(BitConverterTests, SingleToInt32Bits_One) {
    // IEEE 754: 1.0f = 0x3F800000
    EXPECT_EQ(BitConverter::SingleToInt32Bits(1.0f), static_cast<SharpRuntime::intcs>(0x3F800000));
}

TEST(BitConverterTests, Int32BitsToSingle_One) {
    EXPECT_FLOAT_EQ(BitConverter::Int32BitsToSingle(0x3F800000), 1.0f);
}

// ---------------------------------------------------------------------------
// GetBytes — unsigned and char variants
// ---------------------------------------------------------------------------

TEST(BitConverterTests, GetBytes_Char_SizeIsTwo) {
    EXPECT_EQ(BitConverter::GetBytes(charcs(u'A')).size(), 2u);
}

TEST(BitConverterTests, GetBytes_UInt16_RoundTrip) {
    ushortcs val = 0xABCDu;
    auto b = BitConverter::GetBytes(val);
    EXPECT_EQ(BitConverter::ToUInt16(b.data(), 0), val);
}

TEST(BitConverterTests, GetBytes_UInt32_RoundTrip) {
    uintcs val = 0xDEADBEEFu;
    auto b = BitConverter::GetBytes(val);
    EXPECT_EQ(BitConverter::ToUInt32(b.data(), 0), val);
}

TEST(BitConverterTests, GetBytes_UInt64_RoundTrip) {
    ulongcs val = 0xCAFEBABEDEADBEEFull;
    auto b = BitConverter::GetBytes(val);
    EXPECT_EQ(BitConverter::ToUInt64(b.data(), 0), val);
}

TEST(BitConverterTests, ToChar_RoundTrip) {
    charcs c = u'é'; // é
    auto b = BitConverter::GetBytes(c);
    EXPECT_EQ(BitConverter::ToChar(b.data(), 0), c);
}

TEST(BitConverterTests, ToUInt32_VectorOverload) {
    uintcs val = 42u;
    auto arr = BitConverter::GetBytes(val);
    std::vector<bytecs> v(arr.begin(), arr.end());
    EXPECT_EQ(BitConverter::ToUInt32(v, 0), val);
}

TEST(BitConverterTests, ToUInt64_VectorOverload) {
    ulongcs val = 0xFFFFFFFFFFFFFFFFull;
    auto arr = BitConverter::GetBytes(val);
    std::vector<bytecs> v(arr.begin(), arr.end());
    EXPECT_EQ(BitConverter::ToUInt64(v, 0), val);
}

// ---------------------------------------------------------------------------
// Unsigned bit reinterpretation
// ---------------------------------------------------------------------------

TEST(BitConverterTests, DoubleToUInt64Bits_RoundTrip) {
    double val = 3.14;
    ulongcs bits = BitConverter::DoubleToUInt64Bits(val);
    EXPECT_DOUBLE_EQ(BitConverter::UInt64BitsToDouble(bits), val);
}

TEST(BitConverterTests, DoubleToUInt64Bits_One) {
    EXPECT_EQ(BitConverter::DoubleToUInt64Bits(1.0), 0x3FF0000000000000ull);
}

TEST(BitConverterTests, SingleToUInt32Bits_RoundTrip) {
    float val = 2.5f;
    uintcs bits = BitConverter::SingleToUInt32Bits(val);
    EXPECT_FLOAT_EQ(BitConverter::UInt32BitsToSingle(bits), val);
}

TEST(BitConverterTests, SingleToUInt32Bits_One) {
    EXPECT_EQ(BitConverter::SingleToUInt32Bits(1.0f), 0x3F800000u);
}

// ---------------------------------------------------------------------------
// ToString(vector, startIndex)
// ---------------------------------------------------------------------------

TEST(BitConverterTests, ToString_VectorStartIndex_SkipsFirst) {
    std::vector<bytecs> v = {0x01, 0x02, 0x03};
    EXPECT_EQ(BitConverter::ToString(v, 1), "02-03");
}

TEST(BitConverterTests, ToString_VectorStartIndex_Zero_MatchesFull) {
    std::vector<bytecs> v = {0xAA, 0xBB};
    EXPECT_EQ(BitConverter::ToString(v, 0), BitConverter::ToString(v));
}
