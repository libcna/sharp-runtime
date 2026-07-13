// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Regression tests for the NumberStyles.AllowThousands/AllowDecimalPoint/AllowCurrencySymbol/
// AllowParentheses/AllowTrailingSign extension to include/System/detail/IntegerNumberStylesParser.hpp
// (NEXT.md 2026-07-13 task: extend ticket 1717's NumberStyles-aware integer parser to also
// support NumberStyles.Number and NumberStyles.Currency, not just .Integer/.HexNumber).
#include <gtest/gtest.h>
#include "System/Byte.hpp"
#include "System/Int16.hpp"
#include "System/Int32.hpp"
#include "System/Int64.hpp"
#include "System/SByte.hpp"
#include "System/UInt16.hpp"
#include "System/UInt32.hpp"
#include "System/UInt64.hpp"
#include "System/FormatException.hpp"
#include "System/OverflowException.hpp"

using System::Byte;
using System::Int16;
using System::Int32;
using System::Int64;
using System::SByte;
using System::UInt16;
using System::UInt32;
using System::UInt64;
using System::Globalization::NumberStyles;

namespace {
// UTF-8 encoding of U+00A4 (international currency sign), matching
// NumberFormatInfo.InvariantInfo.CurrencySymbol -- see IntegerNumberStylesParser's doc-comment.
constexpr const char* kCurrency = "\xC2\xA4";
} // namespace

// -----------------------------------------------------------------------
// AllowThousands
// -----------------------------------------------------------------------

TEST(NumberStylesExtendedTests, AllowThousands_SingleGroupSeparator) {
    EXPECT_EQ(Int32::Parse("1,234", NumberStyles::Number, nullptr), 1234);
}

TEST(NumberStylesExtendedTests, AllowThousands_MultipleGroupSeparators) {
    EXPECT_EQ(Int32::Parse("1,234,567", NumberStyles::Number, nullptr), 1234567);
}

TEST(NumberStylesExtendedTests, AllowThousands_NegativeWithGrouping) {
    EXPECT_EQ(Int32::Parse("-1,234", NumberStyles::Number, nullptr), -1234);
}

TEST(NumberStylesExtendedTests, AllowThousands_LeadingSeparatorIsInvalid) {
    SharpRuntime::intcs out;
    EXPECT_FALSE(Int32::TryParse(",1234", NumberStyles::Number, nullptr, out));
}

TEST(NumberStylesExtendedTests, AllowThousands_NotSetRejectsGroupedInput) {
    // Integer style (the pre-existing default) does NOT include AllowThousands -- confirms the
    // new grammar is gated by the flag, not unconditionally applied.
    SharpRuntime::intcs out;
    EXPECT_FALSE(Int32::TryParse("1,234", NumberStyles::Integer, nullptr, out));
}

// -----------------------------------------------------------------------
// AllowDecimalPoint
// -----------------------------------------------------------------------

TEST(NumberStylesExtendedTests, AllowDecimalPoint_TrailingZerosParsesAsInteger) {
    EXPECT_EQ(Int32::Parse("123.00", NumberStyles::Number, nullptr), 123);
    EXPECT_EQ(Int32::Parse("123.", NumberStyles::Number, nullptr), 123);
}

TEST(NumberStylesExtendedTests, AllowDecimalPoint_NonZeroFraction_ThrowsOverflowNotFormat) {
    // Faithful to real .NET's own quirk: TryNumberBufferToBinaryInteger rejects any nonzero
    // fractional digit with Overflow, not a format failure -- Int32.Parse("123.5",
    // NumberStyles.Number) throws OverflowException in real .NET, not FormatException.
    EXPECT_THROW(Int32::Parse("123.5", NumberStyles::Number, nullptr), System::OverflowException);
    SharpRuntime::intcs out;
    EXPECT_FALSE(Int32::TryParse("123.5", NumberStyles::Number, nullptr, out));
}

TEST(NumberStylesExtendedTests, AllowDecimalPoint_TrailingGarbageTakesPrecedenceOverOverflow) {
    // Format-grammar violations outrank the fractional-overflow quirk (matches real .NET's own
    // documented precedence) -- "123.5x" has trailing garbage, so it's a FormatException.
    EXPECT_THROW(Int32::Parse("123.5x", NumberStyles::Number, nullptr), System::FormatException);
}

TEST(NumberStylesExtendedTests, AllowDecimalPoint_OnlyFractionalDigits) {
    EXPECT_EQ(Int32::Parse(".00", NumberStyles::Number, nullptr), 0);
}

// -----------------------------------------------------------------------
// AllowCurrencySymbol / NumberStyles.Currency
// -----------------------------------------------------------------------

TEST(NumberStylesExtendedTests, Currency_LeadingSymbolWithGroupingAndDecimal) {
    std::string input = std::string(kCurrency) + "1,234.00";
    EXPECT_EQ(Int32::Parse(input, NumberStyles::Currency, nullptr), 1234);
}

TEST(NumberStylesExtendedTests, Currency_TrailingSymbol) {
    std::string input = "1234" + std::string(kCurrency);
    EXPECT_EQ(Int32::Parse(input, NumberStyles::Currency, nullptr), 1234);
}

TEST(NumberStylesExtendedTests, Currency_NegativeWithSymbol) {
    std::string input = "-" + std::string(kCurrency) + "1234";
    EXPECT_EQ(Int32::Parse(input, NumberStyles::Currency, nullptr), -1234);
}

TEST(NumberStylesExtendedTests, Currency_DuplicateSymbolIsInvalid) {
    std::string input = std::string(kCurrency) + "12" + std::string(kCurrency) + "34";
    SharpRuntime::intcs out;
    EXPECT_FALSE(Int32::TryParse(input, NumberStyles::Currency, nullptr, out));
}

// -----------------------------------------------------------------------
// AllowParentheses
// -----------------------------------------------------------------------

TEST(NumberStylesExtendedTests, AllowParentheses_WrapsNegativeValue) {
    EXPECT_EQ(Int32::Parse("(42)", NumberStyles::Integer | NumberStyles::AllowParentheses, nullptr), -42);
}

TEST(NumberStylesExtendedTests, AllowParentheses_UnclosedIsInvalid) {
    SharpRuntime::intcs out;
    EXPECT_FALSE(Int32::TryParse("(42", NumberStyles::Integer | NumberStyles::AllowParentheses, nullptr, out));
}

TEST(NumberStylesExtendedTests, AllowParentheses_UnsignedTypeRejectsParens) {
    // Parens always indicate a negative value; this port rejects that outright for unsigned
    // types rather than importing real .NET's negative-unsigned-throws-Overflow quirk -- see
    // TryParseUnsignedCore's doc-comment.
    SharpRuntime::uintcs out;
    EXPECT_FALSE(UInt32::TryParse("(5)", NumberStyles::Integer | NumberStyles::AllowParentheses, nullptr, out));
}

// -----------------------------------------------------------------------
// AllowTrailingSign
// -----------------------------------------------------------------------

TEST(NumberStylesExtendedTests, AllowTrailingSign_NegativeAfterDigits) {
    EXPECT_EQ(Int32::Parse("42-", NumberStyles::Integer | NumberStyles::AllowTrailingSign, nullptr), -42);
}

TEST(NumberStylesExtendedTests, AllowTrailingSign_PositiveAfterDigits) {
    EXPECT_EQ(Int32::Parse("42+", NumberStyles::Integer | NumberStyles::AllowTrailingSign, nullptr), 42);
}

TEST(NumberStylesExtendedTests, AllowTrailingSign_UnsignedRejectsTrailingMinus) {
    SharpRuntime::uintcs out;
    EXPECT_FALSE(UInt32::TryParse("42-", NumberStyles::Integer | NumberStyles::AllowTrailingSign, nullptr, out));
}

TEST(NumberStylesExtendedTests, AllowTrailingSign_UnsignedAcceptsTrailingPlus) {
    SharpRuntime::uintcs out;
    EXPECT_TRUE(UInt32::TryParse("42+", NumberStyles::Integer | NumberStyles::AllowTrailingSign, nullptr, out));
    EXPECT_EQ(out, 42u);
}

// -----------------------------------------------------------------------
// Coverage across the other 6 integer types (Int16/Int64/SByte/UInt16/UInt64/Byte), spot-checked
// with one representative case each rather than the full matrix (already exercised for Int32).
// -----------------------------------------------------------------------

TEST(NumberStylesExtendedTests, Int16_ThousandsAndDecimal) {
    EXPECT_EQ(Int16::Parse("1,234.00", NumberStyles::Number, nullptr), 1234);
}

TEST(NumberStylesExtendedTests, Int64_ThousandsAndDecimal) {
    EXPECT_EQ(Int64::Parse("1,234,567.00", NumberStyles::Number, nullptr), 1234567);
}

TEST(NumberStylesExtendedTests, SByte_Parentheses) {
    EXPECT_EQ(SByte::Parse("(12)", NumberStyles::Integer | NumberStyles::AllowParentheses, nullptr), -12);
}

TEST(NumberStylesExtendedTests, UInt16_ThousandsAndDecimal) {
    EXPECT_EQ(UInt16::Parse("1,234.00", NumberStyles::Number, nullptr), 1234);
}

TEST(NumberStylesExtendedTests, UInt64_Currency) {
    std::string input = std::string(kCurrency) + "1,234,567.00";
    EXPECT_EQ(UInt64::Parse(input, NumberStyles::Currency, nullptr), 1234567u);
}

TEST(NumberStylesExtendedTests, Byte_ThousandsSeparatorRejectedWhenTooLarge) {
    EXPECT_THROW(Byte::Parse("1,234", NumberStyles::Number, nullptr), System::OverflowException);
}
