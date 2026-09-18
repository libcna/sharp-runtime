// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// The IFormatProvider overloads of the numeric primitives honour the NumberFormatInfo the provider
// supplies (System/detail/NumberFormatText.hpp), and Single/Double gained the NumberStyles
// overloads (System/detail/FloatNumberStylesParser.hpp). Cultures come from the Globalization
// component, a declared test dependency of Core.Base.
#include <gtest/gtest.h>
#include <cmath>
#include <limits>
#include <string>
#include "System/ArgumentException.hpp"
#include "System/Byte.hpp"
#include "System/Double.hpp"
#include "System/FormatException.hpp"
#include "System/Int16.hpp"
#include "System/Int32.hpp"
#include "System/Int64.hpp"
#include "System/OverflowException.hpp"
#include "System/SByte.hpp"
#include "System/Single.hpp"
#include "System/UInt16.hpp"
#include "System/UInt32.hpp"
#include "System/UInt64.hpp"
#include "System/Globalization/CultureInfo.hpp"
#include "System/Globalization/NumberFormatInfo.hpp"
#include "System/Globalization/NumberStyles.hpp"
#include "System/detail/FloatNumberStylesParser.hpp"
#include "System/detail/NumberFormatText.hpp"

using System::Byte;
using System::Double;
using System::Int16;
using System::Int32;
using System::Int64;
using System::SByte;
using System::Single;
using System::UInt16;
using System::UInt32;
using System::UInt64;
using System::Globalization::CultureInfo;
using System::Globalization::NumberFormatInfo;
using System::Globalization::NumberStyles;

namespace {

const CultureInfo& czech() {
    static const CultureInfo culture = CultureInfo::GetCultureInfo("cs-CZ");
    return culture;
}
const CultureInfo& german() {
    static const CultureInfo culture = CultureInfo::GetCultureInfo("de-DE");
    return culture;
}
const CultureInfo& english() {
    static const CultureInfo culture = CultureInfo::GetCultureInfo("en-US");
    return culture;
}
const CultureInfo& invariant() { return CultureInfo::getInvariantCultureProperty(); }

const std::string kNbsp = "\xc2\xa0";

// A NumberFormatInfo whose signs and special symbols differ from the invariant spelling, which no
// row of the culture table exercises but which the rewrite has to handle for a caller that builds
// its own info (a mutable CultureInfo("en-US") whose NumberFormat was edited, for instance).
NumberFormatInfo exoticInfo() {
    NumberFormatInfo info;
    info.setNumberDecimalSeparatorProperty(",");
    info.setNumberGroupSeparatorProperty("'");
    info.setNegativeSignProperty("\xe2\x88\x92");          // U+2212 MINUS SIGN
    info.setPositiveSignProperty("plus");
    info.setNaNSymbolProperty("not-a-number");
    info.setPositiveInfinitySymbolProperty("\xe2\x88\x9e"); // U+221E
    info.setNegativeInfinitySymbolProperty("\xe2\x88\x92\xe2\x88\x9e");
    return info;
}

class ExoticProvider final : public System::IFormatProvider {
public:
    [[nodiscard]] void* GetFormat(const std::type_info& type) const override {
        if (type == typeid(NumberFormatInfo)) return const_cast<NumberFormatInfo*>(&info_);
        return nullptr;
    }
private:
    NumberFormatInfo info_ = exoticInfo();
};

} // namespace

// ---------------------------------------------------------------------------
// NumberFormatText -- the rewrite itself
// ---------------------------------------------------------------------------

TEST(NumberFormatTextTest, InvariantSpellingIsTheIdentity) {
    EXPECT_TRUE(System::detail::NumberFormatText::IsInvariantSpelling(NumberFormatInfo::getInvariantInfoProperty()));
    EXPECT_TRUE(System::detail::NumberFormatText::IsInvariantSpelling(english().getNumberFormatProperty()));
    EXPECT_FALSE(System::detail::NumberFormatText::IsInvariantSpelling(czech().getNumberFormatProperty()));
    EXPECT_EQ(System::detail::NumberFormatText::NormalizeForParsing("1,234.5", NumberStyles::Float, &english()), "1,234.5");
    EXPECT_EQ(System::detail::NumberFormatText::NormalizeForParsing("1,234.5", NumberStyles::Float, nullptr), "1,234.5");
}

TEST(NumberFormatTextTest, GermanSwapsDecimalAndGroupInOnePass) {
    const auto& de = german().getNumberFormatProperty();
    EXPECT_EQ(System::detail::NumberFormatText::NormalizeForParsing("1.234,5", NumberStyles::Number, de), "1,234.5");
    EXPECT_EQ(System::detail::NumberFormatText::NormalizeForParsing("-0,5", NumberStyles::Number, de), "-0.5");
}

TEST(NumberFormatTextTest, AnInvariantTokenTheCultureDoesNotUseCannotSneakThrough) {
    // cs-CZ has no role for "." at all, so "1.5" must not become a decimal fraction.
    const auto& cs = czech().getNumberFormatProperty();
    const std::string rewritten = System::detail::NumberFormatText::NormalizeForParsing("1.5", NumberStyles::Float, cs);
    EXPECT_EQ(rewritten, std::string("1\x01") + "5");
}

TEST(NumberFormatTextTest, CurrencyFamilyTakesPartOnlyWhenTheStyleAllowsIt) {
    NumberFormatInfo info;
    info.setCurrencySymbolProperty("Kc");
    info.setCurrencyDecimalSeparatorProperty(";");
    EXPECT_EQ(System::detail::NumberFormatText::NormalizeForParsing("Kc 1;5", NumberStyles::Currency, info), "\xc2\xa4 1.5");
    // Without AllowCurrencySymbol the currency separators are not consulted, so ";" stays.
    EXPECT_EQ(System::detail::NumberFormatText::NormalizeForParsing("1;5", NumberStyles::Float, info), "1;5");
}

TEST(NumberFormatTextTest, LocalizeRespellsSeparatorsSignsAndSymbols) {
    const NumberFormatInfo info = exoticInfo();
    EXPECT_EQ(System::detail::NumberFormatText::LocalizeFormatted("-1,234.5", info), "\xe2\x88\x92" "1'234,5");
    EXPECT_EQ(System::detail::NumberFormatText::LocalizeFormatted("NaN", info), "not-a-number");
    EXPECT_EQ(System::detail::NumberFormatText::LocalizeFormatted("-Infinity", info), "\xe2\x88\x92\xe2\x88\x9e");
    EXPECT_EQ(System::detail::NumberFormatText::LocalizeFormatted("1.5E+10", info), "1,5Eplus10");
    EXPECT_EQ(System::detail::NumberFormatText::LocalizeFormatted("42", nullptr), "42");
}

// ---------------------------------------------------------------------------
// Single / Double with a provider
// ---------------------------------------------------------------------------

TEST(SingleProviderTest, ParseHonoursTheCultureDecimalSeparator) {
    EXPECT_FLOAT_EQ(Single::Parse("1,5", &czech()), 1.5f);
    EXPECT_FLOAT_EQ(Single::Parse("-0,25", &german()), -0.25f);
    EXPECT_FLOAT_EQ(Single::Parse("1.5", &english()), 1.5f);
    EXPECT_FLOAT_EQ(Single::Parse("1.5", &invariant()), 1.5f);
    EXPECT_FLOAT_EQ(Single::Parse("1.5", nullptr), 1.5f);
}

TEST(SingleProviderTest, ParseHonoursTheCultureGroupSeparator) {
    EXPECT_FLOAT_EQ(Single::Parse("1" + kNbsp + "234,5", &czech()), 1234.5f);
    EXPECT_FLOAT_EQ(Single::Parse("1.234,5", &german()), 1234.5f);
    EXPECT_FLOAT_EQ(Single::Parse("1,234.5", &english()), 1234.5f);
}

TEST(SingleProviderTest, AnInvariantDecimalPointIsRejectedWhereTheCultureHasNoRoleForIt) {
    EXPECT_THROW((void)Single::Parse("1.5", &czech()), System::FormatException);
    float value = 1.0f;
    EXPECT_FALSE(Single::TryParse("1.5", &czech(), value));
    EXPECT_EQ(value, 0.0f);
    // de-DE does have a role for "." -- the group separator -- so "1.5" is fifteen there, as in .NET.
    EXPECT_FLOAT_EQ(Single::Parse("1.5", &german()), 15.0f);
}

TEST(SingleProviderTest, FloatStyleRejectsGroupSeparatorsAndTheDefaultStyleAcceptsThem) {
    EXPECT_THROW((void)Single::Parse("1,000", NumberStyles::Float, nullptr), System::FormatException);
    EXPECT_FLOAT_EQ(Single::Parse("1,000", NumberStyles::Float | NumberStyles::AllowThousands, nullptr), 1000.0f);
    EXPECT_FLOAT_EQ(Single::Parse("1,000", nullptr), 1000.0f);
    EXPECT_THROW((void)Single::Parse("1" + kNbsp + "000", NumberStyles::Float, &czech()), System::FormatException);
    EXPECT_FLOAT_EQ(Single::Parse("1" + kNbsp + "000", NumberStyles::Float | NumberStyles::AllowThousands, &czech()), 1000.0f);
}

TEST(SingleProviderTest, StyleFlagsGateWhitespaceSignExponentAndDecimalPoint) {
    EXPECT_FLOAT_EQ(Single::Parse(" 1.5 ", NumberStyles::Float, nullptr), 1.5f);
    EXPECT_THROW((void)Single::Parse(" 1.5", NumberStyles::None, nullptr), System::FormatException);
    EXPECT_THROW((void)Single::Parse("1.5 ", NumberStyles::AllowLeadingWhite, nullptr), System::FormatException);
    EXPECT_THROW((void)Single::Parse("-1", NumberStyles::None, nullptr), System::FormatException);
    EXPECT_FLOAT_EQ(Single::Parse("-1", NumberStyles::AllowLeadingSign, nullptr), -1.0f);
    EXPECT_THROW((void)Single::Parse("1.5", NumberStyles::AllowLeadingSign, nullptr), System::FormatException);
    EXPECT_FLOAT_EQ(Single::Parse("1.5", NumberStyles::AllowDecimalPoint, nullptr), 1.5f);
    EXPECT_THROW((void)Single::Parse("1e2", NumberStyles::AllowDecimalPoint, nullptr), System::FormatException);
    EXPECT_FLOAT_EQ(Single::Parse("1e2", NumberStyles::AllowExponent, nullptr), 100.0f);
    EXPECT_FLOAT_EQ(Single::Parse("1.5E-1", NumberStyles::Float, nullptr), 0.15f);
    EXPECT_FLOAT_EQ(Single::Parse("12", NumberStyles::None, nullptr), 12.0f);
}

TEST(SingleProviderTest, NumberAndCurrencyStylesAdmitTrailingSignParenthesesAndSymbol) {
    EXPECT_FLOAT_EQ(Single::Parse("1.5-", NumberStyles::Number, nullptr), -1.5f);
    EXPECT_FLOAT_EQ(Single::Parse("(1.5)", NumberStyles::Currency, nullptr), -1.5f);
    EXPECT_THROW((void)Single::Parse("(1.5", NumberStyles::Currency, nullptr), System::FormatException);
    EXPECT_FLOAT_EQ(Single::Parse("\xc2\xa4" "2.5", NumberStyles::Currency, nullptr), 2.5f);
    EXPECT_FLOAT_EQ(Single::Parse("2.5 \xc2\xa4", NumberStyles::Currency, nullptr), 2.5f);
    EXPECT_THROW((void)Single::Parse("\xc2\xa4" "2.5", NumberStyles::Float, nullptr), System::FormatException);
    EXPECT_THROW((void)Single::Parse("1.5-", NumberStyles::Float, nullptr), System::FormatException);
}

TEST(SingleProviderTest, SpecialSymbolsAreMatchedOutsideTheGrammar) {
    EXPECT_TRUE(std::isnan(Single::Parse(" NaN ", NumberStyles::None, nullptr)));
    EXPECT_EQ(Single::Parse("-Infinity", NumberStyles::None, nullptr), -std::numeric_limits<float>::infinity());
    EXPECT_EQ(Single::Parse("infinity", NumberStyles::Float, nullptr), std::numeric_limits<float>::infinity());
    const ExoticProvider exotic;
    EXPECT_TRUE(std::isnan(Single::Parse("not-a-number", &exotic)));
    EXPECT_EQ(Single::Parse("\xe2\x88\x92\xe2\x88\x9e", &exotic), -std::numeric_limits<float>::infinity());
    // The invariant word is not the exotic info's symbol, so it is not recognised there.
    EXPECT_THROW((void)Single::Parse("NaN", &exotic), System::FormatException);
}

TEST(SingleProviderTest, ExoticSignsAreHonoured) {
    const ExoticProvider exotic;
    EXPECT_FLOAT_EQ(Single::Parse("\xe2\x88\x92" "1'234,5", &exotic), -1234.5f);
    EXPECT_FLOAT_EQ(Single::Parse("plus2,5", &exotic), 2.5f);
    EXPECT_THROW((void)Single::Parse("-1", &exotic), System::FormatException);
}

TEST(SingleProviderTest, InvalidStylesAreArgumentErrors) {
    EXPECT_THROW((void)Single::Parse("1", NumberStyles::HexNumber, nullptr), System::ArgumentException);
    EXPECT_THROW((void)Single::Parse("1", NumberStyles::BinaryNumber, nullptr), System::ArgumentException);
    EXPECT_THROW((void)Single::Parse("1", static_cast<NumberStyles>(0x1000), nullptr), System::ArgumentException);
    float value{};
    EXPECT_THROW((void)Single::TryParse("1", NumberStyles::HexNumber, nullptr, value), System::ArgumentException);
}

TEST(SingleProviderTest, OverflowStillSaturatesUnderAStyle) {
    EXPECT_EQ(Single::Parse("1e50", NumberStyles::Float, &czech()), std::numeric_limits<float>::infinity());
    EXPECT_EQ(Single::Parse("-1e50", NumberStyles::Float, nullptr), -std::numeric_limits<float>::infinity());
    EXPECT_EQ(Single::Parse("1e-50", NumberStyles::Float, nullptr), 0.0f);
}

TEST(SingleProviderTest, ToStringHonoursTheProvider) {
    EXPECT_EQ(Single::ToString(1.5f, &czech()), "1,5");
    EXPECT_EQ(Single::ToString(-0.25f, &german()), "-0,25");
    EXPECT_EQ(Single::ToString(1.5f, &english()), "1.5");
    EXPECT_EQ(Single::ToString(1.5f, "R", &czech()), "1,5");
    EXPECT_EQ(Single::ToString(1234.5f, "N1", &czech()), "1" + kNbsp + "234,5");
    EXPECT_EQ(Single::ToString(1234.5f, "N1", &german()), "1.234,5");
    EXPECT_EQ(Single::ToString(1234.5f, "N1", nullptr), "1,234.5");
    EXPECT_EQ(Single::ToString(0.5f, "F2", &czech()), "0,50");
    EXPECT_EQ(Single::ToString(std::numeric_limits<float>::quiet_NaN(), &czech()), "NaN");
    const ExoticProvider exotic;
    EXPECT_EQ(Single::ToString(-1.5f, &exotic), "\xe2\x88\x92" "1,5");
    EXPECT_EQ(Single::ToString(std::numeric_limits<float>::infinity(), &exotic), "\xe2\x88\x9e");
}

TEST(SingleProviderTest, RoundTripsThroughACulture) {
    const float values[] = {0.0f, -0.0f, 1.0f / 3.0f, 3.1415927f, -123456.79f, 1e-10f, 1e30f};
    for (const float value : values) {
        const std::string text = Single::ToString(value, "R", &czech());
        EXPECT_EQ(Single::Parse(text, NumberStyles::Float, &czech()), value) << text;
    }
}

TEST(DoubleProviderTest, ParseAndToStringHonourTheProvider) {
    EXPECT_DOUBLE_EQ(Double::Parse("1,5", &czech()), 1.5);
    EXPECT_DOUBLE_EQ(Double::Parse("1.234,5", NumberStyles::Number, &german()), 1234.5);
    EXPECT_THROW((void)Double::Parse("1.5", &czech()), System::FormatException);
    EXPECT_THROW((void)Double::Parse("1,000", NumberStyles::Float, nullptr), System::FormatException);
    double value{};
    EXPECT_TRUE(Double::TryParse("2,5", &czech(), value));
    EXPECT_DOUBLE_EQ(value, 2.5);
    EXPECT_TRUE(Double::TryParse(" -2.5e1 ", NumberStyles::Float, nullptr, value));
    EXPECT_DOUBLE_EQ(value, -25.0);
    EXPECT_THROW((void)Double::Parse("1", NumberStyles::HexNumber, nullptr), System::ArgumentException);
    EXPECT_EQ(Double::ToString(1.5, &czech()), "1,5");
    EXPECT_EQ(Double::ToString(-1234.5, "N1", &german()), "-1.234,5");
    EXPECT_EQ(Double::ToString(0.1, "R", &english()), "0.1");
    EXPECT_TRUE(std::isnan(Double::Parse("NaN", NumberStyles::None, &czech())));
}

// ---------------------------------------------------------------------------
// The eight integer types
// ---------------------------------------------------------------------------

TEST(IntegerProviderTest, Int32ParseHonoursTheCultureUnderNumberAndCurrencyStyles) {
    EXPECT_EQ(Int32::Parse("1.234", NumberStyles::Number, &german()), 1234);
    EXPECT_EQ(Int32::Parse("1" + kNbsp + "234", NumberStyles::Number, &czech()), 1234);
    EXPECT_EQ(Int32::Parse("1,234", NumberStyles::Number, &english()), 1234);
    EXPECT_EQ(Int32::Parse("-42", NumberStyles::Integer, &czech()), -42);
    // NumberStyles.Integer admits no group separator in any culture.
    EXPECT_THROW((void)Int32::Parse("1.234", NumberStyles::Integer, &german()), System::FormatException);
    // "1,5" is a fractional value under de-DE, and a fraction with a nonzero digit overflows an
    // integer in .NET's model rather than failing the format.
    EXPECT_THROW((void)Int32::Parse("1,5", NumberStyles::Number, &german()), System::OverflowException);
    SharpRuntime::intcs value{};
    EXPECT_TRUE(Int32::TryParse("7.000", NumberStyles::Number, &german(), value));
    EXPECT_EQ(value, 7000);
    EXPECT_FALSE(Int32::TryParse("7.000", NumberStyles::Integer, &german(), value));
}

TEST(IntegerProviderTest, HexStyleIsUnaffectedByTheCulture) {
    EXPECT_EQ(Int32::Parse("FF", NumberStyles::HexNumber, &czech()), 255);
    EXPECT_EQ(Byte::Parse("ff", NumberStyles::HexNumber, &german()), 255);
}

TEST(IntegerProviderTest, EveryIntegerTypeParsesAndFormatsWithTheProvider) {
    EXPECT_EQ(Byte::Parse("1" + kNbsp + "2", NumberStyles::Number, &czech()), 12);
    EXPECT_EQ(SByte::Parse("-1.2", NumberStyles::Number, &german()), -12);
    EXPECT_EQ(Int16::Parse("1.234", NumberStyles::Number, &german()), 1234);
    EXPECT_EQ(UInt16::Parse("1.234", NumberStyles::Number, &german()), 1234u);
    EXPECT_EQ(UInt32::Parse("1.234.567", NumberStyles::Number, &german()), 1234567u);
    EXPECT_EQ(Int64::Parse("-1.234.567", NumberStyles::Number, &german()), -1234567);
    EXPECT_EQ(UInt64::Parse("1" + kNbsp + "234", NumberStyles::Number, &czech()), 1234u);

    // Only Int32 (with Single and Double) implements the custom placeholder formats today, so
    // the group-separator respelling is pinned on it alone; the other integer types are pinned
    // on the sign and on the standard specifiers they do implement.
    EXPECT_EQ(Int32::ToString(-1234567, "#,##0", &german()), "-1.234.567");
    EXPECT_EQ(Int32::ToString(-1234567, "#,##0", &czech()), "-1" + kNbsp + "234" + kNbsp + "567");
    EXPECT_EQ(Int32::ToString(-5, &czech()), "-5");
    EXPECT_EQ(Byte::ToString(200, &czech()), "200");
    EXPECT_EQ(SByte::ToString(-7, &german()), "-7");
    EXPECT_EQ(Int16::ToString(-1234, &german()), "-1234");
    EXPECT_EQ(Int16::ToString(-1234, "D6", &german()), "-001234");
    EXPECT_EQ(UInt16::ToString(1234, &czech()), "1234");
    EXPECT_EQ(UInt32::ToString(1234567u, "X", &german()), "12D687");
    EXPECT_EQ(Int64::ToString(-1234567, &german()), "-1234567");
    EXPECT_EQ(UInt64::ToString(1234567u, "D8", &german()), "01234567");
    EXPECT_EQ(Int32::ToString(255, "X", &german()), "FF");
    const ExoticProvider exotic;
    EXPECT_EQ(Int32::ToString(-5, &exotic), "\xe2\x88\x92" "5");
    EXPECT_EQ(Int32::Parse("\xe2\x88\x92" "5", NumberStyles::Integer, &exotic), -5);
}

TEST(IntegerProviderTest, TheProviderlessOverloadsAreUnchanged) {
    EXPECT_EQ(Int32::Parse("1,234", NumberStyles::Number, nullptr), 1234);
    EXPECT_THROW((void)Int32::Parse("1.234", NumberStyles::Number, nullptr), System::OverflowException);
    EXPECT_EQ(Int32::ToString(-1234, "#,##0"), "-1,234");
}

// ---------------------------------------------------------------------------
// FloatNumberStylesParser directly
// ---------------------------------------------------------------------------

TEST(FloatNumberStylesParserTest, CanonicalFormDropsGroupSeparatorsAndNormalisesTheExponent) {
    std::string canonical;
    ASSERT_TRUE(System::detail::FloatNumberStylesParser::TryCanonicalize("  -1,234.50e+2  ", NumberStyles::Any, canonical));
    EXPECT_EQ(canonical, "-1234.50E+2");
    ASSERT_TRUE(System::detail::FloatNumberStylesParser::TryCanonicalize(".5", NumberStyles::Float, canonical));
    EXPECT_EQ(canonical, "0.5");
    ASSERT_TRUE(System::detail::FloatNumberStylesParser::TryCanonicalize("5.", NumberStyles::Float, canonical));
    EXPECT_EQ(canonical, "5");
    EXPECT_FALSE(System::detail::FloatNumberStylesParser::TryCanonicalize(".", NumberStyles::Float, canonical));
    EXPECT_FALSE(System::detail::FloatNumberStylesParser::TryCanonicalize("", NumberStyles::Float, canonical));
    EXPECT_FALSE(System::detail::FloatNumberStylesParser::TryCanonicalize("1e", NumberStyles::Float, canonical));
    EXPECT_FALSE(System::detail::FloatNumberStylesParser::TryCanonicalize("1,2.3,4", NumberStyles::Any, canonical));
    EXPECT_FALSE(System::detail::FloatNumberStylesParser::TryCanonicalize("1\x01" "5", NumberStyles::Any, canonical));
}
