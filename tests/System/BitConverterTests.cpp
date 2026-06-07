// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <vector>
#include "System/BitConverter.hpp"

using System::BitConverter;
using System::bytecs;

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
