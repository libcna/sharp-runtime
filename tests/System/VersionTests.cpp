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

// ---------------------------------------------------------------------------
// Parse / TryParse / CompareTo / Equals
// ---------------------------------------------------------------------------

TEST(VersionTests, Parse_ValidString) {
    Version v = Version::Parse("3.5.1.9");
    EXPECT_EQ(v.Major, 3);
    EXPECT_EQ(v.Minor, 5);
    EXPECT_EQ(v.Build, 1);
    EXPECT_EQ(v.Revision, 9);
}

TEST(VersionTests, Parse_TwoParts) {
    Version v = Version::Parse("1.0");
    EXPECT_EQ(v.Major, 1);
    EXPECT_EQ(v.Minor, 0);
}

TEST(VersionTests, TryParse_Valid_ReturnsTrue) {
    Version v;
    EXPECT_TRUE(Version::TryParse("2.4.6", v));
    EXPECT_EQ(v.Major, 2);
    EXPECT_EQ(v.Minor, 4);
    EXPECT_EQ(v.Build, 6);
}

TEST(VersionTests, TryParse_Invalid_ReturnsFalse) {
    Version v(9, 9);
    EXPECT_FALSE(Version::TryParse("not.a.version!", v));
}

TEST(VersionTests, CompareTo_Equal_ReturnsZero) {
    Version a(1, 2, 3), b(1, 2, 3);
    EXPECT_EQ(a.CompareTo(b), 0);
}

TEST(VersionTests, CompareTo_Less_ReturnsNegative) {
    Version a(1, 0), b(2, 0);
    EXPECT_LT(a.CompareTo(b), 0);
}

TEST(VersionTests, CompareTo_Greater_ReturnsPositive) {
    Version a(3, 0), b(1, 5);
    EXPECT_GT(a.CompareTo(b), 0);
}

TEST(VersionTests, Equals_Same_True) {
    EXPECT_TRUE(Version(1, 2, 3, 4).Equals(Version(1, 2, 3, 4)));
}

TEST(VersionTests, Equals_Different_False) {
    EXPECT_FALSE(Version(1, 2).Equals(Version(1, 3)));
}

// ---------------------------------------------------------------------------
// MajorRevision / MinorRevision
// ---------------------------------------------------------------------------

TEST(VersionTests, MajorRevision_HighBits) {
    // Revision = 0x00020003 => MajorRevision = 0x0002 = 2
    Version v(1, 0, 0, (2 << 16) | 3);
    EXPECT_EQ(v.getMajorRevisionProperty(), static_cast<short>(2));
}

TEST(VersionTests, MinorRevision_LowBits) {
    // Revision = 0x00020003 => MinorRevision = 0x0003 = 3
    Version v(1, 0, 0, (2 << 16) | 3);
    EXPECT_EQ(v.getMinorRevisionProperty(), static_cast<short>(3));
}

TEST(VersionTests, MajorRevision_NoRevision) {
    Version v(1, 2, 3, 0);
    EXPECT_EQ(v.getMajorRevisionProperty(), static_cast<short>(0));
    EXPECT_EQ(v.getMinorRevisionProperty(), static_cast<short>(0));
}

// ---------------------------------------------------------------------------
// GetHashCode
// ---------------------------------------------------------------------------

TEST(VersionTests, GetHashCode_SameVersion_SameHash) {
    Version a(1, 2, 3, 4), b(1, 2, 3, 4);
    EXPECT_EQ(a.GetHashCode(), b.GetHashCode());
}

TEST(VersionTests, GetHashCode_DifferentVersions_DifferentHash) {
    EXPECT_NE(Version(1, 2, 3, 4).GetHashCode(), Version(1, 2, 3, 5).GetHashCode());
}

TEST(VersionTests, GetHashCode_ZeroVersion_Zero) {
    EXPECT_EQ(Version(0, 0, 0, 0).GetHashCode(), 0);
}

// ---------------------------------------------------------------------------
// ToString(fieldCount)
// ---------------------------------------------------------------------------

TEST(VersionTests, ToString_FieldCount0_Empty) {
    EXPECT_EQ(Version(1, 2, 3, 4).ToString(0), "");
}

TEST(VersionTests, ToString_FieldCount1_MajorOnly) {
    EXPECT_EQ(Version(5, 6, 7, 8).ToString(1), "5");
}

TEST(VersionTests, ToString_FieldCount2_MajorMinor) {
    EXPECT_EQ(Version(1, 2, 3, 4).ToString(2), "1.2");
}

TEST(VersionTests, ToString_FieldCount3_MajorMinorBuild) {
    EXPECT_EQ(Version(1, 2, 3, 4).ToString(3), "1.2.3");
}

TEST(VersionTests, ToString_FieldCount4_All) {
    EXPECT_EQ(Version(1, 2, 3, 4).ToString(4), "1.2.3.4");
}

TEST(VersionTests, ToString_FieldCountNegative_Throws) {
    EXPECT_THROW(Version(1, 2, 3, 4).ToString(-1), std::invalid_argument);
}

TEST(VersionTests, ToString_FieldCount5_Throws) {
    EXPECT_THROW(Version(1, 2, 3, 4).ToString(5), std::invalid_argument);
}
