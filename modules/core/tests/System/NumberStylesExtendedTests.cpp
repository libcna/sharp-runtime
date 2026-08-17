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

// -----------------------------------------------------------------------
// Whitespace interleaved with leading/trailing tokens (fixed 2026-07-14): AllowLeadingWhite/
// AllowTrailingWhite previously only skipped whitespace once, strictly before/after the
// sign/parentheses/currency-symbol matching loops -- real .NET tolerates whitespace BETWEEN a
// matched token and the digits (or end of string) too. Confirmed via source trace against
// Number.Parsing.Common.cs and empirical cross-check against real .NET/Mono.
// -----------------------------------------------------------------------

TEST(NumberStylesExtendedTests, TrailingWhitespace_AfterTrailingSign) {
    EXPECT_EQ(Int32::Parse("123-  ", NumberStyles::Number, nullptr), -123);
    EXPECT_EQ(Int32::Parse("123+  ", NumberStyles::Number, nullptr), 123);
}

TEST(NumberStylesExtendedTests, TrailingWhitespace_AfterClosingParenthesis) {
    EXPECT_EQ(Int32::Parse(" (123)  ", NumberStyles::Currency, nullptr), -123);
}

TEST(NumberStylesExtendedTests, TrailingWhitespace_AfterTrailingCurrencySymbol) {
    std::string input = "123" + std::string(kCurrency) + "  ";
    EXPECT_EQ(Int32::Parse(input, NumberStyles::Currency, nullptr), 123);
}

TEST(NumberStylesExtendedTests, LeadingWhitespace_BetweenCurrencySymbolAndDigits) {
    std::string input = std::string(kCurrency) + "  123";
    EXPECT_EQ(UInt32::Parse(input, NumberStyles::Currency, nullptr), 123u);
    EXPECT_EQ(Int32::Parse(input, NumberStyles::Currency, nullptr), 123);
}

TEST(NumberStylesExtendedTests, TrailingWhitespace_StillRejectedWhenNotAllowed) {
    // AllowLeadingSign | AllowTrailingSign WITHOUT AllowTrailingWhite (NumberStyles::Integer
    // would already bundle AllowTrailingWhite in, so it can't be used here to test this): the
    // fix must not make whitespace tolerance unconditional -- it stays gated by the flag.
    SharpRuntime::intcs out;
    EXPECT_FALSE(Int32::TryParse("123-  ", NumberStyles::AllowLeadingSign | NumberStyles::AllowTrailingSign, nullptr, out));
    EXPECT_TRUE(Int32::TryParse("123-", NumberStyles::AllowLeadingSign | NumberStyles::AllowTrailingSign, nullptr, out));
    EXPECT_EQ(out, -123);
}

// -----------------------------------------------------------------------
// HexNumber leading zeros (fixed 2026-07-14): digitCount previously included leading zeros when
// checking against maxDigits, so a harmlessly zero-padded hex string longer than the type's
// native width incorrectly overflowed even though its significant digit count fit. Confirmed
// against real .NET's TryParseBinaryIntegerHexOrBinaryNumberStyle, which skips leading zeros
// before counting.
// -----------------------------------------------------------------------

TEST(NumberStylesExtendedTests, HexNumber_LeadingZeros_DoNotCountTowardOverflow) {
    EXPECT_EQ(UInt32::Parse("000000001", NumberStyles::HexNumber, nullptr), 1u);
    EXPECT_EQ(UInt32::Parse("00000000FFFFFFFF", NumberStyles::HexNumber, nullptr), 4294967295u);
    EXPECT_EQ(Int32::Parse("00000000FFFFFFFF", NumberStyles::HexNumber, nullptr), -1);
}

TEST(NumberStylesExtendedTests, HexNumber_GenuineOverflow_StillThrows) {
    // Sanity check: 9 significant (non-zero-leading) digits still correctly overflows a 32-bit type.
    EXPECT_THROW(UInt32::Parse("100000000", NumberStyles::HexNumber, nullptr), System::OverflowException);
}

TEST(NumberStylesExtendedTests, HexNumber_AllZeros_ParsesAsZero) {
    EXPECT_EQ(UInt32::Parse("00000000", NumberStyles::HexNumber, nullptr), 0u);
    EXPECT_EQ(UInt32::Parse("0", NumberStyles::HexNumber, nullptr), 0u);
}

// -----------------------------------------------------------------------
// NumberStyles.BinaryNumber / AllowBinarySpecifier (added 2026-07-14): previously defined in
// NumberStyles.hpp but never checked by any Parse/TryParse overload, so it silently fell through
// to decimal parsing instead of throwing FormatException or parsing as binary -- e.g.
// Int32::TryParse("101", NumberStyles::BinaryNumber, ...) used to return true with result 101
// (a decimal reinterpretation), not 5. Mirrors the existing, already-correct HexNumber support.
// -----------------------------------------------------------------------

TEST(NumberStylesExtendedTests, BinaryNumber_Int32_RoundTrips) {
    EXPECT_EQ(Int32::Parse("101", NumberStyles::BinaryNumber, nullptr), 5);
    EXPECT_EQ(Int32::Parse("0", NumberStyles::BinaryNumber, nullptr), 0);
    EXPECT_EQ(Int32::Parse(std::string(32, '1'), NumberStyles::BinaryNumber, nullptr), -1);
    EXPECT_EQ(Int32::Parse(" 101 ", NumberStyles::BinaryNumber, nullptr), 5);
}

TEST(NumberStylesExtendedTests, BinaryNumber_LeadingZeros_DoNotCountTowardOverflow) {
    EXPECT_EQ(UInt32::Parse(std::string(20, '0') + "101", NumberStyles::BinaryNumber, nullptr), 5u);
}

TEST(NumberStylesExtendedTests, BinaryNumber_TooManyDigits_ThrowsOverflow) {
    EXPECT_THROW(UInt32::Parse(std::string(33, '1'), NumberStyles::BinaryNumber, nullptr), System::OverflowException);
    SharpRuntime::uintcs out;
    EXPECT_FALSE(UInt32::TryParse(std::string(33, '1'), NumberStyles::BinaryNumber, nullptr, out));
}

TEST(NumberStylesExtendedTests, BinaryNumber_InvalidDigit_ThrowsFormat) {
    EXPECT_THROW(Int32::Parse("102", NumberStyles::BinaryNumber, nullptr), System::FormatException);
}

TEST(NumberStylesExtendedTests, BinaryNumber_CoverageAcrossOtherTypes) {
    EXPECT_EQ(Int16::Parse("101", NumberStyles::BinaryNumber, nullptr), 5);
    EXPECT_EQ(Int64::Parse("101", NumberStyles::BinaryNumber, nullptr), 5);
    EXPECT_EQ(SByte::Parse("101", NumberStyles::BinaryNumber, nullptr), 5);
    EXPECT_EQ(UInt16::Parse("101", NumberStyles::BinaryNumber, nullptr), 5u);
    EXPECT_EQ(UInt64::Parse("101", NumberStyles::BinaryNumber, nullptr), 5u);
    EXPECT_EQ(Byte::Parse("101", NumberStyles::BinaryNumber, nullptr), 5u);
}

// -----------------------------------------------------------------------
// Unsigned-parser repeated-sign rejection (fixed 2026-07-14, duplicated-implementation audit
// finding): TryParseUnsignedCore's leading/trailing '+' matching had no "already consumed a
// sign" guard, unlike TryParseSignedCore's shared `haveSign` flag -- so multiple '+' tokens
// (leading, trailing, or both) were silently accepted instead of rejected as a format error.
// -----------------------------------------------------------------------

TEST(NumberStylesExtendedTests, UnsignedParse_RepeatedLeadingSign_Rejected) {
    SharpRuntime::uintcs out;
    EXPECT_FALSE(UInt32::TryParse("++5", NumberStyles::Integer, nullptr, out));
    EXPECT_FALSE(UInt32::TryParse("+++5", NumberStyles::Integer, nullptr, out));
}

TEST(NumberStylesExtendedTests, UnsignedParse_RepeatedTrailingSign_Rejected) {
    SharpRuntime::uintcs out;
    NumberStyles style = NumberStyles::Integer | NumberStyles::AllowTrailingSign;
    EXPECT_FALSE(UInt32::TryParse("5++", style, nullptr, out));
}

TEST(NumberStylesExtendedTests, UnsignedParse_LeadingAndTrailingSignTogether_Rejected) {
    SharpRuntime::uintcs out;
    NumberStyles style = NumberStyles::Integer | NumberStyles::AllowTrailingSign;
    EXPECT_FALSE(UInt32::TryParse("+5+", style, nullptr, out));
}

TEST(NumberStylesExtendedTests, UnsignedParse_SingleSign_StillAccepted) {
    // Sanity check: the fix must not reject a single, legitimate '+'.
    EXPECT_EQ(UInt32::Parse("+5", NumberStyles::Integer, nullptr), 5u);
    EXPECT_EQ(UInt32::Parse("5+", NumberStyles::Integer | NumberStyles::AllowTrailingSign, nullptr), 5u);
    EXPECT_EQ(Byte::Parse("+5", NumberStyles::Integer, nullptr), 5u);
}

// ---------------------------------------------------------------------------------------
// #2268 / SR-AUD-177 -- NumberStyles::AllowExponent on the integer parsers.
//
// Every expectation below is transcribed from .NET's OWN test suite where it pins a row, and
// otherwise derived from the digit-buffer-and-scale model in Number.Parsing.Common.cs and
// Number.Parsing.cs. The ticket was blocked on evidence, not on approval, and this is it.
// ---------------------------------------------------------------------------------------

TEST(IntegerAllowExponentTests, Fix2268_TheRowsDotNetsOwnTestSuitePins) {
    // Int32Tests.cs:353-358 -- the whole reason SR-AUD-177 exists.
    const auto E = NumberStyles::AllowExponent;
    EXPECT_EQ(100, Int32::Parse("1E2", E, nullptr));
    EXPECT_EQ(100, Int32::Parse("1E+2", E, nullptr));
    EXPECT_EQ(100, Int32::Parse("1e2", E, nullptr));
    EXPECT_EQ(1, Int32::Parse("1E0", E, nullptr));
    EXPECT_EQ(-100, Int32::Parse("-1E2", E | NumberStyles::AllowLeadingSign, nullptr));
    EXPECT_EQ(-100, Int32::Parse("(1E2)", E | NumberStyles::AllowParentheses, nullptr));

    // Int32Tests.cs:546-548 and :665 -- and note the exception TYPE. An exponent that cannot be
    // represented is an OverflowException, never a FormatException, however small the number
    // it denotes.
    EXPECT_THROW((void)Int32::Parse("65E10", E, nullptr), System::OverflowException);
    EXPECT_THROW((void)Int32::Parse("65E+10", E, nullptr), System::OverflowException);
    EXPECT_THROW((void)Int32::Parse("2E10", E, nullptr), System::OverflowException);

    // Int32Tests.cs:548. THIS is the row the ticket was blocked on: 65E-1 is 6.5, and .NET
    // reports it as an overflow because scale 1 cannot carry two significant digits.
    EXPECT_THROW((void)Int32::Parse("65E-1", E, nullptr), System::OverflowException);

    // Int32Tests.cs:473 -- without the flag the 'E' is simply trailing garbage.
    EXPECT_THROW((void)Int32::Parse("1E23", NumberStyles::Integer, nullptr),
                 System::FormatException);
}

TEST(IntegerAllowExponentTests, Fix2268_ANegativeExponentIsAcceptedWhenTheScaleStillCarriesTheDigits) {
    // The rule is `Scale >= DigitsCount`, not "the exponent must be non-negative". A trailing
    // zero does not advance DigitsCount for an integer buffer, so "100" is digits "1" at scale
    // 3 -- which is exactly what lets 100E-2 be 1 while 1E-2 overflows.
    const auto E = NumberStyles::AllowExponent;
    EXPECT_EQ(1, Int32::Parse("100E-2", E, nullptr));
    EXPECT_EQ(10, Int32::Parse("1000E-2", E, nullptr));
    EXPECT_EQ(5, Int32::Parse("5E-0", E, nullptr));
    EXPECT_THROW((void)Int32::Parse("1E-2", E, nullptr), System::OverflowException);
    EXPECT_THROW((void)Int32::Parse("12E-1", E, nullptr), System::OverflowException);
}

TEST(IntegerAllowExponentTests, Fix2268_TheExponentCombinesWithADecimalPoint) {
    // 1.5E1 is digits "15" at scale 1+1 = 2, so 15. 1.5E0 is the same digits at scale 1, which
    // cannot carry them -- the same rule that has always made "123.5" an overflow here.
    const auto ANY = NumberStyles::Any;
    EXPECT_EQ(15, Int32::Parse("1.5E1", ANY, nullptr));
    EXPECT_EQ(150, Int32::Parse("1.5E2", ANY, nullptr));
    EXPECT_EQ(1234, Int32::Parse("1.234E3", ANY, nullptr));
    EXPECT_EQ(1, Int32::Parse("0.001E3", ANY, nullptr));
    EXPECT_EQ(1, Int32::Parse("1,000E-3", ANY, nullptr));
    EXPECT_THROW((void)Int32::Parse("1.5E0", ANY, nullptr), System::OverflowException);
    EXPECT_THROW((void)Int32::Parse("0.001E2", ANY, nullptr), System::OverflowException);
    EXPECT_THROW((void)Int32::Parse("1.2345E3", ANY, nullptr), System::OverflowException);
}

TEST(IntegerAllowExponentTests, Fix2268_AnExponentWithoutDigitsIsAFormatError) {
    // .NET rewinds to the 'E' when no digit follows it (`p = temp`), so the 'E' is left as
    // trailing garbage and the parse fails on the GRAMMAR -- not as an overflow.
    const auto E = NumberStyles::AllowExponent;
    for (const char* text : {"1E", "1E+", "1E-", "E2", "1EE2", "1E2E3"}) {
        SCOPED_TRACE(text);
        EXPECT_THROW((void)Int32::Parse(text, E, nullptr), System::FormatException);
    }
}

TEST(IntegerAllowExponentTests, Fix2268_AnAbsurdExponentOverflowsEitherWayItIsSigned) {
    // .NET caps the exponent at int.MaxValue once it reaches 100,000,000, resetting the scale
    // to zero first, so the sign cannot rescue it. This also has to terminate promptly rather
    // than multiplying by ten a hundred million times.
    const auto E = NumberStyles::AllowExponent;
    EXPECT_THROW((void)Int32::Parse("1E100000000", E, nullptr), System::OverflowException);
    EXPECT_THROW((void)Int32::Parse("1E-100000000", E, nullptr), System::OverflowException);
    EXPECT_THROW((void)Int32::Parse("1E999999999999999999999", E, nullptr),
                 System::OverflowException);
}

TEST(IntegerAllowExponentTests, Fix2268_EveryIntegerWrapperGotTheFlagNotJustInt32) {
    // SR-AUD-177 is a gap across all eight wrappers; a repair in the shared core has to show up
    // in all eight, and the per-type range check has to keep working on top of it.
    const auto E = NumberStyles::AllowExponent;
    EXPECT_EQ(100, Int16::Parse("1E2", E, nullptr));
    EXPECT_EQ(100, Int64::Parse("1E2", E, nullptr));
    EXPECT_EQ(100, SByte::Parse("1E2", E, nullptr));
    EXPECT_EQ(100u, UInt16::Parse("1E2", E, nullptr));
    EXPECT_EQ(100u, UInt32::Parse("1E2", E, nullptr));
    EXPECT_EQ(100u, UInt64::Parse("1E2", E, nullptr));
    EXPECT_EQ(100u, System::Byte::Parse("1E2", E, nullptr));

    // 1E3 is 1000: inside Int16 and out of range for SByte and Byte.
    EXPECT_EQ(1000, Int16::Parse("1E3", E, nullptr));
    EXPECT_THROW((void)SByte::Parse("1E3", E, nullptr), System::OverflowException);
    EXPECT_THROW((void)System::Byte::Parse("1E3", E, nullptr), System::OverflowException);

    // And an exponent no 64-bit type can hold.
    EXPECT_THROW((void)UInt64::Parse("1E20", E, nullptr), System::OverflowException);
    EXPECT_EQ(10000000000000000000ull, UInt64::Parse("1E19", E, nullptr));
}

TEST(IntegerAllowExponentTests, Fix2268_TheAllZeroMagnitudeIsTheOneDeliberateDeviation) {
    // Read literally, `Scale < DigitsCount` makes "0.0" an OverflowException in .NET: the
    // fractional zero takes the `Scale--` branch and leaves scale -1 against a digits count of
    // 0. This port keeps returning 0, because .NET's own suite pins no such row, the reading is
    // an unexecutable source trace, and it is a narrowing SR-AUD-177 never asked for. Ticket
    // #2356 holds the question; this test is what stops the answer changing by accident.
    EXPECT_EQ(0, Int32::Parse("0.0", NumberStyles::Number, nullptr));
    EXPECT_EQ(0, Int32::Parse("000.000", NumberStyles::Number, nullptr));
    EXPECT_EQ(0, Int32::Parse("0E-2", NumberStyles::AllowExponent, nullptr));
    EXPECT_EQ(0, Int32::Parse("0E2", NumberStyles::AllowExponent, nullptr));

    // The deviation is confined to a NON-POSITIVE scale. An all-zero magnitude with an absurd
    // positive exponent still overflows, exactly as .NET does.
    EXPECT_THROW((void)Int32::Parse("0E100000000", NumberStyles::AllowExponent, nullptr),
                 System::OverflowException);
}

TEST(IntegerAllowExponentTests, Fix2268_TheDecimalPointRowsAreUntouched) {
    // The scale model replaced a `fracNonZero` flag, so every pre-existing decimal-point answer
    // has to be reproduced by the new code rather than merely left alone.
    const auto N = NumberStyles::Number;
    EXPECT_EQ(123, Int32::Parse("123.00", N, nullptr));
    EXPECT_EQ(1, Int32::Parse("1.0", N, nullptr));
    EXPECT_EQ(10, Int32::Parse("10.0", N, nullptr));
    EXPECT_EQ(1, Int32::Parse("1.", N, nullptr));
    EXPECT_THROW((void)Int32::Parse("123.5", N, nullptr), System::OverflowException);
    EXPECT_THROW((void)Int32::Parse("0.5", N, nullptr), System::OverflowException);
    EXPECT_THROW((void)Int32::Parse(".5", N, nullptr), System::OverflowException);
}
