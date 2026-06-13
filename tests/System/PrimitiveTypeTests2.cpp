// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

#include "System/Int16.hpp"
#include "System/UInt16.hpp"
#include "System/SByte.hpp"
#include "System/Boolean.hpp"
#include "System/Char.hpp"
#include "System/Single.hpp"
#include "System/Double.hpp"

using System::Int16;
using System::UInt16;
using System::SByte;
using System::Boolean;
using System::Char;
using System::Single;
using System::Double;

// ---------------------------------------------------------------------------
// Int16
// ---------------------------------------------------------------------------

TEST(Int16Tests, Constants) {
    EXPECT_EQ(Int16::MaxValue,  32767);
    EXPECT_EQ(Int16::MinValue, -32768);
}

TEST(Int16Tests, ParsePositive) {
    EXPECT_EQ(Int16::Parse(std::string("100")), 100);
}

TEST(Int16Tests, ParseNegative) {
    EXPECT_EQ(Int16::Parse(std::string("-1")), -1);
}

TEST(Int16Tests, ParseMaxValue) {
    EXPECT_EQ(Int16::Parse(std::string("32767")), 32767);
}

TEST(Int16Tests, ParseMinValue) {
    EXPECT_EQ(Int16::Parse(std::string("-32768")), -32768);
}

TEST(Int16Tests, ParseOverflowThrows) {
    EXPECT_THROW(Int16::Parse(std::string("32768")), std::out_of_range);
}

TEST(Int16Tests, ParseUnderflowThrows) {
    EXPECT_THROW(Int16::Parse(std::string("-32769")), std::out_of_range);
}

TEST(Int16Tests, ParseInvalidThrows) {
    EXPECT_THROW(Int16::Parse(std::string("abc")), std::invalid_argument);
}

TEST(Int16Tests, TryParseValid) {
    int16_t result = 0;
    EXPECT_TRUE(Int16::TryParse(std::string("42"), result));
    EXPECT_EQ(result, 42);
}

TEST(Int16Tests, TryParseInvalid) {
    int16_t result = 99;
    EXPECT_FALSE(Int16::TryParse(std::string("xyz"), result));
    EXPECT_EQ(result, 0);
}

TEST(Int16Tests, ToString) {
    EXPECT_EQ(Int16::ToString(static_cast<int16_t>(0)),     "0");
    EXPECT_EQ(Int16::ToString(static_cast<int16_t>(32767)), "32767");
    EXPECT_EQ(Int16::ToString(static_cast<int16_t>(-1)),    "-1");
}

// ---------------------------------------------------------------------------
// UInt16
// ---------------------------------------------------------------------------

TEST(UInt16Tests, Constants) {
    EXPECT_EQ(UInt16::MaxValue, 65535u);
    EXPECT_EQ(UInt16::MinValue, 0u);
}

TEST(UInt16Tests, ParseValid) {
    EXPECT_EQ(UInt16::Parse(std::string("1000")), 1000u);
}

TEST(UInt16Tests, ParseZero) {
    EXPECT_EQ(UInt16::Parse(std::string("0")), 0u);
}

TEST(UInt16Tests, ParseMaxValue) {
    EXPECT_EQ(UInt16::Parse(std::string("65535")), 65535u);
}

TEST(UInt16Tests, ParseOverflowThrows) {
    EXPECT_THROW(UInt16::Parse(std::string("65536")), std::out_of_range);
}

TEST(UInt16Tests, ParseInvalidThrows) {
    EXPECT_THROW(UInt16::Parse(std::string("abc")), std::invalid_argument);
}

TEST(UInt16Tests, TryParseValid) {
    uint16_t result = 0;
    EXPECT_TRUE(UInt16::TryParse(std::string("255"), result));
    EXPECT_EQ(result, 255u);
}

TEST(UInt16Tests, TryParseInvalid) {
    uint16_t result = 7;
    EXPECT_FALSE(UInt16::TryParse(std::string("bad"), result));
    EXPECT_EQ(result, 0u);
}

TEST(UInt16Tests, ToString) {
    EXPECT_EQ(UInt16::ToString(static_cast<uint16_t>(0)),     "0");
    EXPECT_EQ(UInt16::ToString(static_cast<uint16_t>(65535)), "65535");
}

// ---------------------------------------------------------------------------
// SByte
// ---------------------------------------------------------------------------

TEST(SByteTests, Constants) {
    EXPECT_EQ(SByte::MaxValue,  127);
    EXPECT_EQ(SByte::MinValue, -128);
}

TEST(SByteTests, ParsePositive) {
    EXPECT_EQ(SByte::Parse(std::string("127")), 127);
}

TEST(SByteTests, ParseNegative) {
    EXPECT_EQ(SByte::Parse(std::string("-128")), -128);
}

TEST(SByteTests, ParseZero) {
    EXPECT_EQ(SByte::Parse(std::string("0")), 0);
}

TEST(SByteTests, ParseOverflowThrows) {
    EXPECT_THROW(SByte::Parse(std::string("128")), std::out_of_range);
}

TEST(SByteTests, ParseUnderflowThrows) {
    EXPECT_THROW(SByte::Parse(std::string("-129")), std::out_of_range);
}

TEST(SByteTests, ParseInvalidThrows) {
    EXPECT_THROW(SByte::Parse(std::string("!!")), std::invalid_argument);
}

TEST(SByteTests, TryParseValid) {
    int8_t result = 0;
    EXPECT_TRUE(SByte::TryParse(std::string("50"), result));
    EXPECT_EQ(result, 50);
}

TEST(SByteTests, TryParseInvalid) {
    int8_t result = 9;
    EXPECT_FALSE(SByte::TryParse(std::string("xyz"), result));
    EXPECT_EQ(result, 0);
}

TEST(SByteTests, ToString) {
    EXPECT_EQ(SByte::ToString(static_cast<int8_t>(0)),    "0");
    EXPECT_EQ(SByte::ToString(static_cast<int8_t>(127)),  "127");
    EXPECT_EQ(SByte::ToString(static_cast<int8_t>(-128)), "-128");
}

// ---------------------------------------------------------------------------
// Boolean
// ---------------------------------------------------------------------------

TEST(BooleanTests, TrueString) {
    EXPECT_EQ(std::string(Boolean::TrueString),  "True");
}

TEST(BooleanTests, FalseString) {
    EXPECT_EQ(std::string(Boolean::FalseString), "False");
}

TEST(BooleanTests, ParseTrueExact) {
    EXPECT_TRUE(Boolean::Parse(std::string("True")));
}

TEST(BooleanTests, ParseTrueLower) {
    EXPECT_TRUE(Boolean::Parse(std::string("true")));
}

TEST(BooleanTests, ParseFalseExact) {
    EXPECT_FALSE(Boolean::Parse(std::string("False")));
}

TEST(BooleanTests, ParseFalseLower) {
    EXPECT_FALSE(Boolean::Parse(std::string("false")));
}

TEST(BooleanTests, ParseInvalidThrows) {
    EXPECT_THROW(Boolean::Parse(std::string("yes")),  std::invalid_argument);
    EXPECT_THROW(Boolean::Parse(std::string("1")),    std::invalid_argument);
    EXPECT_THROW(Boolean::Parse(std::string("")),     std::invalid_argument);
}

TEST(BooleanTests, TryParseTrue) {
    bool result = false;
    EXPECT_TRUE(Boolean::TryParse(std::string("True"), result));
    EXPECT_TRUE(result);
}

TEST(BooleanTests, TryParseFalse) {
    bool result = true;
    EXPECT_TRUE(Boolean::TryParse(std::string("false"), result));
    EXPECT_FALSE(result);
}

TEST(BooleanTests, TryParseInvalid) {
    bool result = true;
    EXPECT_FALSE(Boolean::TryParse(std::string("maybe"), result));
    EXPECT_FALSE(result);
}

TEST(BooleanTests, ToStringTrue) {
    EXPECT_EQ(Boolean::ToString(true),  "True");
}

TEST(BooleanTests, ToStringFalse) {
    EXPECT_EQ(Boolean::ToString(false), "False");
}

// ---------------------------------------------------------------------------
// Char  (charcs = char16_t)
// ---------------------------------------------------------------------------

TEST(CharTests, MaxValue) {
    EXPECT_EQ(Char::MaxValue, static_cast<char16_t>(0xFFFF));
}

TEST(CharTests, IsLetterAlpha) {
    EXPECT_TRUE(Char::IsLetter(u'A'));
    EXPECT_TRUE(Char::IsLetter(u'z'));
}

TEST(CharTests, IsLetterDigitFalse) {
    EXPECT_FALSE(Char::IsLetter(u'5'));
    EXPECT_FALSE(Char::IsLetter(u'!'));
}

TEST(CharTests, IsDigitTrue) {
    EXPECT_TRUE(Char::IsDigit(u'0'));
    EXPECT_TRUE(Char::IsDigit(u'9'));
}

TEST(CharTests, IsDigitFalse) {
    EXPECT_FALSE(Char::IsDigit(u'a'));
    EXPECT_FALSE(Char::IsDigit(u' '));
}

TEST(CharTests, IsLetterOrDigit) {
    EXPECT_TRUE(Char::IsLetterOrDigit(u'A'));
    EXPECT_TRUE(Char::IsLetterOrDigit(u'5'));
    EXPECT_FALSE(Char::IsLetterOrDigit(u'!'));
}

TEST(CharTests, IsWhiteSpace) {
    EXPECT_TRUE(Char::IsWhiteSpace(u' '));
    EXPECT_TRUE(Char::IsWhiteSpace(u'\n'));
    EXPECT_FALSE(Char::IsWhiteSpace(u'a'));
}

TEST(CharTests, IsUpperLower) {
    EXPECT_TRUE(Char::IsUpper(u'A'));
    EXPECT_FALSE(Char::IsUpper(u'a'));
    EXPECT_TRUE(Char::IsLower(u'a'));
    EXPECT_FALSE(Char::IsLower(u'A'));
}

TEST(CharTests, IsPunctuation) {
    EXPECT_TRUE(Char::IsPunctuation(u'.'));
    EXPECT_TRUE(Char::IsPunctuation(u','));
    EXPECT_FALSE(Char::IsPunctuation(u'a'));
}

TEST(CharTests, IsControl) {
    EXPECT_TRUE(Char::IsControl(u'\0'));
    EXPECT_TRUE(Char::IsControl(u'\t'));
    EXPECT_FALSE(Char::IsControl(u'a'));
}

TEST(CharTests, ToUpperFromLower) {
    EXPECT_EQ(Char::ToUpper(u'a'), u'A');
    EXPECT_EQ(Char::ToUpper(u'z'), u'Z');
}

TEST(CharTests, ToUpperAlreadyUpper) {
    EXPECT_EQ(Char::ToUpper(u'A'), u'A');
}

TEST(CharTests, ToLowerFromUpper) {
    EXPECT_EQ(Char::ToLower(u'A'), u'a');
    EXPECT_EQ(Char::ToLower(u'Z'), u'z');
}

TEST(CharTests, ToLowerAlreadyLower) {
    EXPECT_EQ(Char::ToLower(u'a'), u'a');
}

TEST(CharTests, GetNumericValueDigit) {
    EXPECT_EQ(Char::GetNumericValue(u'0'), 0);
    EXPECT_EQ(Char::GetNumericValue(u'5'), 5);
    EXPECT_EQ(Char::GetNumericValue(u'9'), 9);
}

TEST(CharTests, GetNumericValueNonDigit) {
    EXPECT_EQ(Char::GetNumericValue(u'a'), -1);
    EXPECT_EQ(Char::GetNumericValue(u' '), -1);
}

TEST(CharTests, ParseSingleChar) {
    EXPECT_EQ(Char::Parse(std::string("A")), u'A');
    EXPECT_EQ(Char::Parse(std::string("z")), u'z');
}

TEST(CharTests, ParseMultiCharThrows) {
    EXPECT_THROW(Char::Parse(std::string("ab")), std::invalid_argument);
}

TEST(CharTests, ParseEmptyThrows) {
    EXPECT_THROW(Char::Parse(std::string("")), std::invalid_argument);
}

TEST(CharTests, ToStringChar) {
    EXPECT_EQ(Char::ToString(u'A'), "A");
    EXPECT_EQ(Char::ToString(u'z'), "z");
}

TEST(CharTests, Parse_TwoByte_UTF8) {
    // "é" = U+00E9 = 0xC3 0xA9 in UTF-8
    EXPECT_EQ(Char::Parse("\xC3\xA9"), u'é');
}

TEST(CharTests, Parse_ThreeByte_UTF8) {
    // "€" = U+20AC = 0xE2 0x82 0xAC in UTF-8
    EXPECT_EQ(Char::Parse("\xE2\x82\xAC"), u'€');
}

TEST(CharTests, Parse_TwoByte_UTF8_TwoCharsThrows) {
    // "éé" = two code points — must throw
    EXPECT_THROW(Char::Parse("\xC3\xA9\xC3\xA9"), std::invalid_argument);
}

TEST(CharTests, ToString_TwoByte_UTF8) {
    // Round-trip: char16_t U+00E9 → UTF-8 "é"
    EXPECT_EQ(Char::ToString(u'é'), "\xC3\xA9");
}

TEST(CharTests, ToString_ThreeByte_UTF8) {
    // Round-trip: char16_t U+20AC → UTF-8 "€"
    EXPECT_EQ(Char::ToString(u'€'), "\xE2\x82\xAC");
}

TEST(CharTests, Parse_ToString_RoundTrip_Multibyte) {
    char16_t orig = u'č'; // 'č' U+010D
    EXPECT_EQ(Char::Parse(Char::ToString(orig)), orig);
}

TEST(CharTests, IsHighSurrogate) {
    EXPECT_TRUE(Char::IsHighSurrogate(static_cast<char16_t>(0xD800)));
    EXPECT_TRUE(Char::IsHighSurrogate(static_cast<char16_t>(0xDBFF)));
    EXPECT_FALSE(Char::IsHighSurrogate(u'a'));
}

TEST(CharTests, IsLowSurrogate) {
    EXPECT_TRUE(Char::IsLowSurrogate(static_cast<char16_t>(0xDC00)));
    EXPECT_TRUE(Char::IsLowSurrogate(static_cast<char16_t>(0xDFFF)));
    EXPECT_FALSE(Char::IsLowSurrogate(u'a'));
}

TEST(CharTests, IsSurrogatePair) {
    EXPECT_TRUE(Char::IsSurrogatePair(
        static_cast<char16_t>(0xD800), static_cast<char16_t>(0xDC00)));
    EXPECT_FALSE(Char::IsSurrogatePair(u'a', u'b'));
}

TEST(CharTests, ConvertToUtf32) {
    // U+10000 = surrogate pair (0xD800, 0xDC00)
    EXPECT_EQ(Char::ConvertToUtf32(
        static_cast<char16_t>(0xD800), static_cast<char16_t>(0xDC00)), 0x10000);
}

TEST(CharTests, ConvertToUtf32InvalidThrows) {
    EXPECT_THROW(Char::ConvertToUtf32(u'a', u'b'), std::invalid_argument);
}

// ---------------------------------------------------------------------------
// ASCII helpers (.NET 7+)
// ---------------------------------------------------------------------------
TEST(CharTests, IsAscii_True)  { EXPECT_TRUE(Char::IsAscii(u'A')); }
TEST(CharTests, IsAscii_False) { EXPECT_FALSE(Char::IsAscii(u'\x80')); }

TEST(CharTests, IsAsciiDigit_True)    { EXPECT_TRUE(Char::IsAsciiDigit(u'5')); }
TEST(CharTests, IsAsciiDigit_Letter)  { EXPECT_FALSE(Char::IsAsciiDigit(u'a')); }

TEST(CharTests, IsAsciiUpper_True)    { EXPECT_TRUE(Char::IsAsciiUpper(u'Z')); }
TEST(CharTests, IsAsciiUpper_Lower)   { EXPECT_FALSE(Char::IsAsciiUpper(u'z')); }

TEST(CharTests, IsAsciiLower_True)    { EXPECT_TRUE(Char::IsAsciiLower(u'a')); }
TEST(CharTests, IsAsciiLower_Upper)   { EXPECT_FALSE(Char::IsAsciiLower(u'A')); }

TEST(CharTests, IsAsciiLetter_Upper)      { EXPECT_TRUE(Char::IsAsciiLetter(u'A')); }
TEST(CharTests, IsAsciiLetter_Lower)      { EXPECT_TRUE(Char::IsAsciiLetter(u'z')); }
TEST(CharTests, IsAsciiLetter_Digit)      { EXPECT_FALSE(Char::IsAsciiLetter(u'3')); }

TEST(CharTests, IsAsciiLetterOrDigit_Digit)  { EXPECT_TRUE(Char::IsAsciiLetterOrDigit(u'9')); }
TEST(CharTests, IsAsciiLetterOrDigit_Letter) { EXPECT_TRUE(Char::IsAsciiLetterOrDigit(u'B')); }
TEST(CharTests, IsAsciiLetterOrDigit_Space)  { EXPECT_FALSE(Char::IsAsciiLetterOrDigit(u' ')); }

TEST(CharTests, IsAsciiLetterUpper_True)  { EXPECT_TRUE(Char::IsAsciiLetterUpper(u'M')); }
TEST(CharTests, IsAsciiLetterUpper_Lower) { EXPECT_FALSE(Char::IsAsciiLetterUpper(u'm')); }

TEST(CharTests, IsAsciiLetterLower_True)  { EXPECT_TRUE(Char::IsAsciiLetterLower(u'm')); }
TEST(CharTests, IsAsciiLetterLower_Upper) { EXPECT_FALSE(Char::IsAsciiLetterLower(u'M')); }

TEST(CharTests, IsAsciiHexDigit_Digit) { EXPECT_TRUE(Char::IsAsciiHexDigit(u'9')); }
TEST(CharTests, IsAsciiHexDigit_Upper) { EXPECT_TRUE(Char::IsAsciiHexDigit(u'F')); }
TEST(CharTests, IsAsciiHexDigit_Lower) { EXPECT_TRUE(Char::IsAsciiHexDigit(u'f')); }
TEST(CharTests, IsAsciiHexDigit_G)     { EXPECT_FALSE(Char::IsAsciiHexDigit(u'G')); }

TEST(CharTests, IsAsciiHexDigitLower_af) { EXPECT_TRUE(Char::IsAsciiHexDigitLower(u'f')); }
TEST(CharTests, IsAsciiHexDigitLower_AF) { EXPECT_FALSE(Char::IsAsciiHexDigitLower(u'F')); }

TEST(CharTests, IsAsciiHexDigitUpper_AF) { EXPECT_TRUE(Char::IsAsciiHexDigitUpper(u'F')); }
TEST(CharTests, IsAsciiHexDigitUpper_af) { EXPECT_FALSE(Char::IsAsciiHexDigitUpper(u'f')); }

// ---------------------------------------------------------------------------
// Single (float)
// ---------------------------------------------------------------------------

TEST(SingleTests, MaxValuePositive) {
    EXPECT_GT(Single::MaxValue, 0.0f);
    EXPECT_TRUE(std::isfinite(Single::MaxValue));
}

TEST(SingleTests, MinValueNegative) {
    EXPECT_LT(Single::MinValue, 0.0f);
}

TEST(SingleTests, EpsilonPositive) {
    EXPECT_GT(Single::Epsilon, 0.0f);
    EXPECT_LT(Single::Epsilon, 1.0f);
}

TEST(SingleTests, NaNIsNaN) {
    EXPECT_TRUE(Single::IsNaN(Single::NaN));
    EXPECT_FALSE(Single::IsNaN(1.0f));
}

TEST(SingleTests, PositiveInfinity) {
    EXPECT_TRUE(Single::IsPositiveInfinity(Single::PositiveInfinity));
    EXPECT_FALSE(Single::IsPositiveInfinity(Single::NegativeInfinity));
    EXPECT_FALSE(Single::IsPositiveInfinity(1.0f));
}

TEST(SingleTests, NegativeInfinity) {
    EXPECT_TRUE(Single::IsNegativeInfinity(Single::NegativeInfinity));
    EXPECT_FALSE(Single::IsNegativeInfinity(Single::PositiveInfinity));
}

TEST(SingleTests, IsInfinity) {
    EXPECT_TRUE(Single::IsInfinity(Single::PositiveInfinity));
    EXPECT_TRUE(Single::IsInfinity(Single::NegativeInfinity));
    EXPECT_FALSE(Single::IsInfinity(1.0f));
    EXPECT_FALSE(Single::IsInfinity(Single::NaN));
}

TEST(SingleTests, IsFinite) {
    EXPECT_TRUE(Single::IsFinite(1.0f));
    EXPECT_TRUE(Single::IsFinite(0.0f));
    EXPECT_FALSE(Single::IsFinite(Single::PositiveInfinity));
    EXPECT_FALSE(Single::IsFinite(Single::NaN));
}

TEST(SingleTests, IsNormal) {
    EXPECT_TRUE(Single::IsNormal(1.0f));
    EXPECT_FALSE(Single::IsNormal(Single::NaN));
    EXPECT_FALSE(Single::IsNormal(0.0f));
}

TEST(SingleTests, ParseValid) {
    float v = Single::Parse(std::string("3.14"));
    EXPECT_NEAR(v, 3.14f, 1e-4f);
}

TEST(SingleTests, ParseNegative) {
    float v = Single::Parse(std::string("-1.5"));
    EXPECT_NEAR(v, -1.5f, 1e-6f);
}

TEST(SingleTests, ParseInvalidThrows) {
    EXPECT_THROW(Single::Parse(std::string("abc")), std::invalid_argument);
}

TEST(SingleTests, TryParseValid) {
    float result = 0.0f;
    EXPECT_TRUE(Single::TryParse(std::string("2.5"), result));
    EXPECT_NEAR(result, 2.5f, 1e-6f);
}

TEST(SingleTests, TryParseInvalid) {
    float result = 99.0f;
    EXPECT_FALSE(Single::TryParse(std::string("bad"), result));
    EXPECT_NEAR(result, 0.0f, 1e-6f);
}

TEST(SingleTests, ToStringContainsDigits) {
    std::string s = Single::ToString(1.0f);
    EXPECT_NE(s.find('1'), std::string::npos);
}

// ---------------------------------------------------------------------------
// Double
// ---------------------------------------------------------------------------

TEST(DoubleTests, MaxValuePositive) {
    EXPECT_GT(Double::MaxValue, 0.0);
    EXPECT_GT(Double::MaxValue, static_cast<double>(Single::MaxValue));
    EXPECT_TRUE(std::isfinite(Double::MaxValue));
}

TEST(DoubleTests, MinValueNegative) {
    EXPECT_LT(Double::MinValue, 0.0);
}

TEST(DoubleTests, EpsilonPositive) {
    EXPECT_GT(Double::Epsilon, 0.0);
    EXPECT_LT(Double::Epsilon, 1.0);
}

TEST(DoubleTests, NaNIsNaN) {
    EXPECT_TRUE(Double::IsNaN(Double::NaN));
    EXPECT_FALSE(Double::IsNaN(1.0));
}

TEST(DoubleTests, PositiveInfinity) {
    EXPECT_TRUE(Double::IsPositiveInfinity(Double::PositiveInfinity));
    EXPECT_FALSE(Double::IsPositiveInfinity(Double::NegativeInfinity));
    EXPECT_FALSE(Double::IsPositiveInfinity(1.0));
}

TEST(DoubleTests, NegativeInfinity) {
    EXPECT_TRUE(Double::IsNegativeInfinity(Double::NegativeInfinity));
    EXPECT_FALSE(Double::IsNegativeInfinity(Double::PositiveInfinity));
}

TEST(DoubleTests, IsInfinity) {
    EXPECT_TRUE(Double::IsInfinity(Double::PositiveInfinity));
    EXPECT_TRUE(Double::IsInfinity(Double::NegativeInfinity));
    EXPECT_FALSE(Double::IsInfinity(1.0));
    EXPECT_FALSE(Double::IsInfinity(Double::NaN));
}

TEST(DoubleTests, IsFinite) {
    EXPECT_TRUE(Double::IsFinite(1.0));
    EXPECT_TRUE(Double::IsFinite(0.0));
    EXPECT_FALSE(Double::IsFinite(Double::PositiveInfinity));
    EXPECT_FALSE(Double::IsFinite(Double::NaN));
}

TEST(DoubleTests, IsNormal) {
    EXPECT_TRUE(Double::IsNormal(1.0));
    EXPECT_FALSE(Double::IsNormal(Double::NaN));
    EXPECT_FALSE(Double::IsNormal(0.0));
}

TEST(DoubleTests, ParseValid) {
    double v = Double::Parse(std::string("3.141592653589793"));
    EXPECT_NEAR(v, 3.141592653589793, 1e-14);
}

TEST(DoubleTests, ParseNegative) {
    double v = Double::Parse(std::string("-2.71828"));
    EXPECT_NEAR(v, -2.71828, 1e-5);
}

TEST(DoubleTests, ParseZero) {
    EXPECT_EQ(Double::Parse(std::string("0")), 0.0);
}

TEST(DoubleTests, ParseInvalidThrows) {
    EXPECT_THROW(Double::Parse(std::string("abc")), std::invalid_argument);
}

TEST(DoubleTests, TryParseValid) {
    double result = 0.0;
    EXPECT_TRUE(Double::TryParse(std::string("1.5"), result));
    EXPECT_NEAR(result, 1.5, 1e-12);
}

TEST(DoubleTests, TryParseInvalid) {
    double result = 99.0;
    EXPECT_FALSE(Double::TryParse(std::string("bad"), result));
    EXPECT_NEAR(result, 0.0, 1e-12);
}

TEST(DoubleTests, ToStringContainsDigits) {
    std::string s = Double::ToString(1.0);
    EXPECT_NE(s.find('1'), std::string::npos);
}
