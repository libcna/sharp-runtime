// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// The culture data table (System/Globalization/detail/CultureData.hpp) and the three places that
// read it: CultureInfo::EnglishName, CultureInfo::TextInfo / TextInfo::ListSeparator, and the
// separators of CultureInfo::NumberFormat.
#include <gtest/gtest.h>
#include <string>
#include "System/Globalization/CultureInfo.hpp"
#include "System/Globalization/NumberFormatInfo.hpp"
#include "System/Globalization/TextInfo.hpp"
#include "System/Globalization/detail/CultureData.hpp"
#include "System/InvalidOperationException.hpp"

using System::Globalization::CultureInfo;
using System::Globalization::NumberFormatInfo;
using System::Globalization::TextInfo;
namespace detail = System::Globalization::detail;

namespace {
const std::string kNbsp = "\xc2\xa0";
const std::string kNarrowNbsp = "\xe2\x80\xaf";
}

// ---------------------------------------------------------------------------
// The table itself
// ---------------------------------------------------------------------------

TEST(CultureDataTest, InvariantRowIsReturnedForTheEmptyName) {
    const auto* row = detail::FindCultureData("");
    ASSERT_NE(row, nullptr);
    EXPECT_EQ(row->listSeparator, ",");
    EXPECT_EQ(row->numberDecimalSeparator, ".");
    EXPECT_EQ(row->numberGroupSeparator, ",");
}

TEST(CultureDataTest, LookupIsAsciiCaseInsensitive) {
    const auto* lower = detail::FindCultureData("cs-cz");
    const auto* upper = detail::FindCultureData("CS-CZ");
    ASSERT_NE(lower, nullptr);
    EXPECT_EQ(lower, upper);
    EXPECT_EQ(lower->name, "cs-CZ");
}

TEST(CultureDataTest, UnlistedNameYieldsNullAndFallsBackToInvariant) {
    EXPECT_EQ(detail::FindCultureData("xx-YY"), nullptr);
    EXPECT_EQ(&detail::CultureDataOrInvariant("xx-YY"), &detail::kInvariantCultureData);
}

TEST(CultureDataTest, TableIsSortedByNameAndEveryRowIsComplete) {
    std::string previous;
    for (const auto& row : detail::kCultureData) {
        EXPECT_LT(previous, std::string(row.name)) << row.name;
        previous = std::string(row.name);
        EXPECT_FALSE(row.englishName.empty()) << row.name;
        EXPECT_FALSE(row.listSeparator.empty()) << row.name;
        EXPECT_FALSE(row.numberDecimalSeparator.empty()) << row.name;
        EXPECT_FALSE(row.numberGroupSeparator.empty()) << row.name;
        // A comma-decimal culture never lists with a comma, and a dot-decimal one never lists
        // with a semicolon, in every CLDR locale this table carries.
        if (row.numberDecimalSeparator == ",") EXPECT_EQ(row.listSeparator, ";") << row.name;
        else EXPECT_EQ(row.listSeparator, ",") << row.name;
        EXPECT_NE(row.numberDecimalSeparator, row.numberGroupSeparator) << row.name;
    }
}

// ---------------------------------------------------------------------------
// TextInfo::ListSeparator
// ---------------------------------------------------------------------------

TEST(CultureDataTest, TextInfoListSeparatorComesFromTheTable) {
    EXPECT_EQ(TextInfo("en-US").getListSeparatorProperty(), ",");
    EXPECT_EQ(TextInfo("en-GB").getListSeparatorProperty(), ",");
    EXPECT_EQ(TextInfo("cs-CZ").getListSeparatorProperty(), ";");
    EXPECT_EQ(TextInfo("de-DE").getListSeparatorProperty(), ";");
    EXPECT_EQ(TextInfo("fr-FR").getListSeparatorProperty(), ";");
    EXPECT_EQ(TextInfo("ja-JP").getListSeparatorProperty(), ",");
    EXPECT_EQ(TextInfo("").getListSeparatorProperty(), ",");
    EXPECT_EQ(TextInfo("xx-YY").getListSeparatorProperty(), ",");
}

TEST(CultureDataTest, TextInfoDefaultConstructorKeepsEnUS) {
    TextInfo ti;
    EXPECT_EQ(ti.getCultureNameProperty(), "en-US");
    EXPECT_EQ(ti.getListSeparatorProperty(), ",");
}

// ---------------------------------------------------------------------------
// CultureInfo::TextInfo
// ---------------------------------------------------------------------------

TEST(CultureDataTest, CultureInfoExposesItsTextInfo) {
    const CultureInfo czech = CultureInfo::GetCultureInfo("cs-CZ");
    EXPECT_EQ(czech.getTextInfoProperty().getCultureNameProperty(), "cs-CZ");
    EXPECT_EQ(czech.getTextInfoProperty().getListSeparatorProperty(), ";");
    EXPECT_EQ(CultureInfo::getInvariantCultureProperty().getTextInfoProperty().getListSeparatorProperty(), ",");
    EXPECT_EQ(CultureInfo("en-US").getTextInfoProperty().getListSeparatorProperty(), ",");
}

TEST(CultureDataTest, ReadOnlyCultureHasReadOnlyTextInfo) {
    const CultureInfo readOnly = CultureInfo::GetCultureInfo("de-DE");
    EXPECT_TRUE(readOnly.getTextInfoProperty().getIsReadOnlyProperty());
    CultureInfo writable("de-DE");
    EXPECT_FALSE(writable.getTextInfoProperty().getIsReadOnlyProperty());
    EXPECT_FALSE(writable.getNumberFormatProperty().getIsReadOnlyProperty());
    EXPECT_TRUE(readOnly.getNumberFormatProperty().getIsReadOnlyProperty());
}

// ---------------------------------------------------------------------------
// CultureInfo::NumberFormat separators
// ---------------------------------------------------------------------------

TEST(CultureDataTest, NumberFormatSeparatorsComeFromTheTable) {
    const CultureInfo czech = CultureInfo::GetCultureInfo("cs-CZ");
    const NumberFormatInfo& cs = czech.getNumberFormatProperty();
    EXPECT_EQ(cs.getNumberDecimalSeparatorProperty(), ",");
    EXPECT_EQ(cs.getNumberGroupSeparatorProperty(), kNbsp);
    EXPECT_EQ(cs.getCurrencyDecimalSeparatorProperty(), ",");
    EXPECT_EQ(cs.getCurrencyGroupSeparatorProperty(), kNbsp);
    EXPECT_EQ(cs.getPercentDecimalSeparatorProperty(), ",");
    EXPECT_EQ(cs.getPercentGroupSeparatorProperty(), kNbsp);

    const CultureInfo german("de-DE");
    const NumberFormatInfo& de = german.getNumberFormatProperty();
    EXPECT_EQ(de.getNumberDecimalSeparatorProperty(), ",");
    EXPECT_EQ(de.getNumberGroupSeparatorProperty(), ".");

    const CultureInfo french("fr-FR");
    const NumberFormatInfo& fr = french.getNumberFormatProperty();
    EXPECT_EQ(fr.getNumberDecimalSeparatorProperty(), ",");
    EXPECT_EQ(fr.getNumberGroupSeparatorProperty(), kNarrowNbsp);

    const CultureInfo english("en-US");
    const NumberFormatInfo& en = english.getNumberFormatProperty();
    EXPECT_EQ(en.getNumberDecimalSeparatorProperty(), ".");
    EXPECT_EQ(en.getNumberGroupSeparatorProperty(), ",");
}

TEST(CultureDataTest, EverythingTheTableDoesNotCoverStaysInvariant) {
    const CultureInfo czech("cs-CZ");
    const NumberFormatInfo& cs = czech.getNumberFormatProperty();
    const NumberFormatInfo& inv = NumberFormatInfo::getInvariantInfoProperty();
    EXPECT_EQ(cs.getNegativeSignProperty(), inv.getNegativeSignProperty());
    EXPECT_EQ(cs.getPositiveSignProperty(), inv.getPositiveSignProperty());
    EXPECT_EQ(cs.getCurrencySymbolProperty(), inv.getCurrencySymbolProperty());
    EXPECT_EQ(cs.getNaNSymbolProperty(), inv.getNaNSymbolProperty());
    EXPECT_EQ(cs.getPositiveInfinitySymbolProperty(), inv.getPositiveInfinitySymbolProperty());
    EXPECT_EQ(cs.getNumberDecimalDigitsProperty(), inv.getNumberDecimalDigitsProperty());
    EXPECT_EQ(cs.getNumberGroupSizesProperty(), inv.getNumberGroupSizesProperty());
    EXPECT_EQ(cs.getNativeDigitsProperty(), inv.getNativeDigitsProperty());
}

TEST(CultureDataTest, UnlistedCultureBehavesAsInvariant) {
    const CultureInfo unknown("xx-YY");
    EXPECT_EQ(unknown.getNumberFormatProperty().getNumberDecimalSeparatorProperty(), ".");
    EXPECT_EQ(unknown.getNumberFormatProperty().getNumberGroupSeparatorProperty(), ",");
    EXPECT_EQ(unknown.getTextInfoProperty().getListSeparatorProperty(), ",");
    EXPECT_EQ(unknown.getEnglishNameProperty(), "xx-YY");
}

TEST(CultureDataTest, GetFormatStillAnswersWithTheCultureOwnNumberFormat) {
    const CultureInfo czech = CultureInfo::GetCultureInfo("cs-CZ");
    const auto* viaProvider = static_cast<const NumberFormatInfo*>(czech.GetFormat(typeid(NumberFormatInfo)));
    ASSERT_NE(viaProvider, nullptr);
    EXPECT_EQ(viaProvider, &czech.getNumberFormatProperty());
    EXPECT_EQ(&NumberFormatInfo::GetInstance(&czech), &czech.getNumberFormatProperty());
    EXPECT_EQ(&NumberFormatInfo::GetInstance(nullptr), &NumberFormatInfo::getInvariantInfoProperty());
}

TEST(CultureDataTest, EnglishNamesForTheNewRows) {
    EXPECT_EQ(CultureInfo("cs-CZ").getEnglishNameProperty(), "Czech (Czechia)");
    EXPECT_EQ(CultureInfo("de-DE").getEnglishNameProperty(), "German (Germany)");
    EXPECT_EQ(CultureInfo("pl-PL").getEnglishNameProperty(), "Polish (Poland)");
    EXPECT_EQ(CultureInfo("en-US").getEnglishNameProperty(), "English (United States)");
}
