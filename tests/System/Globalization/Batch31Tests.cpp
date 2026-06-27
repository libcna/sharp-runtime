// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Tests for Batch 31:
//   JulianCalendar:    IsLeapYear, GetDaysInMonth, GetEra, GetErasCount
//   KoreanCalendar:    GetEra, GetErasCount, GetYear
//   NumberFormatInfo:  default values, IsReadOnly, InvariantInfo, ReadOnly, Clone,
//                      missing fields (CurrencyDecimalDigits, NativeDigits, PerMilleSymbol, etc.)
#include <gtest/gtest.h>
#include "System/Globalization/JulianCalendar.hpp"
#include "System/Globalization/KoreanCalendar.hpp"
#include "System/Globalization/NumberFormatInfo.hpp"
#include "System/DateTime.hpp"

using System::Globalization::JulianCalendar;
using System::Globalization::KoreanCalendar;
using System::Globalization::NumberFormatInfo;

// ===========================================================================
// JulianCalendar
// ===========================================================================

TEST(JulianCalendarBatch31Test, EraConstant) {
    EXPECT_EQ(JulianCalendar::JulianEra, 1);
}

TEST(JulianCalendarBatch31Test, GetEra) {
    JulianCalendar jc;
    EXPECT_EQ(jc.GetEra(System::DateTime(2024, 6, 1)), JulianCalendar::JulianEra);
}

TEST(JulianCalendarBatch31Test, GetErasCount) {
    JulianCalendar jc;
    EXPECT_EQ(jc.GetErasCount(), 1);
}

TEST(JulianCalendarBatch31Test, IsLeapYear_DivisibleBy4) {
    JulianCalendar jc;
    EXPECT_TRUE(jc.IsLeapYear(100));  // leap in Julian (not in Gregorian)
    EXPECT_TRUE(jc.IsLeapYear(1900)); // leap in Julian (not in Gregorian)
    EXPECT_TRUE(jc.IsLeapYear(2000));
    EXPECT_TRUE(jc.IsLeapYear(2024));
}

TEST(JulianCalendarBatch31Test, IsLeapYear_NotDivisibleBy4) {
    JulianCalendar jc;
    EXPECT_FALSE(jc.IsLeapYear(2023));
    EXPECT_FALSE(jc.IsLeapYear(1999));
}

TEST(JulianCalendarBatch31Test, GetDaysInMonth_February_JulianLeap) {
    JulianCalendar jc;
    EXPECT_EQ(jc.GetDaysInMonth(1900, 2), 29); // leap in Julian
    EXPECT_EQ(jc.GetDaysInMonth(2024, 2), 29);
    EXPECT_EQ(jc.GetDaysInMonth(2023, 2), 28);
}

TEST(JulianCalendarBatch31Test, GetDaysInMonth_OtherMonths) {
    JulianCalendar jc;
    EXPECT_EQ(jc.GetDaysInMonth(2024, 1),  31);
    EXPECT_EQ(jc.GetDaysInMonth(2024, 4),  30);
    EXPECT_EQ(jc.GetDaysInMonth(2024, 12), 31);
}

// ===========================================================================
// KoreanCalendar
// ===========================================================================

TEST(KoreanCalendarBatch31Test, EraConstant) {
    EXPECT_EQ(KoreanCalendar::KoreanEra, 1);
}

TEST(KoreanCalendarBatch31Test, GetEra) {
    KoreanCalendar kc;
    EXPECT_EQ(kc.GetEra(System::DateTime(2024, 1, 1)), KoreanCalendar::KoreanEra);
}

TEST(KoreanCalendarBatch31Test, GetErasCount) {
    KoreanCalendar kc;
    EXPECT_EQ(kc.GetErasCount(), 1);
}

TEST(KoreanCalendarBatch31Test, GetYear_Offset2333) {
    KoreanCalendar kc;
    EXPECT_EQ(kc.GetYear(System::DateTime(2000, 1, 1)), 4333);
    EXPECT_EQ(kc.GetYear(System::DateTime(2024, 6, 1)), 4357);
    EXPECT_EQ(kc.GetYear(System::DateTime(1,    1, 1)), 2334);
}

// ===========================================================================
// NumberFormatInfo
// ===========================================================================

TEST(NumberFormatInfoBatch31Test, DefaultCtor_IsMutable) {
    NumberFormatInfo nfi;
    EXPECT_FALSE(nfi.getIsReadOnlyProperty());
}

TEST(NumberFormatInfoBatch31Test, InvariantInfo_IsReadOnly) {
    const auto& inv = NumberFormatInfo::InvariantInfo();
    EXPECT_TRUE(inv.getIsReadOnlyProperty());
}

TEST(NumberFormatInfoBatch31Test, CurrentInfo_IsInvariant) {
    const auto& cur = NumberFormatInfo::CurrentInfo();
    EXPECT_TRUE(cur.getIsReadOnlyProperty());
}

TEST(NumberFormatInfoBatch31Test, ReadOnly_MakesReadOnly) {
    NumberFormatInfo nfi;
    auto ro = NumberFormatInfo::ReadOnly(nfi);
    EXPECT_TRUE(ro.getIsReadOnlyProperty());
}

TEST(NumberFormatInfoBatch31Test, Clone_IsMutable) {
    auto clone = NumberFormatInfo::InvariantInfo().Clone();
    EXPECT_FALSE(clone.getIsReadOnlyProperty());
}

TEST(NumberFormatInfoBatch31Test, DefaultNumberFields) {
    NumberFormatInfo nfi;
    EXPECT_EQ(nfi.NumberDecimalSeparator, ".");
    EXPECT_EQ(nfi.NumberGroupSeparator,   ",");
    EXPECT_EQ(nfi.NumberDecimalDigits,    2);
    EXPECT_EQ(nfi.NegativeSign,           "-");
    EXPECT_EQ(nfi.PositiveSign,           "+");
}

TEST(NumberFormatInfoBatch31Test, DefaultCurrencyFields) {
    NumberFormatInfo nfi;
    EXPECT_EQ(nfi.CurrencyDecimalSeparator, ".");
    EXPECT_EQ(nfi.CurrencyGroupSeparator,   ",");
    EXPECT_EQ(nfi.CurrencySymbol,           "$");
    EXPECT_EQ(nfi.CurrencyDecimalDigits,    2);
}

TEST(NumberFormatInfoBatch31Test, DefaultSpecialSymbols) {
    NumberFormatInfo nfi;
    EXPECT_EQ(nfi.NaNSymbol,              "NaN");
    EXPECT_EQ(nfi.PositiveInfinitySymbol, "Infinity");
    EXPECT_EQ(nfi.NegativeInfinitySymbol, "-Infinity");
    EXPECT_EQ(nfi.PercentSymbol,          "%");
    EXPECT_FALSE(nfi.PerMilleSymbol.empty());
}

TEST(NumberFormatInfoBatch31Test, GroupSizes) {
    NumberFormatInfo nfi;
    EXPECT_EQ(nfi.NumberGroupSizes.size(),   1u);
    EXPECT_EQ(nfi.NumberGroupSizes[0],       3);
    EXPECT_EQ(nfi.CurrencyGroupSizes.size(), 1u);
    EXPECT_EQ(nfi.CurrencyGroupSizes[0],     3);
    EXPECT_EQ(nfi.PercentGroupSizes.size(),  1u);
    EXPECT_EQ(nfi.PercentGroupSizes[0],      3);
}

TEST(NumberFormatInfoBatch31Test, NativeDigits) {
    NumberFormatInfo nfi;
    ASSERT_EQ(nfi.NativeDigits.size(), 10u);
    EXPECT_EQ(nfi.NativeDigits[0], "0");
    EXPECT_EQ(nfi.NativeDigits[9], "9");
}

TEST(NumberFormatInfoBatch31Test, DigitSubstitution_Default) {
    NumberFormatInfo nfi;
    EXPECT_EQ(nfi.DigitSubstitution, System::Globalization::DigitShapes::None);
}
