// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/Convert.hpp"
#include "System/OverflowException.hpp"
#include "System/FormatException.hpp"

using System::Convert;
using SharpRuntime::intcs;
using SharpRuntime::longcs;
using SharpRuntime::shortcs;
using SharpRuntime::bytecs;
using SharpRuntime::ushortcs;
using SharpRuntime::sbytecs;
using SharpRuntime::uintcs;
using SharpRuntime::ulongcs;

// ---------------------------------------------------------------------------
// ToBoolean — new overloads
// ---------------------------------------------------------------------------

TEST(ConvertTests, ToBoolean_FromBool_True)   { EXPECT_TRUE(Convert::ToBoolean(true)); }
TEST(ConvertTests, ToBoolean_FromBool_False)  { EXPECT_FALSE(Convert::ToBoolean(false)); }
TEST(ConvertTests, ToBoolean_FromShort_NonZero) { EXPECT_TRUE(Convert::ToBoolean(shortcs(5))); }
TEST(ConvertTests, ToBoolean_FromShort_Zero)    { EXPECT_FALSE(Convert::ToBoolean(shortcs(0))); }
TEST(ConvertTests, ToBoolean_FromByte_NonZero)  { EXPECT_TRUE(Convert::ToBoolean(bytecs(1))); }
TEST(ConvertTests, ToBoolean_FromByte_Zero)     { EXPECT_FALSE(Convert::ToBoolean(bytecs(0))); }

// ---------------------------------------------------------------------------
// ToChar
// ---------------------------------------------------------------------------

TEST(ConvertTests, ToChar_FromChar_Identity)   { EXPECT_EQ(Convert::ToChar('Z'), 'Z'); }
TEST(ConvertTests, ToChar_FromInt_65)          { EXPECT_EQ(Convert::ToChar(intcs(65)), 'A'); }
TEST(ConvertTests, ToChar_FromLong_Valid)       { EXPECT_EQ(Convert::ToChar(longcs(97)), 'a'); }
TEST(ConvertTests, ToChar_FromLong_Overflow)    { EXPECT_THROW(Convert::ToChar(longcs(300)), System::OverflowException); }
TEST(ConvertTests, ToChar_FromString_Single)    { EXPECT_EQ(Convert::ToChar(std::string("X")), 'X'); }
TEST(ConvertTests, ToChar_FromString_Empty_Throws)
    { EXPECT_THROW(Convert::ToChar(std::string("")), System::FormatException); }
TEST(ConvertTests, ToChar_FromString_Multi_Throws)
    { EXPECT_THROW(Convert::ToChar(std::string("AB")), System::FormatException); }

// ---------------------------------------------------------------------------
// ToByte — new overloads
// ---------------------------------------------------------------------------

TEST(ConvertTests, ToByte_FromByte_Identity)   { EXPECT_EQ(Convert::ToByte(bytecs(200)), bytecs(200)); }
TEST(ConvertTests, ToByte_FromShort_Valid)      { EXPECT_EQ(Convert::ToByte(shortcs(100)), bytecs(100)); }
TEST(ConvertTests, ToByte_FromShort_Overflow)   { EXPECT_THROW(Convert::ToByte(shortcs(300)), System::OverflowException); }
TEST(ConvertTests, ToByte_FromShort_Negative)   { EXPECT_THROW(Convert::ToByte(shortcs(-1)), System::OverflowException); }
TEST(ConvertTests, ToByte_FromUInt32_Valid)     { EXPECT_EQ(Convert::ToByte(uint32_t(255)), bytecs(255)); }
TEST(ConvertTests, ToByte_FromUInt32_Overflow)  { EXPECT_THROW(Convert::ToByte(uint32_t(256)), System::OverflowException); }

// ---------------------------------------------------------------------------
// ToInt16 — new overload
// ---------------------------------------------------------------------------

TEST(ConvertTests, ToInt16_FromShort_Identity)  { EXPECT_EQ(Convert::ToInt16(shortcs(1000)), shortcs(1000)); }

// ---------------------------------------------------------------------------
// ToInt32 — new overload
// ---------------------------------------------------------------------------

TEST(ConvertTests, ToInt32_FromShort)           { EXPECT_EQ(Convert::ToInt32(shortcs(-5)), intcs(-5)); }

// ---------------------------------------------------------------------------
// ToInt64 — new overloads
// ---------------------------------------------------------------------------

TEST(ConvertTests, ToInt64_FromShort)           { EXPECT_EQ(Convert::ToInt64(shortcs(-32768)), longcs(-32768)); }
TEST(ConvertTests, ToInt64_FromByte)            { EXPECT_EQ(Convert::ToInt64(bytecs(255)), longcs(255)); }
TEST(ConvertTests, ToInt64_FromUInt32)          { EXPECT_EQ(Convert::ToInt64(uint32_t(4294967295u)), longcs(4294967295LL)); }
TEST(ConvertTests, ToInt64_FromUInt64_Valid)    { EXPECT_EQ(Convert::ToInt64(uint64_t(100)), longcs(100)); }
TEST(ConvertTests, ToInt64_FromUInt64_Overflow) {
    EXPECT_THROW(Convert::ToInt64(uint64_t(9999999999999999999ULL)), System::OverflowException);
}
TEST(ConvertTests, ToInt64_Identity)            { EXPECT_EQ(Convert::ToInt64(longcs(-7)), longcs(-7)); }

// ---------------------------------------------------------------------------
// ToDouble — new overloads
// ---------------------------------------------------------------------------

TEST(ConvertTests, ToDouble_FromShort)          { EXPECT_DOUBLE_EQ(Convert::ToDouble(shortcs(-3)), -3.0); }
TEST(ConvertTests, ToDouble_FromByte)           { EXPECT_DOUBLE_EQ(Convert::ToDouble(bytecs(128)), 128.0); }
TEST(ConvertTests, ToDouble_Identity)           { EXPECT_DOUBLE_EQ(Convert::ToDouble(3.14), 3.14); }

// ---------------------------------------------------------------------------
// ToSingle — new overloads
// ---------------------------------------------------------------------------

TEST(ConvertTests, ToSingle_FromShort)          { EXPECT_FLOAT_EQ(Convert::ToSingle(shortcs(7)), 7.0f); }
TEST(ConvertTests, ToSingle_FromByte)           { EXPECT_FLOAT_EQ(Convert::ToSingle(bytecs(10)), 10.0f); }
TEST(ConvertTests, ToSingle_Identity)           { EXPECT_FLOAT_EQ(Convert::ToSingle(1.5f), 1.5f); }

// ---------------------------------------------------------------------------
// ToUInt32 — new overloads
// ---------------------------------------------------------------------------

TEST(ConvertTests, ToUInt32_FromBool_True)      { EXPECT_EQ(Convert::ToUInt32(true), 1u); }
TEST(ConvertTests, ToUInt32_FromBool_False)     { EXPECT_EQ(Convert::ToUInt32(false), 0u); }
TEST(ConvertTests, ToUInt32_FromByte)           { EXPECT_EQ(Convert::ToUInt32(bytecs(200)), 200u); }
TEST(ConvertTests, ToUInt32_FromDouble_Valid)   { EXPECT_EQ(Convert::ToUInt32(255.9), 255u); }
TEST(ConvertTests, ToUInt32_FromDouble_Negative) {
    EXPECT_THROW(Convert::ToUInt32(-1.0), System::OverflowException);
}

// ---------------------------------------------------------------------------
// ToUInt64 — new overloads
// ---------------------------------------------------------------------------

TEST(ConvertTests, ToUInt64_FromBool_True)      { EXPECT_EQ(Convert::ToUInt64(true), 1ull); }
TEST(ConvertTests, ToUInt64_FromBool_False)     { EXPECT_EQ(Convert::ToUInt64(false), 0ull); }
TEST(ConvertTests, ToUInt64_FromByte)           { EXPECT_EQ(Convert::ToUInt64(bytecs(255)), 255ull); }
TEST(ConvertTests, ToUInt64_FromDouble_Valid)   { EXPECT_EQ(Convert::ToUInt64(1000.0), 1000ull); }
TEST(ConvertTests, ToUInt64_FromDouble_Negative) {
    EXPECT_THROW(Convert::ToUInt64(-1.0), System::OverflowException);
}

// ---------------------------------------------------------------------------
// ToString — new overloads
// ---------------------------------------------------------------------------

TEST(ConvertTests, ToString_FromShort)          { EXPECT_EQ(Convert::ToString(shortcs(-32768)), "-32768"); }
TEST(ConvertTests, ToString_FromUInt32)         { EXPECT_EQ(Convert::ToString(uint32_t(4294967295u)), "4294967295"); }
TEST(ConvertTests, ToString_FromUInt64)         { EXPECT_EQ(Convert::ToString(uint64_t(18446744073709551615ull)), "18446744073709551615"); }

// ---------------------------------------------------------------------------
// ToHexStringLower
//
// Regression coverage for a code-audit finding (ticket 248): this method was previously
// named ToHexStringUpper despite producing uppercase output -- a name that doesn't exist in
// real .NET's Convert class at all -- while the real .NET name ToHexString (which is
// documented, and verified against Convert.cs, to produce UPPERCASE) was attached to a
// lowercase implementation. Renamed to match .NET's actual API surface: ToHexString is now
// uppercase (tested in ConvertTests.cpp) and this method (ToHexStringLower) is lowercase.
// ---------------------------------------------------------------------------

TEST(ConvertTests, ToHexStringLower_Empty)      { EXPECT_EQ(Convert::ToHexStringLower({}), ""); }
TEST(ConvertTests, ToHexStringLower_SingleByte) { EXPECT_EQ(Convert::ToHexStringLower({0xFF}), "ff"); }
TEST(ConvertTests, ToHexStringLower_Multi)      { EXPECT_EQ(Convert::ToHexStringLower({0xAB, 0xCD}), "abcd"); }
TEST(ConvertTests, ToHexStringLower_AllZeros)   { EXPECT_EQ(Convert::ToHexStringLower({0x00, 0x00}), "0000"); }

TEST(ConvertTests, ToHexStringLower_RoundTripWithFromHex) {
    std::vector<uint8_t> original{0x01, 0xAB, 0xCD, 0xEF};
    auto hex = Convert::ToHexStringLower(original);
    EXPECT_EQ(hex, "01abcdef");
    auto back = Convert::FromHexString(hex);
    EXPECT_EQ(original, back);
}

// ---------------------------------------------------------------------------
// ToBoolean — uint/ulong/ushort/sbyte overloads
// ---------------------------------------------------------------------------

TEST(ConvertTests, ToBoolean_FromUInt_Zero)    { EXPECT_FALSE(Convert::ToBoolean(uintcs(0))); }
TEST(ConvertTests, ToBoolean_FromUInt_NonZero) { EXPECT_TRUE(Convert::ToBoolean(uintcs(42))); }
TEST(ConvertTests, ToBoolean_FromULong_Zero)   { EXPECT_FALSE(Convert::ToBoolean(ulongcs(0))); }
TEST(ConvertTests, ToBoolean_FromULong_NonZero){ EXPECT_TRUE(Convert::ToBoolean(ulongcs(1))); }
TEST(ConvertTests, ToBoolean_FromUShort_Zero)  { EXPECT_FALSE(Convert::ToBoolean(ushortcs(0))); }
TEST(ConvertTests, ToBoolean_FromSByte_NonZero){ EXPECT_TRUE(Convert::ToBoolean(sbytecs(-1))); }

// ---------------------------------------------------------------------------
// ToUInt16
// ---------------------------------------------------------------------------

TEST(ConvertTests, ToUInt16_Identity)           { EXPECT_EQ(Convert::ToUInt16(ushortcs(1000)), 1000u); }
TEST(ConvertTests, ToUInt16_FromByte)           { EXPECT_EQ(Convert::ToUInt16(bytecs(255)), 255u); }
TEST(ConvertTests, ToUInt16_FromBool_True)      { EXPECT_EQ(Convert::ToUInt16(true), 1u); }
TEST(ConvertTests, ToUInt16_FromBool_False)     { EXPECT_EQ(Convert::ToUInt16(false), 0u); }
TEST(ConvertTests, ToUInt16_FromInt_Valid)      { EXPECT_EQ(Convert::ToUInt16(intcs(65535)), ushortcs(65535)); }
TEST(ConvertTests, ToUInt16_FromInt_Overflow)   { EXPECT_THROW(Convert::ToUInt16(intcs(65536)), System::OverflowException); }
TEST(ConvertTests, ToUInt16_FromInt_Negative)   { EXPECT_THROW(Convert::ToUInt16(intcs(-1)),  System::OverflowException); }
TEST(ConvertTests, ToUInt16_FromShort_Valid)    { EXPECT_EQ(Convert::ToUInt16(shortcs(100)), 100u); }
TEST(ConvertTests, ToUInt16_FromShort_Negative) { EXPECT_THROW(Convert::ToUInt16(shortcs(-1)), System::OverflowException); }
TEST(ConvertTests, ToUInt16_FromLong_Valid)     { EXPECT_EQ(Convert::ToUInt16(longcs(0)), 0u); }
TEST(ConvertTests, ToUInt16_FromLong_Overflow)  { EXPECT_THROW(Convert::ToUInt16(longcs(70000)), System::OverflowException); }
TEST(ConvertTests, ToUInt16_FromDouble_Valid)   { EXPECT_EQ(Convert::ToUInt16(1.9), ushortcs(1)); }
TEST(ConvertTests, ToUInt16_FromDouble_Overflow){ EXPECT_THROW(Convert::ToUInt16(-1.0), System::OverflowException); }
TEST(ConvertTests, ToUInt16_FromString_Valid)   { EXPECT_EQ(Convert::ToUInt16(std::string("1000")), 1000u); }
TEST(ConvertTests, ToUInt16_FromString_Overflow){ EXPECT_THROW(Convert::ToUInt16(std::string("65536")), System::OverflowException); }

// ---------------------------------------------------------------------------
// ToSByte
// ---------------------------------------------------------------------------

TEST(ConvertTests, ToSByte_Identity)            { EXPECT_EQ(Convert::ToSByte(sbytecs(-5)), sbytecs(-5)); }
TEST(ConvertTests, ToSByte_FromBool_True)       { EXPECT_EQ(Convert::ToSByte(true), sbytecs(1)); }
TEST(ConvertTests, ToSByte_FromBool_False)      { EXPECT_EQ(Convert::ToSByte(false), sbytecs(0)); }
TEST(ConvertTests, ToSByte_FromByte_Valid)      { EXPECT_EQ(Convert::ToSByte(bytecs(127)), sbytecs(127)); }
TEST(ConvertTests, ToSByte_FromByte_Overflow)   { EXPECT_THROW(Convert::ToSByte(bytecs(128)), System::OverflowException); }
TEST(ConvertTests, ToSByte_FromShort_Valid)     { EXPECT_EQ(Convert::ToSByte(shortcs(-128)), sbytecs(-128)); }
TEST(ConvertTests, ToSByte_FromShort_Overflow)  { EXPECT_THROW(Convert::ToSByte(shortcs(128)), System::OverflowException); }
TEST(ConvertTests, ToSByte_FromInt_Valid)       { EXPECT_EQ(Convert::ToSByte(intcs(0)), sbytecs(0)); }
TEST(ConvertTests, ToSByte_FromInt_Overflow)    { EXPECT_THROW(Convert::ToSByte(intcs(200)), System::OverflowException); }
TEST(ConvertTests, ToSByte_FromLong_Overflow)   { EXPECT_THROW(Convert::ToSByte(longcs(-200)), System::OverflowException); }
TEST(ConvertTests, ToSByte_FromDouble_Valid)    { EXPECT_EQ(Convert::ToSByte(-1.0), sbytecs(-1)); }
TEST(ConvertTests, ToSByte_FromDouble_Overflow) { EXPECT_THROW(Convert::ToSByte(200.0), System::OverflowException); }
TEST(ConvertTests, ToSByte_FromString_Valid)    { EXPECT_EQ(Convert::ToSByte(std::string("-1")), sbytecs(-1)); }
TEST(ConvertTests, ToSByte_FromString_Overflow) { EXPECT_THROW(Convert::ToSByte(std::string("200")), System::OverflowException); }
