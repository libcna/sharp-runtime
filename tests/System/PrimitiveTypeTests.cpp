// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>

#include "System/Int32.hpp"
#include "System/Int64.hpp"
#include "System/UInt32.hpp"

using System::Int32;
using System::Int64;
using System::UInt32;

// ── Int32 ────────────────────────────────────────────────────────────────────

TEST(Int32Tests, MaxMinValues) {
    EXPECT_EQ(Int32::MaxValue,  2147483647);
    EXPECT_EQ(Int32::MinValue, -2147483648);
}

TEST(Int32Tests, ParseValid) {
    EXPECT_EQ(Int32::Parse("0"),           0);
    EXPECT_EQ(Int32::Parse("42"),         42);
    EXPECT_EQ(Int32::Parse("-1"),         -1);
    EXPECT_EQ(Int32::Parse("2147483647"), Int32::MaxValue);
    EXPECT_EQ(Int32::Parse("-2147483648"), Int32::MinValue);
}

TEST(Int32Tests, ParseInvalidThrows) {
    EXPECT_THROW(Int32::Parse("abc"),          std::invalid_argument);
    EXPECT_THROW(Int32::Parse(""),             std::invalid_argument);
    EXPECT_THROW(Int32::Parse("99999999999"),  std::invalid_argument);
}

TEST(Int32Tests, TryParseSuccess) {
    SharpRuntime::intcs v = 0;
    EXPECT_TRUE(Int32::TryParse("123", v));
    EXPECT_EQ(v, 123);

    EXPECT_TRUE(Int32::TryParse("-2147483648", v));
    EXPECT_EQ(v, Int32::MinValue);
}

TEST(Int32Tests, TryParseFailure) {
    SharpRuntime::intcs v = 99;
    EXPECT_FALSE(Int32::TryParse("nope", v));
    EXPECT_EQ(v, 0);
}

TEST(Int32Tests, ToString) {
    EXPECT_EQ(Int32::ToString(0),   "0");
    EXPECT_EQ(Int32::ToString(-1),  "-1");
    EXPECT_EQ(Int32::ToString(2147483647), "2147483647");
}

// ── Int64 ────────────────────────────────────────────────────────────────────

TEST(Int64Tests, MaxMinValues) {
    EXPECT_EQ(Int64::MaxValue,  9223372036854775807LL);
    EXPECT_EQ(Int64::MinValue, -9223372036854775807LL - 1LL);
}

TEST(Int64Tests, ParseValid) {
    EXPECT_EQ(Int64::Parse("0"),                    0LL);
    EXPECT_EQ(Int64::Parse("9223372036854775807"),  Int64::MaxValue);
    EXPECT_EQ(Int64::Parse("-9223372036854775808"), Int64::MinValue);
    EXPECT_EQ(Int64::Parse("-100"),                -100LL);
}

TEST(Int64Tests, ParseInvalidThrows) {
    EXPECT_THROW(Int64::Parse("xyz"),                   std::invalid_argument);
    EXPECT_THROW(Int64::Parse(""),                      std::invalid_argument);
    EXPECT_THROW(Int64::Parse("99999999999999999999"),  std::invalid_argument);
}

TEST(Int64Tests, TryParseSuccess) {
    SharpRuntime::longcs v = 0;
    EXPECT_TRUE(Int64::TryParse("9223372036854775807", v));
    EXPECT_EQ(v, Int64::MaxValue);
}

TEST(Int64Tests, TryParseFailure) {
    SharpRuntime::longcs v = 7;
    EXPECT_FALSE(Int64::TryParse("bad", v));
    EXPECT_EQ(v, 0);
}

TEST(Int64Tests, ToString) {
    EXPECT_EQ(Int64::ToString(0LL),                   "0");
    EXPECT_EQ(Int64::ToString(9223372036854775807LL), "9223372036854775807");
    EXPECT_EQ(Int64::ToString(-1LL),                  "-1");
}

// ── UInt32 ───────────────────────────────────────────────────────────────────

TEST(UInt32Tests, MaxMinValues) {
    EXPECT_EQ(UInt32::MaxValue, 4294967295U);
    EXPECT_EQ(UInt32::MinValue, 0U);
}

TEST(UInt32Tests, ParseValid) {
    EXPECT_EQ(UInt32::Parse("0"),          0U);
    EXPECT_EQ(UInt32::Parse("4294967295"), UInt32::MaxValue);
    EXPECT_EQ(UInt32::Parse("100"),        100U);
}

TEST(UInt32Tests, ParseInvalidThrows) {
    EXPECT_THROW(UInt32::Parse("abc"),          std::invalid_argument);
    EXPECT_THROW(UInt32::Parse(""),             std::invalid_argument);
    EXPECT_THROW(UInt32::Parse("99999999999"),  std::invalid_argument);
}

TEST(UInt32Tests, TryParseSuccess) {
    SharpRuntime::uintcs v = 0;
    EXPECT_TRUE(UInt32::TryParse("4294967295", v));
    EXPECT_EQ(v, UInt32::MaxValue);
}

TEST(UInt32Tests, TryParseFailure) {
    SharpRuntime::uintcs v = 5;
    EXPECT_FALSE(UInt32::TryParse("nope", v));
    EXPECT_EQ(v, 0U);
}

TEST(UInt32Tests, ToString) {
    EXPECT_EQ(UInt32::ToString(0U),          "0");
    EXPECT_EQ(UInt32::ToString(4294967295U), "4294967295");
}

// ---------------------------------------------------------------------------
// Int32::ToString(format)
// ---------------------------------------------------------------------------
TEST(Int32Tests, ToString_Hex_Uppercase) { EXPECT_EQ(Int32::ToString(255, std::string("X")), "FF"); }
TEST(Int32Tests, ToString_Hex_Padded)    { EXPECT_EQ(Int32::ToString(255, std::string("X4")), "00FF"); }
TEST(Int32Tests, ToString_Hex_Lower)     { EXPECT_EQ(Int32::ToString(255, std::string("x")), "ff"); }
TEST(Int32Tests, ToString_D_NoWidth)     { EXPECT_EQ(Int32::ToString(42, std::string("D")), "42"); }
TEST(Int32Tests, ToString_D_Padded)      { EXPECT_EQ(Int32::ToString(7, std::string("D3")), "007"); }
TEST(Int32Tests, ToString_D_Negative)    { EXPECT_EQ(Int32::ToString(-5, std::string("D3")), "-005"); }
TEST(Int32Tests, ToString_G)             { EXPECT_EQ(Int32::ToString(99, std::string("G")), "99"); }
TEST(Int32Tests, ToString_B_Basic)       { EXPECT_EQ(Int32::ToString(5, std::string("B")), "101"); }
TEST(Int32Tests, ToString_B_Padded)      { EXPECT_EQ(Int32::ToString(5, std::string("B8")), "00000101"); }

// ---------------------------------------------------------------------------
// Int64::ToString(format)
// ---------------------------------------------------------------------------
TEST(Int64Tests, ToString_Hex) { EXPECT_EQ(Int64::ToString(255LL, std::string("X")), "FF"); }
TEST(Int64Tests, ToString_D_Padded) { EXPECT_EQ(Int64::ToString(7LL, std::string("D5")), "00007"); }
