// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Tests for Batch 27:
//   CompareInfo:           getLCIDProperty, all comparison methods
//   CompareOptions:        enum values, operator|, operator&
//   CultureInfo:           int ctor, CurrentUICulture, all properties
//   CultureNotFoundException: all ctors, getInvalidCultureIdProperty
#include <gtest/gtest.h>
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Globalization/CompareInfo.hpp"
#include "System/Globalization/CompareOptions.hpp"
#include "System/Globalization/CultureInfo.hpp"
#include "System/Globalization/CultureNotFoundException.hpp"
#include <string>

using System::Globalization::CompareInfo;
using System::Globalization::CompareOptions;
using System::Globalization::SortKey;
using System::Globalization::CultureInfo;
using System::Globalization::CultureNotFoundException;

// ===========================================================================
// CompareOptions
// ===========================================================================

TEST(CompareOptionsBatch27Test, Values) {
    EXPECT_EQ(static_cast<int>(CompareOptions::None),             0x00000000);
    EXPECT_EQ(static_cast<int>(CompareOptions::IgnoreCase),       0x00000001);
    EXPECT_EQ(static_cast<int>(CompareOptions::Ordinal),          0x40000000);
    EXPECT_EQ(static_cast<int>(CompareOptions::OrdinalIgnoreCase), 0x10000000);
}

TEST(CompareOptionsBatch27Test, OperatorOr) {
    auto combined = CompareOptions::IgnoreCase | CompareOptions::IgnoreNonSpace;
    EXPECT_EQ(static_cast<int>(combined), 0x03);
}

TEST(CompareOptionsBatch27Test, OperatorAnd) {
    auto opts = CompareOptions::IgnoreCase | CompareOptions::IgnoreWidth;
    auto masked = opts & CompareOptions::IgnoreCase;
    EXPECT_EQ(masked, CompareOptions::IgnoreCase);
}

// ===========================================================================
// CompareInfo
// ===========================================================================

TEST(CompareInfoBatch27Test, GetCompareInfo_ByName) {
    auto ci = CompareInfo::GetCompareInfo("fr-FR");
    EXPECT_EQ(ci.getNameProperty(), "fr-FR");
}

TEST(CompareInfoBatch27Test, GetCompareInfo_ByLCID) {
    auto ci = CompareInfo::GetCompareInfo(1033);
    EXPECT_EQ(ci.getNameProperty(), "en-US");
}

TEST(CompareInfoBatch27Test, LCIDProperty) {
    auto ci = CompareInfo::GetCompareInfo("en-US");
    EXPECT_EQ(ci.getLCIDProperty(), 0);
}

TEST(CompareInfoBatch27Test, Compare_CaseSensitive) {
    CompareInfo ci("en-US");
    EXPECT_LT(ci.Compare("abc", "abd"), 0);
    EXPECT_EQ(ci.Compare("abc", "abc"), 0);
    EXPECT_GT(ci.Compare("b", "a"), 0);
}

TEST(CompareInfoBatch27Test, Compare_IgnoreCase) {
    CompareInfo ci("en-US");
    EXPECT_EQ(ci.Compare("Hello", "hello", CompareOptions::IgnoreCase), 0);
    EXPECT_NE(ci.Compare("Hello", "hello"), 0);
}

TEST(CompareInfoBatch27Test, Compare_SubstringOverload) {
    CompareInfo ci("en-US");
    EXPECT_EQ(ci.Compare("hello world", 6, 5, "world", 0, 5), 0);
}

TEST(CompareInfoBatch27Test, Compare_SubstringOverload_OutOfRange_Throws) {
    CompareInfo ci("en-US");
    EXPECT_THROW(ci.Compare("hello", 3, 10, "world", 0, 5), System::ArgumentOutOfRangeException);
    EXPECT_THROW(ci.Compare("hello", -1, 2, "world", 0, 5), System::ArgumentOutOfRangeException);
    EXPECT_THROW(ci.Compare("hello", 0, 2, "world", 3, 10), System::ArgumentOutOfRangeException);
}

TEST(CompareInfoBatch27Test, IsPrefix_IsSuffix) {
    CompareInfo ci("en-US");
    EXPECT_TRUE(ci.IsPrefix("hello", "hel"));
    EXPECT_FALSE(ci.IsPrefix("hello", "world"));
    EXPECT_TRUE(ci.IsSuffix("hello", "llo"));
    EXPECT_FALSE(ci.IsSuffix("hello", "hel"));
}

TEST(CompareInfoBatch27Test, IsPrefix_IgnoreCase) {
    CompareInfo ci("en-US");
    EXPECT_TRUE(ci.IsPrefix("Hello", "HELL", CompareOptions::IgnoreCase));
}

TEST(CompareInfoBatch27Test, IndexOf_LastIndexOf) {
    CompareInfo ci("en-US");
    EXPECT_EQ(ci.IndexOf("abcabc", "bc"), 1);
    EXPECT_EQ(ci.LastIndexOf("abcabc", "bc"), 4);
    EXPECT_EQ(ci.IndexOf("abcabc", "xyz"), -1);
}

TEST(CompareInfoBatch27Test, IndexOf_IgnoreCase) {
    CompareInfo ci("en-US");
    EXPECT_EQ(ci.IndexOf("Hello World", "world", CompareOptions::IgnoreCase), 6);
}

TEST(CompareInfoBatch27Test, IsSortable) {
    EXPECT_TRUE(CompareInfo::IsSortable(u'A'));
    EXPECT_TRUE(CompareInfo::IsSortable("anything"));
}

TEST(CompareInfoBatch27Test, GetSortKey) {
    CompareInfo ci("en-US");
    auto sk = ci.GetSortKey("test");
    EXPECT_EQ(sk.getOriginalStringProperty(), "test");
}

TEST(CompareInfoBatch27Test, GetSortKey_IgnoreCase_ProducesEqualKeys) {
    // Sort keys must be consistent with Compare: strings equal under IgnoreCase
    // should produce equal sort keys, matching the .NET contract.
    CompareInfo ci("en-US");
    auto skLower = ci.GetSortKey("hello", CompareOptions::IgnoreCase);
    auto skUpper = ci.GetSortKey("HELLO", CompareOptions::IgnoreCase);
    EXPECT_EQ(SortKey::Compare(skLower, skUpper), 0);
}

TEST(CompareInfoBatch27Test, GetHashCode) {
    CompareInfo ci("en-US");
    EXPECT_EQ(ci.GetHashCode("same", CompareOptions::None),
              ci.GetHashCode("same", CompareOptions::None));
}

TEST(CompareInfoBatch27Test, GetHashCode_IgnoreCase_MatchesAcrossCase) {
    CompareInfo ci("en-US");
    EXPECT_EQ(ci.GetHashCode("Hello", CompareOptions::IgnoreCase),
              ci.GetHashCode("hello", CompareOptions::IgnoreCase));
}

TEST(CompareInfoBatch27Test, EqualityAndToString) {
    CompareInfo a("en-US"), b("en-US"), c("fr-FR");
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);
    EXPECT_EQ(a.ToString(), "CompareInfo - en-US");
}

// ===========================================================================
// CultureInfo
// ===========================================================================

TEST(CultureInfoBatch27Test, DefaultCtor) {
    CultureInfo ci;
    EXPECT_EQ(ci.getNameProperty(), "");
    EXPECT_FALSE(ci.getIsNeutralCultureProperty());
    EXPECT_FALSE(ci.getIsReadOnlyProperty());
}

TEST(CultureInfoBatch27Test, StringCtor) {
    CultureInfo ci("de-DE");
    EXPECT_EQ(ci.getNameProperty(), "de-DE");
}

TEST(CultureInfoBatch27Test, IntCtor) {
    CultureInfo ci(1033);
    EXPECT_EQ(ci.getNameProperty(), "en-US");
}

TEST(CultureInfoBatch27Test, InvariantCulture) {
    const auto& inv = CultureInfo::InvariantCulture();
    EXPECT_EQ(inv.getNameProperty(), "");
    EXPECT_TRUE(inv.getIsNeutralCultureProperty());
    EXPECT_TRUE(inv.getIsReadOnlyProperty());
}

TEST(CultureInfoBatch27Test, CurrentCulture_ReturnsInvariant) {
    const auto& cur = CultureInfo::CurrentCulture();
    EXPECT_TRUE(cur.getIsNeutralCultureProperty());
}

TEST(CultureInfoBatch27Test, CurrentUICulture_ReturnsInvariant) {
    const auto& ui = CultureInfo::CurrentUICulture();
    EXPECT_TRUE(ui.getIsNeutralCultureProperty());
}

// ===========================================================================
// CultureNotFoundException
// ===========================================================================

TEST(CultureNotFoundExceptionBatch27Test, DefaultCtor) {
    CultureNotFoundException ex;
    EXPECT_NE(std::string(ex.what()).find("not supported"), std::string::npos);
    EXPECT_EQ(ex.getInvalidCultureNameProperty(), "");
    EXPECT_EQ(ex.getInvalidCultureIdProperty(), -1);
}

TEST(CultureNotFoundExceptionBatch27Test, MessageCtor) {
    CultureNotFoundException ex("bad culture");
    EXPECT_NE(std::string(ex.what()).find("bad culture"), std::string::npos);
}

TEST(CultureNotFoundExceptionBatch27Test, MessageAndNameCtor) {
    CultureNotFoundException ex("not found", "xx-XX");
    EXPECT_EQ(ex.getInvalidCultureNameProperty(), "xx-XX");
}

TEST(CultureNotFoundExceptionBatch27Test, MessageAndIdAndInnerCtor) {
    CultureNotFoundException ex("bad id", 9999, std::exception_ptr{});
    EXPECT_EQ(ex.getInvalidCultureIdProperty(), 9999);
}

TEST(CultureNotFoundExceptionBatch27Test, ParamNameAndIdAndMessageCtor) {
    CultureNotFoundException ex("param", 1234, "culture error");
    EXPECT_EQ(ex.getInvalidCultureIdProperty(), 1234);
}
