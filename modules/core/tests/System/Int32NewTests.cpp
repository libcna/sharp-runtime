// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/FormatException.hpp"
#include "System/Int32.hpp"

using System::Int32;

// Additional Int32 tests (Parse/TryParse/ToString/most math already in PrimitiveTypeTests.cpp)

TEST(Int32NewTests, CompareTo_Equal_Zero)      { EXPECT_EQ(Int32::CompareTo(5, 5), 0); }
TEST(Int32NewTests, CompareTo_Less_Negative)   { EXPECT_LT(Int32::CompareTo(3, 5), 0); }
TEST(Int32NewTests, CompareTo_Greater_Positive){ EXPECT_GT(Int32::CompareTo(7, 5), 0); }

TEST(Int32NewTests, Equals_Same_True)  { EXPECT_TRUE(Int32::Equals(42, 42)); }
TEST(Int32NewTests, Equals_Diff_False) { EXPECT_FALSE(Int32::Equals(1, 2)); }

TEST(Int32NewTests, GetHashCode_SameValue_SameHash) {
    EXPECT_EQ(Int32::GetHashCode(100), Int32::GetHashCode(100));
}
TEST(Int32NewTests, GetHashCode_IsValueItself) {
    // .NET Int32.GetHashCode() literally returns m_value.
    EXPECT_EQ(Int32::GetHashCode(-7), -7);
}

// MaxMagnitude/MinMagnitude with MinValue: MinValue has no representable positive
// magnitude, so .NET special-cases it to always win MaxMagnitude and always lose
// MinMagnitude, regardless of the other operand.
TEST(Int32NewTests, MaxMagnitude_MinValueAlwaysWins) {
    EXPECT_EQ(Int32::MaxMagnitude(Int32::MinValue, 5), Int32::MinValue);
    EXPECT_EQ(Int32::MaxMagnitude(5, Int32::MinValue), Int32::MinValue);
    EXPECT_EQ(Int32::MaxMagnitude(Int32::MinValue, Int32::MinValue), Int32::MinValue);
    EXPECT_EQ(Int32::MaxMagnitude(Int32::MinValue, Int32::MaxValue), Int32::MinValue);
}

TEST(Int32NewTests, MinMagnitude_MinValueAlwaysLoses) {
    EXPECT_EQ(Int32::MinMagnitude(Int32::MinValue, 5), 5);
    EXPECT_EQ(Int32::MinMagnitude(5, Int32::MinValue), 5);
    EXPECT_EQ(Int32::MinMagnitude(Int32::MinValue, Int32::MinValue), Int32::MinValue);
    EXPECT_EQ(Int32::MinMagnitude(Int32::MinValue, Int32::MaxValue), Int32::MaxValue);
}

TEST(Int32NewTests, ToString_CustomNumericFormat) {
    // Int32 lacked the custom-numeric path Single and Double already had, so int.ToString("00")
    // -- and therefore String.Format("{0:00}", n) -- raised FormatException on a format .NET
    // formats fine. Found by cna-samples SAMPLE-046, where two ported XNA samples print a clock
    // with String.Format("{0:00}:{1:00}", minutes, seconds).
    EXPECT_EQ(Int32::ToString(3, "00"), "03");
    EXPECT_EQ(Int32::ToString(3, "000"), "003");
    EXPECT_EQ(Int32::ToString(-3, "00"), "-03");
    EXPECT_EQ(Int32::ToString(1234, "00"), "1234");
    EXPECT_EQ(Int32::ToString(3, "0.0"), "3.0");
}

TEST(Int32NewTests, ToString_MalformedStandardSpecifierIsUnaffectedByTheCustomPath) {
    // The custom path must not swallow a malformed STANDARD specifier: those still throw, which
    // is what ticket #1847 pinned. The gate asks whether the format begins with something other
    // than a specifier letter, not merely whether it fails to be a standard format.
    EXPECT_THROW(Int32::ToString(5, "Xz"), System::FormatException);
    EXPECT_THROW(Int32::ToString(5, "Q"), System::FormatException);
}
