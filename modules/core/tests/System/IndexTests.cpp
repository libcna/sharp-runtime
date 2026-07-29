// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Index.hpp"

using System::Index;

TEST(IndexTests2, DefaultCtor_IsFromStart_ValueZero) {
    Index idx;
    EXPECT_FALSE(idx.getIsFromEndProperty());
    EXPECT_EQ(idx.getValueProperty(), 0);
}

TEST(IndexTests2, FromStart_CorrectOffset) {
    Index idx = Index::FromStart(2);
    EXPECT_EQ(idx.GetOffset(5), 2);
    EXPECT_FALSE(idx.getIsFromEndProperty());
}

TEST(IndexTests2, FromEnd_CorrectOffset) {
    Index idx = Index::FromEnd(1);
    EXPECT_TRUE(idx.getIsFromEndProperty());
    EXPECT_EQ(idx.GetOffset(5), 4);
}

TEST(IndexTests2, Start_IsZeroFromStart) {
    Index idx = Index::Start();
    EXPECT_EQ(idx.GetOffset(10), 0);
}

TEST(IndexTests2, End_IsZeroFromEnd) {
    Index idx = Index::End();
    EXPECT_EQ(idx.GetOffset(5), 5);
}

TEST(IndexTests2, NegativeValue_Throws) {
    EXPECT_THROW(Index(-1), System::ArgumentOutOfRangeException);
}

TEST(IndexTests2, GetOffset_OutOfRange_DoesNotThrow_MatchesDotNet) {
    // .NET's Index.GetOffset does not validate the result against the collection
    // length, by design (see Index.cs remarks) - validation is the caller's job
    // (e.g. Range.GetOffsetAndLength).
    Index idx = Index::FromStart(10);
    EXPECT_EQ(idx.GetOffset(5), 10);
}

TEST(IndexTests2, ImplicitConversion_FromInt_IsFromStart) {
    Index idx = 3;
    EXPECT_FALSE(idx.getIsFromEndProperty());
    EXPECT_EQ(idx.getValueProperty(), 3);
}

TEST(IndexTests2, Equals_SameValueAndDirection_True) {
    EXPECT_TRUE(Index::FromStart(2).Equals(Index::FromStart(2)));
    EXPECT_TRUE(Index::FromStart(2) == Index::FromStart(2));
}

TEST(IndexTests2, Equals_DifferentDirection_False) {
    EXPECT_FALSE(Index::FromStart(2).Equals(Index::FromEnd(2)));
    EXPECT_TRUE(Index::FromStart(2) != Index::FromEnd(2));
}

TEST(IndexTests2, GetHashCode_MatchesForEqualIndexes) {
    EXPECT_EQ(Index::FromStart(4).GetHashCode(), Index::FromStart(4).GetHashCode());
    EXPECT_EQ(Index::FromEnd(1).GetHashCode(), Index::FromEnd(1).GetHashCode());
}

TEST(IndexTests2, ToString_FromStart) {
    EXPECT_EQ(Index::FromStart(3).ToString(), "3");
}

TEST(IndexTests2, ToString_FromEnd) {
    EXPECT_EQ(Index::FromEnd(1).ToString(), "^1");
}

// ===========================================================================
// Ticket #1830 -- CCF-004 / SR-AUD-057: defined arithmetic in GetOffset
// ===========================================================================
//
// GetOffset evaluated `length - value_` in signed intcs. .NET's Index.cs
// deliberately skips validation here for performance, and C# gives that decision
// meaning because its default integer arithmetic has DEFINED two's-complement
// wrap. This port cannot inherit the decision by executing signed overflow:
// build-probe/1829_ccf004_survey.log case 6 reported
//
//   Index.hpp:61: runtime error: signed integer overflow:
//                 -2147483648 - 2147483647 cannot be represented in type 'int'
//
// The subtraction now happens in SharpRuntime::uintcs. This is CCF-004 class A --
// a defined-wrap repair with NO observable change -- so the value measured before
// the fix is asserted below, which is what makes that claim provable rather than
// merely stated.

TEST(IndexTests2, GetOffset_FromEndExtremes_ProducesTheDefinedWrap) {
    // The exact audited input. 1 is the value the platform produced before the
    // repair and the value two's-complement arithmetic defines:
    // INTCS_MIN - INTCS_MAX == -4294967295, which is 1 modulo 2^32.
    EXPECT_EQ(Index::FromEnd(2147483647).GetOffset(-2147483648), 1);
}

TEST(IndexTests2, GetOffset_FromEndMaxOverZeroLength_ProducesTheDefinedWrap) {
    EXPECT_EQ(Index::FromEnd(2147483647).GetOffset(0), -2147483647);
}

TEST(IndexTests2, GetOffset_FromEndZero_IsTheLengthItself) {
    EXPECT_EQ(Index::FromEnd(0).GetOffset(10), 10);
    EXPECT_EQ(Index::FromEnd(0).GetOffset(0), 0);
}

// The ordinary paths must be untouched -- the cast must not have inverted anything.
TEST(IndexTests2, GetOffset_OrdinaryValuesUnchanged) {
    EXPECT_EQ(Index::FromEnd(1).GetOffset(10), 9);
    EXPECT_EQ(Index::FromEnd(10).GetOffset(10), 0);
    EXPECT_EQ(Index::FromStart(3).GetOffset(10), 3);
    EXPECT_EQ(Index::FromStart(0).GetOffset(10), 0);
    // A from-start index ignores length entirely, including an extreme one.
    EXPECT_EQ(Index::FromStart(7).GetOffset(-2147483648), 7);
}

// GetOffset is noexcept and must stay noexcept: the class A repair introduces no
// throw, and adding one would be a source-compatibility change #1830 does not
// authorise. Pinned at compile time.
TEST(IndexTests2, GetOffset_IsStillNoexcept) {
    const Index idx = Index::FromEnd(1);
    static_assert(noexcept(idx.GetOffset(10)), "Index::GetOffset must remain noexcept");
    SUCCEED();
}
