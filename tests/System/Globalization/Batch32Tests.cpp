// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Tests for Batch 32:
//   NumberStyles:    enum values (including fixed Currency/Any), HexFloat, operator|/&
//   PersianCalendar: GetYear/Month/Day, IsLeapYear, GetDaysInMonth, GetDaysInYear
//   RegionInfo:      constructor, all property getters, CurrentRegion
//   SortKey:         constructors, getters, Compare, operator==, ToString
//   SortVersion:     constructors, getters, operator==/!=
#include <gtest/gtest.h>
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Globalization/NumberStyles.hpp"
#include "System/Globalization/PersianCalendar.hpp"
#include "System/Globalization/RegionInfo.hpp"
#include "System/Globalization/SortKey.hpp"
#include "System/Globalization/SortVersion.hpp"
#include "System/DateTime.hpp"

using System::Globalization::NumberStyles;
using System::Globalization::PersianCalendar;
using System::Globalization::RegionInfo;
using System::Globalization::SortKey;
using System::Globalization::SortVersion;

// ===========================================================================
// NumberStyles
// ===========================================================================

TEST(NumberStylesBatch32Test, AtomicValues) {
    EXPECT_EQ(static_cast<int>(NumberStyles::None),                0x000);
    EXPECT_EQ(static_cast<int>(NumberStyles::AllowLeadingWhite),   0x001);
    EXPECT_EQ(static_cast<int>(NumberStyles::AllowTrailingWhite),  0x002);
    EXPECT_EQ(static_cast<int>(NumberStyles::AllowLeadingSign),    0x004);
    EXPECT_EQ(static_cast<int>(NumberStyles::AllowTrailingSign),   0x008);
    EXPECT_EQ(static_cast<int>(NumberStyles::AllowParentheses),    0x010);
    EXPECT_EQ(static_cast<int>(NumberStyles::AllowDecimalPoint),   0x020);
    EXPECT_EQ(static_cast<int>(NumberStyles::AllowThousands),      0x040);
    EXPECT_EQ(static_cast<int>(NumberStyles::AllowExponent),       0x080);
    EXPECT_EQ(static_cast<int>(NumberStyles::AllowCurrencySymbol), 0x100);
    EXPECT_EQ(static_cast<int>(NumberStyles::AllowHexSpecifier),   0x200);
    EXPECT_EQ(static_cast<int>(NumberStyles::AllowBinarySpecifier),0x400);
}

TEST(NumberStylesBatch32Test, CompositeValues) {
    EXPECT_EQ(static_cast<int>(NumberStyles::Integer),      7);
    EXPECT_EQ(static_cast<int>(NumberStyles::HexNumber),    515);
    EXPECT_EQ(static_cast<int>(NumberStyles::BinaryNumber), 1027);
    EXPECT_EQ(static_cast<int>(NumberStyles::Number),       111);
    EXPECT_EQ(static_cast<int>(NumberStyles::Float),        167);
    EXPECT_EQ(static_cast<int>(NumberStyles::HexFloat),     679);
    EXPECT_EQ(static_cast<int>(NumberStyles::Currency),     383); // was wrong before (was 511)
    EXPECT_EQ(static_cast<int>(NumberStyles::Any),          511);
}

TEST(NumberStylesBatch32Test, OperatorOr) {
    auto combined = NumberStyles::AllowLeadingWhite | NumberStyles::AllowTrailingWhite;
    EXPECT_EQ(static_cast<int>(combined), 3);
}

TEST(NumberStylesBatch32Test, OperatorAnd) {
    auto combined = NumberStyles::Integer; // 7 = 1|2|4
    auto masked   = combined & NumberStyles::AllowLeadingSign;
    EXPECT_EQ(masked, NumberStyles::AllowLeadingSign);
}

TEST(NumberStylesBatch32Test, AnyEqualsCurrencyOrExponent) {
    auto computed = NumberStyles::Currency | NumberStyles::AllowExponent;
    EXPECT_EQ(computed, NumberStyles::Any);
}

// ===========================================================================
// PersianCalendar
// ===========================================================================

TEST(PersianCalendarBatch32Test, PersianEraConstant) {
    EXPECT_EQ(PersianCalendar::PersianEra, 1);
}

TEST(PersianCalendarBatch32Test, GetEra) {
    PersianCalendar pc;
    EXPECT_EQ(pc.GetEra(System::DateTime(2024, 1, 1)), PersianCalendar::PersianEra);
}

TEST(PersianCalendarBatch32Test, Eras_ContainsPersianEra) {
    PersianCalendar pc;
    auto eras = pc.getErasProperty();
    ASSERT_EQ(eras.size(), 1u);
    EXPECT_EQ(eras[0], PersianCalendar::PersianEra);
}

TEST(PersianCalendarBatch32Test, GetYear_KnownDate) {
    PersianCalendar pc;
    // 2024-01-01 Gregorian = 1402 or 1403 Persian (Nowruz is ~March 20)
    int py = pc.GetYear(System::DateTime(2024, 4, 1));
    EXPECT_EQ(py, 1403);
}

TEST(PersianCalendarBatch32Test, GetMonth_Range) {
    PersianCalendar pc;
    int m = pc.GetMonth(System::DateTime(2024, 6, 15));
    EXPECT_GE(m, 1);
    EXPECT_LE(m, 12);
}

TEST(PersianCalendarBatch32Test, GetDayOfMonth_Range) {
    PersianCalendar pc;
    int d = pc.GetDayOfMonth(System::DateTime(2024, 3, 25));
    EXPECT_GE(d, 1);
    EXPECT_LE(d, 31);
}

TEST(PersianCalendarBatch32Test, IsLeapYear_Known) {
    PersianCalendar pc;
    // Persian year 1403 is a leap year ((1403*8+29)%33 = 11260%33 = 16 — not < 8? let's check)
    // Actually Persian 1399 is a known leap year
    EXPECT_TRUE(pc.IsLeapYear(1399));
    EXPECT_FALSE(pc.IsLeapYear(1400));
}

TEST(PersianCalendarBatch32Test, GetDaysInMonth_FirstSix) {
    PersianCalendar pc;
    for (int m = 1; m <= 6; ++m)
        EXPECT_EQ(pc.GetDaysInMonth(1403, m), 31);
}

TEST(PersianCalendarBatch32Test, GetDaysInMonth_Months7to11) {
    PersianCalendar pc;
    for (int m = 7; m <= 11; ++m)
        EXPECT_EQ(pc.GetDaysInMonth(1403, m), 30);
}

TEST(PersianCalendarBatch32Test, GetDaysInMonth_Month12) {
    PersianCalendar pc;
    // 1399 is leap → 30 days in month 12; 1400 is common → 29
    EXPECT_EQ(pc.GetDaysInMonth(1399, 12), 30);
    EXPECT_EQ(pc.GetDaysInMonth(1400, 12), 29);
}

TEST(PersianCalendarBatch32Test, GetDaysInYear) {
    PersianCalendar pc;
    EXPECT_EQ(pc.GetDaysInYear(1399), 366);
    EXPECT_EQ(pc.GetDaysInYear(1400), 365);
}

TEST(PersianCalendarBatch32Test, GetDaysInMonth_OutOfRange) {
    PersianCalendar pc;
    EXPECT_THROW(pc.GetDaysInMonth(1403, 0),  System::ArgumentOutOfRangeException);
    EXPECT_THROW(pc.GetDaysInMonth(1403, 13), System::ArgumentOutOfRangeException);
}

// ===========================================================================
// RegionInfo
// ===========================================================================

TEST(RegionInfoBatch32Test, Constructor) {
    RegionInfo r("US");
    EXPECT_EQ(r.getNameProperty(), "US");
}

TEST(RegionInfoBatch32Test, TwoLetterCode) {
    RegionInfo r("DE");
    EXPECT_EQ(r.getTwoLetterISORegionNameProperty(), "DE");
}

TEST(RegionInfoBatch32Test, ThreeLetterCode) {
    RegionInfo r("GBR");
    EXPECT_EQ(r.getThreeLetterISORegionNameProperty(), "GBR");
}

TEST(RegionInfoBatch32Test, CurrencyDefaults) {
    RegionInfo r("US");
    EXPECT_EQ(r.getCurrencySymbolProperty(),    "$");
    EXPECT_EQ(r.getISOCurrencySymbolProperty(), "USD");
}

TEST(RegionInfoBatch32Test, IsMetric) {
    RegionInfo r("US");
    EXPECT_TRUE(r.getIsMetricProperty());
}

TEST(RegionInfoBatch32Test, CurrentRegion) {
    const auto& cr = RegionInfo::CurrentRegion();
    EXPECT_EQ(cr.getNameProperty(), "US");
}

// ===========================================================================
// SortKey
// ===========================================================================

TEST(SortKeyBatch32Test, DefaultCtor) {
    SortKey sk;
    EXPECT_EQ(sk.getOriginalStringProperty(), "");
    EXPECT_TRUE(sk.getKeyDataProperty().empty());
}

TEST(SortKeyBatch32Test, ParameterisedCtor) {
    using SharpRuntime::bytecs;
    std::vector<bytecs> data = {0x61, 0x62};
    SortKey sk("ab", data);
    EXPECT_EQ(sk.getOriginalStringProperty(), "ab");
    EXPECT_EQ(sk.getKeyDataProperty(), data);
}

TEST(SortKeyBatch32Test, Compare_Equal) {
    using SharpRuntime::bytecs;
    std::vector<bytecs> d = {1, 2, 3};
    SortKey a("x", d), b("x", d);
    EXPECT_EQ(SortKey::Compare(a, b), 0);
}

TEST(SortKeyBatch32Test, Compare_Less) {
    using SharpRuntime::bytecs;
    SortKey a("a", {1}), b("b", {2});
    EXPECT_LT(SortKey::Compare(a, b), 0);
}

TEST(SortKeyBatch32Test, Compare_Greater) {
    using SharpRuntime::bytecs;
    SortKey a("b", {2}), b("a", {1});
    EXPECT_GT(SortKey::Compare(a, b), 0);
}

TEST(SortKeyBatch32Test, Compare_ShorterIsLess) {
    using SharpRuntime::bytecs;
    SortKey a("a", {1}), b("ab", {1, 2});
    EXPECT_LT(SortKey::Compare(a, b), 0);
}

TEST(SortKeyBatch32Test, EqualityOperator) {
    using SharpRuntime::bytecs;
    std::vector<bytecs> d = {5};
    SortKey a("t", d), b("t", d), c("t", {6});
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);
}

TEST(SortKeyBatch32Test, ToString) {
    using SharpRuntime::bytecs;
    SortKey sk("hello", {});
    EXPECT_EQ(sk.ToString(), "SortKey - hello");
}

// ===========================================================================
// SortVersion
// ===========================================================================

TEST(SortVersionBatch32Test, SingleArgCtor) {
    SortVersion sv(42);
    EXPECT_EQ(sv.getFullVersionProperty(), 42);
    for (auto b : sv.getSortIdProperty()) EXPECT_EQ(b, 0u);
}

TEST(SortVersionBatch32Test, TwoArgCtor) {
    std::array<uint8_t,16> id{};
    id[0] = 0xAB;
    SortVersion sv(100, id);
    EXPECT_EQ(sv.getFullVersionProperty(), 100);
    EXPECT_EQ(sv.getSortIdProperty()[0], 0xABu);
}

TEST(SortVersionBatch32Test, EqualityOperator) {
    SortVersion a(1), b(1), c(2);
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);
}

TEST(SortVersionBatch32Test, InequalityOperator) {
    SortVersion a(1), b(2);
    EXPECT_TRUE(a != b);
    EXPECT_FALSE(a != SortVersion(1));
}
