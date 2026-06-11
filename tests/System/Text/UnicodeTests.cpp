// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Tests for System::Text::Unicode::UnicodeRange and UnicodeRanges.
#include <gtest/gtest.h>
#include "System/Text/Unicode/UnicodeRange.hpp"
#include "System/Text/Unicode/UnicodeRanges.hpp"

using System::Text::Unicode::UnicodeRange;
using System::Text::Unicode::UnicodeRanges;

// ===========================================================================
// UnicodeRange
// ===========================================================================

TEST(UnicodeRangeTests, Constructor_StoresFirstCodePointAndLength) {
    UnicodeRange r(0x0041, 26);
    EXPECT_EQ(r.getFirstCodePointProperty(), 0x0041);
    EXPECT_EQ(r.getLengthProperty(), 26);
}

TEST(UnicodeRangeTests, Constructor_ZeroLength_IsValid) {
    UnicodeRange r(0x0000, 0);
    EXPECT_EQ(r.getFirstCodePointProperty(), 0x0000);
    EXPECT_EQ(r.getLengthProperty(), 0);
}

TEST(UnicodeRangeTests, Constructor_MaxBMP_IsValid) {
    // 0xFFFF is last BMP code point; length 1 puts end at 0x10000 which is within bounds
    UnicodeRange r(0xFFFF, 1);
    EXPECT_EQ(r.getFirstCodePointProperty(), 0xFFFF);
    EXPECT_EQ(r.getLengthProperty(), 1);
}

TEST(UnicodeRangeTests, Constructor_NegativeFirstCodePoint_Throws) {
    EXPECT_THROW((UnicodeRange(-1, 10)), std::out_of_range);
}

TEST(UnicodeRangeTests, Constructor_FirstCodePointAboveBMP_Throws) {
    EXPECT_THROW((UnicodeRange(0x10000, 1)), std::out_of_range);
}

TEST(UnicodeRangeTests, Constructor_LengthOverflow_Throws) {
    EXPECT_THROW((UnicodeRange(0xFF00, 0x200)), std::out_of_range);
}

TEST(UnicodeRangeTests, Create_BasicLatin) {
    UnicodeRange r = UnicodeRange::Create(u'A', u'Z'); // A–Z
    EXPECT_EQ(r.getFirstCodePointProperty(), 0x0041);
    EXPECT_EQ(r.getLengthProperty(), 26);
}

TEST(UnicodeRangeTests, Create_SingleChar) {
    UnicodeRange r = UnicodeRange::Create(u' ', u' ');
    EXPECT_EQ(r.getFirstCodePointProperty(), 0x0020);
    EXPECT_EQ(r.getLengthProperty(), 1);
}

TEST(UnicodeRangeTests, Create_FirstGreaterThanLast_Throws) {
    EXPECT_THROW(UnicodeRange::Create(u'`', u'A'), std::invalid_argument);
}

// ===========================================================================
// UnicodeRanges
// ===========================================================================

TEST(UnicodeRangesTests, All_StartsAtZero_LengthIs64K) {
    UnicodeRange r = UnicodeRanges::All();
    EXPECT_EQ(r.getFirstCodePointProperty(), 0x0000);
    EXPECT_EQ(r.getLengthProperty(), 0x10000);
}

TEST(UnicodeRangesTests, None_LengthIsZero) {
    EXPECT_EQ(UnicodeRanges::None().getLengthProperty(), 0);
}

TEST(UnicodeRangesTests, BasicLatin_StartsAt0_Length128) {
    UnicodeRange r = UnicodeRanges::BasicLatin();
    EXPECT_EQ(r.getFirstCodePointProperty(), 0x0000);
    EXPECT_EQ(r.getLengthProperty(), 128);
}

TEST(UnicodeRangesTests, Latin1Supplement_StartsAt0x80_Length128) {
    UnicodeRange r = UnicodeRanges::Latin1Supplement();
    EXPECT_EQ(r.getFirstCodePointProperty(), 0x0080);
    EXPECT_EQ(r.getLengthProperty(), 128);
}

TEST(UnicodeRangesTests, Cyrillic_StartsAt0x0400_Length256) {
    UnicodeRange r = UnicodeRanges::Cyrillic();
    EXPECT_EQ(r.getFirstCodePointProperty(), 0x0400);
    EXPECT_EQ(r.getLengthProperty(), 256);
}

TEST(UnicodeRangesTests, CJKUnifiedIdeographs_StartsAt0x4E00) {
    UnicodeRange r = UnicodeRanges::CJKUnifiedIdeographs();
    EXPECT_EQ(r.getFirstCodePointProperty(), 0x4E00);
    EXPECT_GT(r.getLengthProperty(), 0);
}

TEST(UnicodeRangesTests, HangulSyllables_StartsAt0xAC00) {
    UnicodeRange r = UnicodeRanges::HangulSyllables();
    EXPECT_EQ(r.getFirstCodePointProperty(), 0xAC00);
}

TEST(UnicodeRangesTests, Specials_StartsAt0xFFF0_Length16) {
    UnicodeRange r = UnicodeRanges::Specials();
    EXPECT_EQ(r.getFirstCodePointProperty(), 0xFFF0);
    EXPECT_EQ(r.getLengthProperty(), 16);
}

TEST(UnicodeRangesTests, Greek_StartsAt0x0370) {
    UnicodeRange r = UnicodeRanges::GreekandCoptic();
    EXPECT_EQ(r.getFirstCodePointProperty(), 0x0370);
}
