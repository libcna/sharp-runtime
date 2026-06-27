// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Note: Calendar.hpp, GregorianCalendar.hpp, and ISOWeek.hpp are tested in tests/CalendarTests.cpp.
#include <gtest/gtest.h>
#include "System/Globalization/CultureInfo.hpp"
#include "System/Globalization/NumberFormatInfo.hpp"
#include "System/Globalization/RegionInfo.hpp"
#include "System/Globalization/StringInfo.hpp"
#include "System/Globalization/UnicodeCategory.hpp"

using System::Globalization::CultureInfo;
using System::Globalization::NumberFormatInfo;
using System::Globalization::RegionInfo;
using System::Globalization::StringInfo;
using System::Globalization::UnicodeCategory;

// ===========================================================================
// CultureInfo
// ===========================================================================

TEST(CultureInfoTests, InvariantCulture_NameIsEmpty) {
    EXPECT_EQ(CultureInfo::InvariantCulture().getNameProperty(), "");
}

TEST(CultureInfoTests, InvariantCulture_IsNeutral) {
    EXPECT_TRUE(CultureInfo::InvariantCulture().getIsNeutralCultureProperty());
}

TEST(CultureInfoTests, InvariantCulture_IsReadOnly) {
    EXPECT_TRUE(CultureInfo::InvariantCulture().getIsReadOnlyProperty());
}

TEST(CultureInfoTests, CurrentCulture_SameInstanceAsInvariant) {
    EXPECT_EQ(&CultureInfo::CurrentCulture(), &CultureInfo::InvariantCulture());
}

TEST(CultureInfoTests, DefaultConstructor_NameIsEmpty) {
    CultureInfo ci;
    EXPECT_EQ(ci.getNameProperty(), "");
}

TEST(CultureInfoTests, NamedConstructor_StoresName) {
    CultureInfo ci("en-US");
    EXPECT_EQ(ci.getNameProperty(), "en-US");
}

TEST(CultureInfoTests, NamedConstructor_IsNotNeutral) {
    CultureInfo ci("en-US");
    EXPECT_FALSE(ci.getIsNeutralCultureProperty());
}

TEST(CultureInfoTests, NamedConstructor_IsNotReadOnly) {
    CultureInfo ci("en-US");
    EXPECT_FALSE(ci.getIsReadOnlyProperty());
}

TEST(CultureInfoTests, InvariantCulture_ReturnsSameInstanceEveryCall) {
    const CultureInfo& a = CultureInfo::InvariantCulture();
    const CultureInfo& b = CultureInfo::InvariantCulture();
    EXPECT_EQ(&a, &b);
}

TEST(CultureInfoTests, NamedConstructor_DifferentNames) {
    CultureInfo en("en");
    CultureInfo de("de");
    EXPECT_NE(en.getNameProperty(), de.getNameProperty());
}

// ===========================================================================
// NumberFormatInfo
// ===========================================================================

TEST(NumberFormatInfoTests, InvariantInfo_IsReadOnly) {
    EXPECT_TRUE(NumberFormatInfo::InvariantInfo().getIsReadOnlyProperty());
}

TEST(NumberFormatInfoTests, InvariantInfo_DecimalSeparatorIsDot) {
    EXPECT_EQ(NumberFormatInfo::InvariantInfo().NumberDecimalSeparator, ".");
}

TEST(NumberFormatInfoTests, InvariantInfo_GroupSeparatorIsComma) {
    EXPECT_EQ(NumberFormatInfo::InvariantInfo().NumberGroupSeparator, ",");
}

TEST(NumberFormatInfoTests, InvariantInfo_CurrencyDecimalSeparatorIsDot) {
    EXPECT_EQ(NumberFormatInfo::InvariantInfo().CurrencyDecimalSeparator, ".");
}

TEST(NumberFormatInfoTests, InvariantInfo_CurrencyGroupSeparatorIsComma) {
    EXPECT_EQ(NumberFormatInfo::InvariantInfo().CurrencyGroupSeparator, ",");
}

TEST(NumberFormatInfoTests, InvariantInfo_CurrencySymbolIsDollar) {
    EXPECT_EQ(NumberFormatInfo::InvariantInfo().CurrencySymbol, "$");
}

TEST(NumberFormatInfoTests, InvariantInfo_NegativeSignIsMinus) {
    EXPECT_EQ(NumberFormatInfo::InvariantInfo().NegativeSign, "-");
}

TEST(NumberFormatInfoTests, InvariantInfo_PositiveSignIsPlus) {
    EXPECT_EQ(NumberFormatInfo::InvariantInfo().PositiveSign, "+");
}

TEST(NumberFormatInfoTests, InvariantInfo_PercentSymbol) {
    EXPECT_EQ(NumberFormatInfo::InvariantInfo().PercentSymbol, "%");
}

TEST(NumberFormatInfoTests, InvariantInfo_NaNSymbol) {
    EXPECT_EQ(NumberFormatInfo::InvariantInfo().NaNSymbol, "NaN");
}

TEST(NumberFormatInfoTests, InvariantInfo_PositiveInfinitySymbol) {
    EXPECT_EQ(NumberFormatInfo::InvariantInfo().PositiveInfinitySymbol, "Infinity");
}

TEST(NumberFormatInfoTests, InvariantInfo_NegativeInfinitySymbol) {
    EXPECT_EQ(NumberFormatInfo::InvariantInfo().NegativeInfinitySymbol, "-Infinity");
}

TEST(NumberFormatInfoTests, InvariantInfo_DecimalDigitsIsTwo) {
    EXPECT_EQ(NumberFormatInfo::InvariantInfo().NumberDecimalDigits, 2);
}

TEST(NumberFormatInfoTests, CurrentInfo_SameInstanceAsInvariant) {
    EXPECT_EQ(&NumberFormatInfo::CurrentInfo(), &NumberFormatInfo::InvariantInfo());
}

TEST(NumberFormatInfoTests, InvariantInfo_ReturnsSameInstanceEveryCall) {
    const NumberFormatInfo& a = NumberFormatInfo::InvariantInfo();
    const NumberFormatInfo& b = NumberFormatInfo::InvariantInfo();
    EXPECT_EQ(&a, &b);
}

TEST(NumberFormatInfoTests, DefaultInstance_NotReadOnly) {
    NumberFormatInfo nfi;
    EXPECT_FALSE(nfi.getIsReadOnlyProperty());
}

TEST(NumberFormatInfoTests, MutableInstance_FieldsModifiable) {
    NumberFormatInfo nfi;
    nfi.NumberDecimalSeparator = ",";
    nfi.NumberGroupSeparator   = ".";
    EXPECT_EQ(nfi.NumberDecimalSeparator, ",");
    EXPECT_EQ(nfi.NumberGroupSeparator, ".");
}

TEST(NumberFormatInfoTests, MutableInstance_CurrencySymbolModifiable) {
    NumberFormatInfo nfi;
    nfi.CurrencySymbol = "€";
    EXPECT_EQ(nfi.CurrencySymbol, "€");
}

// ===========================================================================
// RegionInfo
// ===========================================================================

TEST(RegionInfoTests, Constructor_StoresName) {
    RegionInfo r("US");
    EXPECT_EQ(r.getNameProperty(), "US");
}

TEST(RegionInfoTests, TwoLetterISORegionName_TwoChars) {
    RegionInfo r("US");
    EXPECT_EQ(r.getTwoLetterISORegionNameProperty().size(), 2u);
    EXPECT_EQ(r.getTwoLetterISORegionNameProperty(), "US");
}

TEST(RegionInfoTests, ThreeLetterISORegionName_ThreeChars) {
    RegionInfo r("USA");
    EXPECT_EQ(r.getThreeLetterISORegionNameProperty().size(), 3u);
    EXPECT_EQ(r.getThreeLetterISORegionNameProperty(), "USA");
}

TEST(RegionInfoTests, CurrencySymbol_Default) {
    RegionInfo r("US");
    EXPECT_EQ(r.getCurrencySymbolProperty(), "$");
}

TEST(RegionInfoTests, ISOCurrencySymbol_Default) {
    RegionInfo r("US");
    EXPECT_EQ(r.getISOCurrencySymbolProperty(), "USD");
}

TEST(RegionInfoTests, IsMetric_DefaultTrue) {
    RegionInfo r("US");
    EXPECT_TRUE(r.getIsMetricProperty());
}

TEST(RegionInfoTests, CurrentRegion_NameIsUS) {
    EXPECT_EQ(RegionInfo::CurrentRegion().getNameProperty(), "US");
}

TEST(RegionInfoTests, EnglishName_MatchesConstructorArg) {
    RegionInfo r("Germany");
    EXPECT_EQ(r.getEnglishNameProperty(), "Germany");
}

TEST(RegionInfoTests, NativeName_MatchesConstructorArg) {
    RegionInfo r("France");
    EXPECT_EQ(r.getNativeNameProperty(), "France");
}

TEST(RegionInfoTests, CurrentRegion_ReturnsSameInstanceEveryCall) {
    const RegionInfo& a = RegionInfo::CurrentRegion();
    const RegionInfo& b = RegionInfo::CurrentRegion();
    EXPECT_EQ(&a, &b);
}

// ===========================================================================
// StringInfo
// ===========================================================================

TEST(StringInfoTests, DefaultConstructor_EmptyString) {
    StringInfo si;
    EXPECT_EQ(si.getStringProperty(), "");
}

TEST(StringInfoTests, Constructor_StoresString) {
    StringInfo si("hello");
    EXPECT_EQ(si.getStringProperty(), "hello");
}

TEST(StringInfoTests, SetString_UpdatesValue) {
    StringInfo si("abc");
    si.setStringProperty("xyz");
    EXPECT_EQ(si.getStringProperty(), "xyz");
}

TEST(StringInfoTests, LengthInTextElements_AsciiString) {
    StringInfo si("hello");
    EXPECT_EQ(si.getLengthInTextElementsProperty(), 5);
}

TEST(StringInfoTests, LengthInTextElements_EmptyString) {
    StringInfo si("");
    EXPECT_EQ(si.getLengthInTextElementsProperty(), 0);
}

TEST(StringInfoTests, LengthInTextElements_SingleChar) {
    StringInfo si("X");
    EXPECT_EQ(si.getLengthInTextElementsProperty(), 1);
}

TEST(StringInfoTests, SubstringByTextElements_StartAndLength) {
    StringInfo si("abcdef");
    EXPECT_EQ(si.SubstringByTextElements(0, 3), "abc");
}

TEST(StringInfoTests, SubstringByTextElements_FromMiddle) {
    StringInfo si("abcdef");
    EXPECT_EQ(si.SubstringByTextElements(2, 2), "cd");
}

TEST(StringInfoTests, SubstringByTextElements_ToEnd) {
    StringInfo si("abcdef");
    EXPECT_EQ(si.SubstringByTextElements(3), "def");
}

TEST(StringInfoTests, SubstringByTextElements_FromStart_ToEnd) {
    StringInfo si("hello");
    EXPECT_EQ(si.SubstringByTextElements(0), "hello");
}

TEST(StringInfoTests, GetNextTextElement_DefaultIndex_FirstChar) {
    EXPECT_EQ(StringInfo::GetNextTextElement("hello"), "h");
}

TEST(StringInfoTests, GetNextTextElement_AtOffset) {
    EXPECT_EQ(StringInfo::GetNextTextElement("hello", 2), "l");
}

TEST(StringInfoTests, GetNextTextElement_LastChar) {
    EXPECT_EQ(StringInfo::GetNextTextElement("hello", 4), "o");
}

TEST(StringInfoTests, GetNextTextElement_PastEnd_ReturnsEmpty) {
    EXPECT_EQ(StringInfo::GetNextTextElement("hi", 5), "");
}

TEST(StringInfoTests, GetNextTextElementLength_AsciiIsOne) {
    EXPECT_EQ(StringInfo::GetNextTextElementLength("hello", 0), 1);
}

TEST(StringInfoTests, GetNextTextElementLength_PastEnd_IsZero) {
    EXPECT_EQ(StringInfo::GetNextTextElementLength("hi", 10), 0);
}

TEST(StringInfoTests, ParseCombiningCharacters_SplitsEachByte) {
    auto v = StringInfo::ParseCombiningCharacters("abc");
    ASSERT_EQ(v.size(), 3u);
    EXPECT_EQ(v[0], 0); // starting index of 'a'
    EXPECT_EQ(v[1], 1); // starting index of 'b'
    EXPECT_EQ(v[2], 2); // starting index of 'c'
}

TEST(StringInfoTests, ParseCombiningCharacters_EmptyString_EmptyResult) {
    auto v = StringInfo::ParseCombiningCharacters("");
    EXPECT_TRUE(v.empty());
}

TEST(StringInfoTests, ParseCombiningCharacters_SingleChar) {
    auto v = StringInfo::ParseCombiningCharacters("Z");
    ASSERT_EQ(v.size(), 1u);
    EXPECT_EQ(v[0], 0); // starting index of 'Z'
}

// ===========================================================================
// UnicodeCategory
// ===========================================================================

TEST(UnicodeCategoryTests, UppercaseLetter_ValueIsZero) {
    EXPECT_EQ(static_cast<int>(UnicodeCategory::UppercaseLetter), 0);
}

TEST(UnicodeCategoryTests, LowercaseLetter_ValueIsOne) {
    EXPECT_EQ(static_cast<int>(UnicodeCategory::LowercaseLetter), 1);
}

TEST(UnicodeCategoryTests, TitlecaseLetter_ValueIsTwo) {
    EXPECT_EQ(static_cast<int>(UnicodeCategory::TitlecaseLetter), 2);
}

TEST(UnicodeCategoryTests, DecimalDigitNumber_ValueIsEight) {
    EXPECT_EQ(static_cast<int>(UnicodeCategory::DecimalDigitNumber), 8);
}

TEST(UnicodeCategoryTests, SpaceSeparator_ValueIs11) {
    EXPECT_EQ(static_cast<int>(UnicodeCategory::SpaceSeparator), 11);
}

TEST(UnicodeCategoryTests, Control_ValueIs14) {
    EXPECT_EQ(static_cast<int>(UnicodeCategory::Control), 14);
}

TEST(UnicodeCategoryTests, MathSymbol_ValueIs25) {
    EXPECT_EQ(static_cast<int>(UnicodeCategory::MathSymbol), 25);
}

TEST(UnicodeCategoryTests, CurrencySymbol_ValueIs26) {
    EXPECT_EQ(static_cast<int>(UnicodeCategory::CurrencySymbol), 26);
}

TEST(UnicodeCategoryTests, OtherNotAssigned_ValueIs29) {
    EXPECT_EQ(static_cast<int>(UnicodeCategory::OtherNotAssigned), 29);
}

TEST(UnicodeCategoryTests, ConnectorPunctuation_ValueIs18) {
    EXPECT_EQ(static_cast<int>(UnicodeCategory::ConnectorPunctuation), 18);
}

TEST(UnicodeCategoryTests, DashPunctuation_ValueIs19) {
    EXPECT_EQ(static_cast<int>(UnicodeCategory::DashPunctuation), 19);
}

TEST(UnicodeCategoryTests, OtherPunctuation_ValueIs24) {
    EXPECT_EQ(static_cast<int>(UnicodeCategory::OtherPunctuation), 24);
}
