// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/FormatException.hpp"
#include "System/OverflowException.hpp"
#include "System/UInt64.hpp"

using System::UInt64;

TEST(UInt64Test, MaxValue) {
    EXPECT_EQ(UInt64::MaxValue, 18446744073709551615ULL);
}

TEST(UInt64Test, MinValue) {
    EXPECT_EQ(UInt64::MinValue, 0ULL);
}

TEST(UInt64Test, Clamp) {
    EXPECT_EQ(UInt64::Clamp(150ULL, 100ULL, 200ULL), 150ULL);
    EXPECT_EQ(UInt64::Clamp(50ULL,  100ULL, 200ULL), 100ULL);
    EXPECT_EQ(UInt64::Clamp(250ULL, 100ULL, 200ULL), 200ULL);
}
// SR-AUD-022 (#1846): an inverted interval must throw, not reach std::clamp UB.
TEST(UInt64Test, Clamp_MinGreaterThanMax_Throws) {
    EXPECT_THROW(UInt64::Clamp(5ULL, 10ULL, 1ULL), System::ArgumentException);
    EXPECT_EQ(UInt64::Clamp(5ULL, 7ULL, 7ULL), 7ULL); // equal bound is valid
}
// SR-AUD-023 (#1845): integral binary ToString("B"/"b").
TEST(UInt64Test, ToString_Binary) {
    EXPECT_EQ(UInt64::ToString(5ULL,  std::string("B")), "101");
    EXPECT_EQ(UInt64::ToString(5ULL,  std::string("b")), "101");
    EXPECT_EQ(UInt64::ToString(0ULL,  std::string("B")), "0");
    EXPECT_EQ(UInt64::ToString(18446744073709551615ULL, std::string("B")), std::string(64, '1'));
    EXPECT_EQ(UInt64::ToString(5ULL,  std::string("B64")), std::string(61, '0') + "101");
}

TEST(UInt64Test, ParseBasic) {
    EXPECT_EQ(UInt64::Parse("12345"), 12345ULL);
}

TEST(UInt64Test, ParseZero) {
    EXPECT_EQ(UInt64::Parse("0"), 0ULL);
}

TEST(UInt64Test, ParseLarge) {
    EXPECT_EQ(UInt64::Parse("18446744073709551615"), 18446744073709551615ULL);
}

TEST(UInt64Test, ParseInvalidThrows) {
    EXPECT_THROW(UInt64::Parse("abc"), System::FormatException);
}

TEST(UInt64Test, TryParseSuccess) {
    uint64_t result = 0;
    EXPECT_TRUE(UInt64::TryParse("999", result));
    EXPECT_EQ(result, 999ULL);
}

TEST(UInt64Test, TryParseFailure) {
    uint64_t result = 0;
    EXPECT_FALSE(UInt64::TryParse("xyz", result));
}

TEST(UInt64Test, Parse_NegativeSign_ThrowsOverflowException) {
    // Unsigned Parse must reject any leading '-' (even "-0") rather than
    // silently wrapping to a huge value.
    EXPECT_THROW(UInt64::Parse("-5"), System::OverflowException);
    EXPECT_THROW(UInt64::Parse("-0"), System::OverflowException);
}

TEST(UInt64Test, Parse_TrailingGarbage_ThrowsFormatException) {
    EXPECT_THROW(UInt64::Parse("123abc"), System::FormatException);
}

TEST(UInt64Test, Parse_OverflowsRange_ThrowsOverflowException) {
    EXPECT_THROW(UInt64::Parse("99999999999999999999"), System::OverflowException);
}

TEST(UInt64Test, ToStringDecimal) {
    EXPECT_EQ(UInt64::ToString(1000ULL), "1000");
}

TEST(UInt64Test, ToStringHex) {
    EXPECT_EQ(UInt64::ToString(255ULL, "X"), "FF");
}

TEST(UInt64Test, ToString_MalformedWidth_ThrowsFormatException) {
    EXPECT_THROW(UInt64::ToString(5ULL, "Xz"), System::FormatException);
    EXPECT_THROW(UInt64::ToString(5ULL, "X99999999999999999999"), System::FormatException);
}

TEST(UInt64Test, ToStringPadded) {
    EXPECT_EQ(UInt64::ToString(10ULL, "D4"), "0010");
}

TEST(UInt64Test, DivRem_WithRemainder) {
    auto [q, r] = UInt64::DivRem(10ULL, 3ULL);
    EXPECT_EQ(q, 3ULL); EXPECT_EQ(r, 1ULL);
}
TEST(UInt64Test, DivRem_ByZero_ThrowsDivideByZeroException) {
    EXPECT_THROW(UInt64::DivRem(10ULL, 0ULL), System::DivideByZeroException);
}
