// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/Char.hpp"

using System::Char;
using SharpRuntime::charcs;

// ---------------------------------------------------------------------------
// ToUpperInvariant / ToLowerInvariant
// ---------------------------------------------------------------------------

TEST(CharTests2, ToUpperInvariant_Lower_ReturnsUpper) {
    EXPECT_EQ(Char::ToUpperInvariant(u'a'), u'A');
    EXPECT_EQ(Char::ToUpperInvariant(u'z'), u'Z');
}

TEST(CharTests2, ToUpperInvariant_AlreadyUpper_Unchanged) {
    EXPECT_EQ(Char::ToUpperInvariant(u'A'), u'A');
}

TEST(CharTests2, ToUpperInvariant_NonLetter_Unchanged) {
    EXPECT_EQ(Char::ToUpperInvariant(u'5'), u'5');
}

TEST(CharTests2, ToLowerInvariant_Upper_ReturnsLower) {
    EXPECT_EQ(Char::ToLowerInvariant(u'A'), u'a');
    EXPECT_EQ(Char::ToLowerInvariant(u'Z'), u'z');
}

TEST(CharTests2, ToLowerInvariant_AlreadyLower_Unchanged) {
    EXPECT_EQ(Char::ToLowerInvariant(u'a'), u'a');
}

TEST(CharTests2, ToLowerInvariant_NonLetter_Unchanged) {
    EXPECT_EQ(Char::ToLowerInvariant(u'9'), u'9');
}

// ---------------------------------------------------------------------------
// CompareTo / Equals
// ---------------------------------------------------------------------------

TEST(CharTests2, CompareTo_Equal_Zero) {
    EXPECT_EQ(Char::CompareTo(u'A', u'A'), 0);
}

TEST(CharTests2, CompareTo_Less_Negative) {
    EXPECT_LT(Char::CompareTo(u'A', u'B'), 0);
}

TEST(CharTests2, CompareTo_Greater_Positive) {
    EXPECT_GT(Char::CompareTo(u'B', u'A'), 0);
}

TEST(CharTests2, Equals_SameChar_True) {
    EXPECT_TRUE(Char::Equals(u'X', u'X'));
}

TEST(CharTests2, Equals_DiffChar_False) {
    EXPECT_FALSE(Char::Equals(u'X', u'Y'));
}

// ---------------------------------------------------------------------------
// GetHashCode
// ---------------------------------------------------------------------------

TEST(CharTests2, GetHashCode_EqualsCodeUnit) {
    EXPECT_EQ(Char::GetHashCode(u'A'), static_cast<int>(u'A'));
}

TEST(CharTests2, GetHashCode_Zero_IsZero) {
    EXPECT_EQ(Char::GetHashCode(u'\0'), 0);
}

// ---------------------------------------------------------------------------
// TryParse
// ---------------------------------------------------------------------------

TEST(CharTests2, TryParse_Valid_ReturnsTrue) {
    charcs r = u'\0';
    EXPECT_TRUE(Char::TryParse("A", r));
    EXPECT_EQ(r, u'A');
}

TEST(CharTests2, TryParse_Empty_ReturnsFalse) {
    charcs r = u'X';
    EXPECT_FALSE(Char::TryParse("", r));
    EXPECT_EQ(r, u'\0');
}

TEST(CharTests2, TryParse_MultiChar_ReturnsFalse) {
    charcs r = u'\0';
    EXPECT_FALSE(Char::TryParse("AB", r));
}
