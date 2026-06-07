// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/Version.hpp"

using System::Version;

// ---------------------------------------------------------------------------
// Constructors
// ---------------------------------------------------------------------------

TEST(VersionTests, DefaultCtor_AllZeroOrMinusOne) {
    Version v;
    EXPECT_EQ(v.Major,    0);
    EXPECT_EQ(v.Minor,    0);
    EXPECT_EQ(v.Build,   -1);
    EXPECT_EQ(v.Revision,-1);
}

TEST(VersionTests, TwoArgCtor_MajorMinorSet) {
    Version v(1, 2);
    EXPECT_EQ(v.Major, 1);
    EXPECT_EQ(v.Minor, 2);
    EXPECT_EQ(v.Build, -1);
}

TEST(VersionTests, ThreeArgCtor_BuildSet) {
    Version v(1, 2, 3);
    EXPECT_EQ(v.Build,    3);
    EXPECT_EQ(v.Revision,-1);
}

TEST(VersionTests, FourArgCtor_RevisionSet) {
    Version v(1, 2, 3, 4);
    EXPECT_EQ(v.Major,    1);
    EXPECT_EQ(v.Minor,    2);
    EXPECT_EQ(v.Build,    3);
    EXPECT_EQ(v.Revision, 4);
}

// ---------------------------------------------------------------------------
// ToString
// ---------------------------------------------------------------------------

TEST(VersionTests, ToString_TwoParts) {
    EXPECT_EQ(Version(1, 0).ToString(), "1.0");
}

TEST(VersionTests, ToString_ThreeParts) {
    EXPECT_EQ(Version(2, 3, 4).ToString(), "2.3.4");
}

TEST(VersionTests, ToString_FourParts) {
    EXPECT_EQ(Version(1, 2, 3, 4).ToString(), "1.2.3.4");
}

TEST(VersionTests, ToString_ZeroMajorMinor) {
    EXPECT_EQ(Version(0, 0).ToString(), "0.0");
}

// ---------------------------------------------------------------------------
// Parse from string
// ---------------------------------------------------------------------------

TEST(VersionTests, ParseString_TwoParts) {
    Version v("3.7");
    EXPECT_EQ(v.Major, 3);
    EXPECT_EQ(v.Minor, 7);
}

TEST(VersionTests, ParseString_ThreeParts) {
    Version v("1.2.3");
    EXPECT_EQ(v.Build, 3);
}

TEST(VersionTests, ParseString_FourParts) {
    Version v("1.2.3.4");
    EXPECT_EQ(v.Revision, 4);
}

TEST(VersionTests, ParseString_RoundTrip) {
    std::string s = "10.20.30.40";
    EXPECT_EQ(Version(s).ToString(), s);
}

// ---------------------------------------------------------------------------
// Equality operators
// ---------------------------------------------------------------------------

TEST(VersionTests, Equality_SameVersion_IsEqual) {
    EXPECT_TRUE(Version(1, 2, 3, 4) == Version(1, 2, 3, 4));
}

TEST(VersionTests, Equality_DifferentMajor_NotEqual) {
    EXPECT_TRUE(Version(1, 0) != Version(2, 0));
}

// ---------------------------------------------------------------------------
// Comparison operators
// ---------------------------------------------------------------------------

TEST(VersionTests, LessThan_OlderMajor_IsLess) {
    EXPECT_TRUE(Version(1, 0) < Version(2, 0));
}

TEST(VersionTests, LessThan_SameMajorOlderMinor_IsLess) {
    EXPECT_TRUE(Version(1, 0) < Version(1, 1));
}

TEST(VersionTests, GreaterThan_NewerMajor_IsGreater) {
    EXPECT_TRUE(Version(3, 0) > Version(2, 9));
}

TEST(VersionTests, LessOrEqual_SameVersion) {
    EXPECT_TRUE(Version(1, 2) <= Version(1, 2));
}

TEST(VersionTests, GreaterOrEqual_SameVersion) {
    EXPECT_TRUE(Version(1, 2) >= Version(1, 2));
}
