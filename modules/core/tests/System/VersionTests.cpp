// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <limits>
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

TEST(VersionTests, ParseString_SingleComponent_Throws) {
    // .NET requires at least "major.minor" - a bare "5" is invalid.
    EXPECT_THROW(Version("5"), System::ArgumentException);
}

TEST(VersionTests, ParseString_Empty_Throws) {
    EXPECT_THROW(Version(""), System::ArgumentException);
}

TEST(VersionTests, ParseString_FiveComponents_Throws) {
    EXPECT_THROW(Version("1.2.3.4.5"), System::ArgumentException);
}

TEST(VersionTests, ParseString_NegativeComponent_Throws) {
    EXPECT_THROW(Version("1.-2"), System::ArgumentOutOfRangeException);
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

TEST(VersionTests, CompareTo_ExtremeValues_NoOverflow) {
    // Subtraction-based comparison of two large, far-apart non-negative components
    // (e.g. INT_MAX vs 0) would risk overflow in some formulations; this must use
    // direct comparison instead (matching .NET's actual CompareTo).
    Version a(std::numeric_limits<SharpRuntime::intcs>::max(), 0);
    Version b(0, 0);
    EXPECT_GT(a.CompareTo(b), 0);
    EXPECT_LT(b.CompareTo(a), 0);
}

TEST(VersionTests, Ctor_NegativeMajor_Throws) {
    EXPECT_THROW(Version(-1, 0), System::ArgumentOutOfRangeException);
}

TEST(VersionTests, Ctor_NegativeMinor_Throws) {
    EXPECT_THROW(Version(0, -1), System::ArgumentOutOfRangeException);
}

TEST(VersionTests, Ctor_NegativeBuild_Throws) {
    EXPECT_THROW(Version(1, 2, -1), System::ArgumentOutOfRangeException);
}

TEST(VersionTests, Ctor_FourArg_NegativeRevision_Throws) {
    // Unlike the 2-/3-arg overloads (where Build/Revision default to -1
    // internally), the 4-arg overload validates revision too, since it's
    // explicitly user-supplied here.
    EXPECT_THROW(Version(1, 2, 3, -1), System::ArgumentOutOfRangeException);
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

// Replaces GetHashCode_DifferentVersions_DifferentHash, which asserted that two unequal versions
// cannot collide. They can, and this is one: Version::GetHashCode packs Revision into 12 bits
// (`Revision & 0x00000FFF`, Version.hpp:120), so 4 and 4100 land on identical bits while the
// versions stay unequal. Together with GetHashCode_ZeroVersion_Zero below -- a perfectly legal
// hash code of zero -- this is the direct evidence for docs/HashAssertionContractRule.md R2/R6.
TEST(VersionTests, GetHashCode_UnequalVersionsMayCollide) {
    const Version a(1, 2, 3, 4);
    const Version wrapped(1, 2, 3, 4 + 0x1000);
    EXPECT_FALSE(a.Equals(wrapped));
    EXPECT_EQ(a.GetHashCode(), wrapped.GetHashCode());
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
    EXPECT_THROW(Version(1, 2, 3, 4).ToString(-1), System::ArgumentException);
}

TEST(VersionTests, ToString_FieldCount5_Throws) {
    EXPECT_THROW(Version(1, 2, 3, 4).ToString(5), System::ArgumentException);
}

// ---------------------------------------------------------------------------
// Parse -- trailing separator (regression: std::getline silently dropped the
// final empty component real .NET's ParseVersion rejects with FormatException)
// ---------------------------------------------------------------------------

TEST(VersionTests, Parse_TrailingDotAfterMinor_Throws) {
    EXPECT_THROW(Version("1.2."), System::FormatException);
}

TEST(VersionTests, Parse_TrailingDotAfterBuild_Throws) {
    EXPECT_THROW(Version("1.2.3."), System::FormatException);
}

TEST(VersionTests, TryParse_TrailingDot_ReturnsFalse) {
    Version result;
    EXPECT_FALSE(Version::TryParse("1.2.", result));
}

TEST(VersionTests, Parse_NoTrailingDot_StillWorks) {
    Version v("1.2.3.4");
    EXPECT_EQ(v.Major, 1);
    EXPECT_EQ(v.Minor, 2);
    EXPECT_EQ(v.Build, 3);
    EXPECT_EQ(v.Revision, 4);
}

// ---------------------------------------------------------------------------
// ToString(fieldCount) -- an undefined component may not be requested
// (SR-AUD-011, ticket #2258).  Before the repair the overload validated only
// the numeric interval 0-4 and then appended Build and Revision
// unconditionally, so Version(1, 2).ToString(3) emitted the "-1" sentinel as
// "1.2.-1".  .NET's Version.ToString(int) rejects a fieldCount that requests a
// component the instance does not define.
//
// The seven ToString(fieldCount) tests above are the unchanged control: they
// all use a fully specified four-component subject and never enter a guard.
// ---------------------------------------------------------------------------

// The finding's own three required assertions.

TEST(VersionTests, ToString_FieldCount3_OnTwoComponentVersion_Throws) {
    EXPECT_THROW(Version(1, 2).ToString(3), System::ArgumentException);
}

TEST(VersionTests, ToString_FieldCount4_OnTwoComponentVersion_Throws) {
    EXPECT_THROW(Version(1, 2).ToString(4), System::ArgumentException);
}

TEST(VersionTests, ToString_FieldCount4_OnThreeComponentVersion_Throws) {
    EXPECT_THROW(Version(1, 2, 3).ToString(4), System::ArgumentException);
}

// The default constructor is a two-component version (0.0), so it is subject to
// the same rejection.  No report or test named this instance.

TEST(VersionTests, ToString_FieldCountBeyondDefault_Throws) {
    EXPECT_THROW(Version().ToString(3), System::ArgumentException);
    EXPECT_THROW(Version().ToString(4), System::ArgumentException);
}

// parse() reaches Build == -1 / Revision == -1 by a different route than the
// defaulted member initialisers, so the parsed spellings are pinned separately.

TEST(VersionTests, ToString_FieldCountBeyondParsedComponents_Throws) {
    EXPECT_THROW(Version("1.2").ToString(3),   System::ArgumentException);
    EXPECT_THROW(Version("1.2").ToString(4),   System::ArgumentException);
    EXPECT_THROW(Version("1.2.3").ToString(4), System::ArgumentException);
}

// Exception identity, not merely the type: the parameter name, and a message
// that distinguishes the two new guards from each other AND from the
// pre-existing out-of-interval guard.  Without the message assertion a single
// over-broad guard could serve both branches and still pass.
//
// #2260 resolved the message text against the reference tree, which was absent
// when #2258 wrote these: .NET formats all three from
// SR.ArgumentOutOfRange_Bounds_Lower_Upper, "Argument must be between {0} and
// {1}." (Strings.resx:1762), with lower bound "0" and an instance-dependent
// upper bound (Version.cs:204-224).  Only the text changed -- every bound this
// port already reported was already .NET's.

TEST(VersionTests, ToString_UndefinedBuild_ExceptionIdentity) {
    try {
        (void)Version(1, 2).ToString(3);
        FAIL() << "expected System::ArgumentException";
    } catch (const System::ArgumentException& e) {
        EXPECT_EQ(e.getParamNameProperty(), "fieldCount");
        EXPECT_STREQ(e.what(), "Argument must be between 0 and 2. (Parameter 'fieldCount')");
    }
}

TEST(VersionTests, ToString_UndefinedRevision_ExceptionIdentity) {
    try {
        (void)Version(1, 2, 3).ToString(4);
        FAIL() << "expected System::ArgumentException";
    } catch (const System::ArgumentException& e) {
        EXPECT_EQ(e.getParamNameProperty(), "fieldCount");
        EXPECT_STREQ(e.what(), "Argument must be between 0 and 3. (Parameter 'fieldCount')");
    }
}

// The out-of-interval branch keeps running FIRST.  A two-component subject is
// used precisely because the new guards would otherwise be able to claim these
// inputs and report the instance-dependent bound instead.  This is also the
// case #2260 suspected of diverging: it does NOT.  .NET switches on
// (uint)fieldCount and tests `case > 4` before either component case, so
// Version(1,2).ToString(5) reports bound 4 in .NET exactly as it does here --
// the suspected bound of 2 would require the component case to run first.

TEST(VersionTests, ToString_OutOfInterval_KeepsIntervalMessageAndRunsFirst) {
    for (System::intcs fieldCount : {-1, 5}) {
        try {
            (void)Version(1, 2).ToString(fieldCount);
            FAIL() << "expected System::ArgumentException for fieldCount " << fieldCount;
        } catch (const System::ArgumentException& e) {
            EXPECT_EQ(e.getParamNameProperty(), "fieldCount");
            EXPECT_STREQ(e.what(), "Argument must be between 0 and 4. (Parameter 'fieldCount')");
        }
    }
}

// Every field count a partially specified instance CAN serve still works.

TEST(VersionTests, ToString_FieldCountsWithinTwoComponentVersion_Unchanged) {
    EXPECT_EQ(Version(1, 2).ToString(0), "");
    EXPECT_EQ(Version(1, 2).ToString(1), "1");
    EXPECT_EQ(Version(1, 2).ToString(2), "1.2");
}

TEST(VersionTests, ToString_FieldCountsWithinThreeComponentVersion_Unchanged) {
    EXPECT_EQ(Version(1, 2, 3).ToString(0), "");
    EXPECT_EQ(Version(1, 2, 3).ToString(1), "1");
    EXPECT_EQ(Version(1, 2, 3).ToString(2), "1.2");
    EXPECT_EQ(Version(1, 2, 3).ToString(3), "1.2.3");
}

// Zero is a defined component, not a sentinel: the guards test for a negative
// value, so an all-zero version must still format every field.

TEST(VersionTests, ToString_ZeroComponentsAreDefined) {
    EXPECT_EQ(Version(0, 0, 0, 0).ToString(3), "0.0.0");
    EXPECT_EQ(Version(0, 0, 0, 0).ToString(4), "0.0.0.0");
}

// Build and Revision are public mutable fields, so this port can reach a state
// no .NET constructor produces: an undefined Build alongside a defined
// Revision.  .NET tests _Build first, so both 3 and 4 are rejected on the Build
// guard and report its bound.

TEST(VersionTests, ToString_UndefinedBuildWithDefinedRevision_RejectsOnBuildGuard) {
    Version v(1, 2);
    v.Revision = 5;
    for (System::intcs fieldCount : {3, 4}) {
        try {
            (void)v.ToString(fieldCount);
            FAIL() << "expected System::ArgumentException for fieldCount " << fieldCount;
        } catch (const System::ArgumentException& e) {
            EXPECT_STREQ(e.what(), "Argument must be between 0 and 2. (Parameter 'fieldCount')");
        }
    }
}

// "Specified" means non-negative, matching the predicate the no-argument
// ToString() already used, rather than exactly the -1 sentinel -- otherwise the
// two overloads would disagree about which components exist for any other
// negative value a caller writes into these public fields.

TEST(VersionTests, ToString_AnyNegativeComponentIsUnspecified) {
    Version v(1, 2, 3, 4);
    v.Build = -5;
    EXPECT_THROW(v.ToString(3), System::ArgumentException);
    EXPECT_THROW(v.ToString(4), System::ArgumentException);
    EXPECT_EQ(v.ToString(2), "1.2");

    Version w(1, 2, 3, 4);
    w.Revision = -5;
    EXPECT_THROW(w.ToString(4), System::ArgumentException);
    EXPECT_EQ(w.ToString(3), "1.2.3");
    EXPECT_EQ(w.ToString(), "1.2.3");
}

// The no-argument ToString() is the control for the whole repair: it already
// omitted unspecified components and must stay byte-identical.

TEST(VersionTests, ToString_NoArgument_ByteIdenticalAcrossTheMatrix) {
    EXPECT_EQ(Version().ToString(),            "0.0");
    EXPECT_EQ(Version(1, 2).ToString(),        "1.2");
    EXPECT_EQ(Version(1, 2, 3).ToString(),     "1.2.3");
    EXPECT_EQ(Version(1, 2, 3, 4).ToString(),  "1.2.3.4");
    EXPECT_EQ(Version("1.2").ToString(),       "1.2");
    EXPECT_EQ(Version("1.2.3").ToString(),     "1.2.3");
    EXPECT_EQ(Version("1.2.3.4").ToString(),   "1.2.3.4");
    EXPECT_EQ(Version(0, 0, 0, 0).ToString(),  "0.0.0.0");
}

// ---------------------------------------------------------------------------
// ToString() truncates at the first unspecified component (ticket #2259, a
// post-audit defect with no SR-AUD identifier -- it is NOT SR-AUD-011, which is
// about the fieldCount overload emitting a sentinel it was asked for).
//
// Before the repair ToString() tested Build >= 0 and Revision >= 0 in two
// INDEPENDENT ifs, so it omitted an undefined leading component while still
// emitting a defined trailing one: Build = -5 on 1.2.3.4 rendered "1.2.4",
// printing Revision in Build's position.  .NET's ToString() delegates to
// ToString(n) with a short-circuiting field count and truncates at two fields.
//
// The state is reachable only because Build and Revision are public mutable
// fields; no constructor and no parse() produces it, which is why
// ToString_NoArgument_ByteIdenticalAcrossTheMatrix above is unaffected and
// stands as this ticket's compatibility control.
// ---------------------------------------------------------------------------

TEST(VersionTests, ToString_UndefinedBuildTruncatesBeforeDefinedRevision) {
    Version v(1, 2, 3, 4);
    v.Build = -5;
    EXPECT_EQ(v.ToString(), "1.2");

    Version w(1, 2);
    w.Revision = 5;
    EXPECT_EQ(w.ToString(), "1.2");
}

// ToString() never rejects its own derived field count, for any component
// state -- the derived count can only name components that are defined.
TEST(VersionTests, ToString_NoArgument_NeverRejectsItsOwnFieldCount) {
    for (System::intcs build : {-5, -1, 0, 3}) {
        for (System::intcs revision : {-5, -1, 0, 4}) {
            Version v(1, 2);
            v.Build    = build;
            v.Revision = revision;
            EXPECT_NO_THROW((void)v.ToString())
                << "Build=" << build << " Revision=" << revision;
        }
    }
}

// The no-argument overload and the fieldCount overload agree: ToString() is
// exactly ToString(n) for the largest n this instance can serve.
TEST(VersionTests, ToString_NoArgument_AgreesWithFieldCountOverload) {
    EXPECT_EQ(Version(1, 2).ToString(),       Version(1, 2).ToString(2));
    EXPECT_EQ(Version(1, 2, 3).ToString(),    Version(1, 2, 3).ToString(3));
    EXPECT_EQ(Version(1, 2, 3, 4).ToString(), Version(1, 2, 3, 4).ToString(4));

    Version v(1, 2, 3, 4);
    v.Build = -5;
    EXPECT_EQ(v.ToString(), v.ToString(2));
}
