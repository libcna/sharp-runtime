// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Tests for remaining Globalization enums:
// NumberStyles, DateTimeStyles, CompareOptions, CultureTypes,
// CalendarAlgorithmType, CalendarWeekRule, GregorianCalendarTypes, TimeSpanStyles.
#include <gtest/gtest.h>
#include "System/Globalization/NumberStyles.hpp"
#include "System/Globalization/DateTimeStyles.hpp"
#include "System/Globalization/CompareOptions.hpp"
#include "System/Globalization/CultureTypes.hpp"
#include "System/Globalization/CalendarAlgorithmType.hpp"
#include "System/Globalization/CalendarWeekRule.hpp"
#include "System/Globalization/GregorianCalendarTypes.hpp"
#include "System/Globalization/TimeSpanStyles.hpp"

using System::Globalization::NumberStyles;
using System::Globalization::DateTimeStyles;
using System::Globalization::CompareOptions;
using System::Globalization::CultureTypes;
using System::Globalization::CalendarAlgorithmType;
using System::Globalization::CalendarWeekRule;
using System::Globalization::GregorianCalendarTypes;
using System::Globalization::TimeSpanStyles;

// ===========================================================================
// NumberStyles
// ===========================================================================

TEST(NumberStylesTests, None_IsZero) {
    EXPECT_EQ(static_cast<int>(NumberStyles::None), 0);
}
TEST(NumberStylesTests, AllowLeadingSign_IsCorrect) {
    EXPECT_EQ(static_cast<int>(NumberStyles::AllowLeadingSign), 0x04);
}
TEST(NumberStylesTests, AllowHexSpecifier_IsCorrect) {
    EXPECT_EQ(static_cast<int>(NumberStyles::AllowHexSpecifier), 0x200);
}
TEST(NumberStylesTests, Integer_ContainsLeadingWhiteAndSign) {
    auto integer = NumberStyles::Integer;
    EXPECT_NE(static_cast<int>(integer & NumberStyles::AllowLeadingWhite), 0);
    EXPECT_NE(static_cast<int>(integer & NumberStyles::AllowLeadingSign), 0);
}
TEST(NumberStylesTests, OrOperator_CombinesFlags) {
    auto combined = NumberStyles::AllowLeadingSign | NumberStyles::AllowDecimalPoint;
    EXPECT_NE(static_cast<int>(combined & NumberStyles::AllowLeadingSign), 0);
    EXPECT_NE(static_cast<int>(combined & NumberStyles::AllowDecimalPoint), 0);
}

// ===========================================================================
// DateTimeStyles
// ===========================================================================

TEST(DateTimeStylesTests, None_IsZero) {
    EXPECT_EQ(static_cast<int>(DateTimeStyles::None), 0);
}
TEST(DateTimeStylesTests, AllowWhiteSpaces_CombinesThree) {
    // AllowLeadingWhite | AllowInnerWhite | AllowTrailingWhite = 0x07
    EXPECT_EQ(static_cast<int>(DateTimeStyles::AllowWhiteSpaces), 0x07);
}
TEST(DateTimeStylesTests, AssumeUniversal_IsCorrect) {
    EXPECT_EQ(static_cast<int>(DateTimeStyles::AssumeUniversal), 0x40);
}
TEST(DateTimeStylesTests, OrOperator_CombinesFlags) {
    auto combined = DateTimeStyles::AssumeLocal | DateTimeStyles::AdjustToUniversal;
    EXPECT_NE(static_cast<int>(combined & DateTimeStyles::AssumeLocal), 0);
    EXPECT_NE(static_cast<int>(combined & DateTimeStyles::AdjustToUniversal), 0);
}

// ===========================================================================
// CompareOptions
// ===========================================================================

TEST(CompareOptionsTests, None_IsZero) {
    EXPECT_EQ(static_cast<int>(CompareOptions::None), 0);
}
TEST(CompareOptionsTests, IgnoreCase_IsOne) {
    EXPECT_EQ(static_cast<int>(CompareOptions::IgnoreCase), 1);
}
TEST(CompareOptionsTests, Ordinal_IsCorrect) {
    EXPECT_EQ(static_cast<int>(CompareOptions::Ordinal), 0x40000000);
}
TEST(CompareOptionsTests, OrOperator_CombinesFlags) {
    auto combined = CompareOptions::IgnoreCase | CompareOptions::IgnoreNonSpace;
    EXPECT_NE(static_cast<int>(combined & CompareOptions::IgnoreCase), 0);
    EXPECT_NE(static_cast<int>(combined & CompareOptions::IgnoreNonSpace), 0);
}

// ===========================================================================
// CultureTypes
// ===========================================================================

TEST(CultureTypesTests, NeutralCultures_IsOne) {
    EXPECT_EQ(static_cast<int>(CultureTypes::NeutralCultures), 0x01);
}
TEST(CultureTypesTests, AllCultures_CombinesNeutralAndSpecific) {
    EXPECT_EQ(static_cast<int>(CultureTypes::AllCultures), 0x07);
}
TEST(CultureTypesTests, OrOperator_CombinesFlags) {
    auto combined = CultureTypes::NeutralCultures | CultureTypes::SpecificCultures;
    EXPECT_NE(static_cast<int>(combined & CultureTypes::NeutralCultures), 0);
    EXPECT_NE(static_cast<int>(combined & CultureTypes::SpecificCultures), 0);
}

// ===========================================================================
// CalendarAlgorithmType
// ===========================================================================

TEST(CalendarAlgorithmTypeTests, Unknown_IsZero) {
    EXPECT_EQ(static_cast<int>(CalendarAlgorithmType::Unknown), 0);
}
TEST(CalendarAlgorithmTypeTests, SolarCalendar_IsOne) {
    EXPECT_EQ(static_cast<int>(CalendarAlgorithmType::SolarCalendar), 1);
}
TEST(CalendarAlgorithmTypeTests, LunarCalendar_IsTwo) {
    EXPECT_EQ(static_cast<int>(CalendarAlgorithmType::LunarCalendar), 2);
}
TEST(CalendarAlgorithmTypeTests, LunisolarCalendar_IsThree) {
    EXPECT_EQ(static_cast<int>(CalendarAlgorithmType::LunisolarCalendar), 3);
}

// ===========================================================================
// CalendarWeekRule
// ===========================================================================

TEST(CalendarWeekRuleTests, FirstDay_IsZero) {
    EXPECT_EQ(static_cast<int>(CalendarWeekRule::FirstDay), 0);
}
TEST(CalendarWeekRuleTests, FirstFullWeek_IsOne) {
    EXPECT_EQ(static_cast<int>(CalendarWeekRule::FirstFullWeek), 1);
}
TEST(CalendarWeekRuleTests, FirstFourDayWeek_IsTwo) {
    EXPECT_EQ(static_cast<int>(CalendarWeekRule::FirstFourDayWeek), 2);
}

// ===========================================================================
// GregorianCalendarTypes
// ===========================================================================

TEST(GregorianCalendarTypesTests, Localized_IsOne) {
    EXPECT_EQ(static_cast<int>(GregorianCalendarTypes::Localized), 1);
}
TEST(GregorianCalendarTypesTests, USEnglish_IsTwo) {
    EXPECT_EQ(static_cast<int>(GregorianCalendarTypes::USEnglish), 2);
}
TEST(GregorianCalendarTypesTests, Arabic_IsTen) {
    EXPECT_EQ(static_cast<int>(GregorianCalendarTypes::Arabic), 10);
}

// ===========================================================================
// TimeSpanStyles
// ===========================================================================

TEST(TimeSpanStylesTests, None_IsZero) {
    EXPECT_EQ(static_cast<int>(TimeSpanStyles::None), 0);
}
TEST(TimeSpanStylesTests, AssumeNegative_IsOne) {
    EXPECT_EQ(static_cast<int>(TimeSpanStyles::AssumeNegative), 1);
}
