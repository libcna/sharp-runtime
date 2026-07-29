// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/Range.hpp"

using System::Range;
using System::Index;

TEST(RangeTest, DefaultCtorIsAll) {
    Range r;
    auto ol = r.GetOffsetAndLength(10);
    EXPECT_EQ(ol.Offset, 0);
    EXPECT_EQ(ol.Length, 10);
}

TEST(RangeTest, ExplicitCtorStartEnd) {
    Range r(Index::FromStart(2), Index::FromStart(5));
    auto ol = r.GetOffsetAndLength(10);
    EXPECT_EQ(ol.Offset, 2);
    EXPECT_EQ(ol.Length, 3);
}

TEST(RangeTest, GetAllProperty) {
    Range r = Range::getAllProperty();
    auto ol = r.GetOffsetAndLength(8);
    EXPECT_EQ(ol.Offset, 0);
    EXPECT_EQ(ol.Length, 8);
}

TEST(RangeTest, StartAt) {
    Range r = Range::StartAt(Index::FromStart(3));
    auto ol = r.GetOffsetAndLength(10);
    EXPECT_EQ(ol.Offset, 3);
    EXPECT_EQ(ol.Length, 7);
}

TEST(RangeTest, EndAt) {
    Range r = Range::EndAt(Index::FromStart(4));
    auto ol = r.GetOffsetAndLength(10);
    EXPECT_EQ(ol.Offset, 0);
    EXPECT_EQ(ol.Length, 4);
}

TEST(RangeTest, FromEnd) {
    Range r(Index::FromStart(0), Index::FromEnd(2));
    auto ol = r.GetOffsetAndLength(10);
    EXPECT_EQ(ol.Offset, 0);
    EXPECT_EQ(ol.Length, 8);
}

TEST(RangeTest, GetStartProperty) {
    Range r(Index::FromStart(2), Index::FromStart(5));
    EXPECT_EQ(r.getStartProperty().getValueProperty(), 2);
}

TEST(RangeTest, GetEndProperty) {
    Range r(Index::FromStart(2), Index::FromStart(5));
    EXPECT_EQ(r.getEndProperty().getValueProperty(), 5);
}

TEST(RangeTest, EqualsTrue) {
    Range a(Index::FromStart(1), Index::FromStart(4));
    Range b(Index::FromStart(1), Index::FromStart(4));
    EXPECT_TRUE(a.Equals(b));
    EXPECT_TRUE(a == b);
}

TEST(RangeTest, EqualsFalse) {
    Range a(Index::FromStart(1), Index::FromStart(4));
    Range b(Index::FromStart(2), Index::FromStart(4));
    EXPECT_FALSE(a.Equals(b));
    EXPECT_TRUE(a != b);
}

TEST(RangeTest, GetHashCode_SameRangesSameHash) {
    Range a(Index::FromStart(1), Index::FromStart(3));
    Range b(Index::FromStart(1), Index::FromStart(3));
    EXPECT_EQ(a.GetHashCode(), b.GetHashCode());
}

TEST(RangeTest, ToString_FromStart) {
    Range r(Index::FromStart(1), Index::FromStart(5));
    EXPECT_EQ(r.ToString(), "1..5");
}

TEST(RangeTest, ToString_FromEnd) {
    Range r(Index::FromStart(0), Index::FromEnd(1));
    EXPECT_EQ(r.ToString(), "0..^1");
}

TEST(RangeTest, GetOffsetAndLength_OutOfOrder_Throws) {
    Range r(Index::FromStart(5), Index::FromStart(2));
    EXPECT_THROW(r.GetOffsetAndLength(10), System::ArgumentOutOfRangeException);
}

TEST(RangeTest, GetOffsetAndLength_EndBeyondLength_Throws) {
    // Index.GetOffset does not itself validate against length (matches .NET), so this
    // must be caught by Range.GetOffsetAndLength's own bounds check.
    Range r(Index::FromStart(2), Index::FromStart(20));
    EXPECT_THROW(r.GetOffsetAndLength(10), System::ArgumentOutOfRangeException);
}

TEST(RangeTest, GetOffsetAndLength_NegativeStart_Throws) {
    Range r(Index::FromEnd(20), Index::FromStart(5));
    EXPECT_THROW(r.GetOffsetAndLength(10), System::ArgumentOutOfRangeException);
}

// ===========================================================================
// Ticket #1830 -- CCF-004 / SR-AUD-057: Range's OWN defined-arithmetic site
// ===========================================================================
//
// This is not the same defect as Index::GetOffset's, and that distinction is the
// point of these tests. SR-AUD-057 and docs/DefinedArithmeticBoundaryPlan.md §2
// both described Range as merely CONSUMING Index's operation. It has a second,
// independent overflow of its own: for a maximal from-end range over an
// INTCS_MIN length the unsigned bounds checks PASS, and the `end - start` that
// follows was UBSan-confirmed undefined behaviour --
//
//   Range.hpp:99: runtime error: signed integer overflow:
//                 -2147483648 - 1 cannot be represented in type 'int'
//
// -- reported in the same process as Index.hpp:61's, which is why the survey's
// first pass, aborting at the first diagnostic, did not show it. Both subtractions
// are now performed in SharpRuntime::uintcs, and the resolved values are unchanged.

TEST(RangeTest, GetOffsetAndLength_ExtremeFromEndOverNegativeLength_DefinedWrap) {
    // The audited input. Both values are the ones measured before the repair.
    Range r(Index::FromEnd(2147483647), Index::FromEnd(0));
    const auto ol = r.GetOffsetAndLength(-2147483648);
    EXPECT_EQ(ol.Offset, 1);
    EXPECT_EQ(ol.Length, 2147483647);
}

// Every ordinary resolution must be untouched.
TEST(RangeTest, GetOffsetAndLength_OrdinaryRangesUnchangedByDefinedArithmetic) {
    EXPECT_EQ(Range(Index::FromStart(2), Index::FromStart(7)).GetOffsetAndLength(10).Offset, 2);
    EXPECT_EQ(Range(Index::FromStart(2), Index::FromStart(7)).GetOffsetAndLength(10).Length, 5);
    EXPECT_EQ(Range(Index::FromStart(0), Index::FromEnd(0)).GetOffsetAndLength(10).Offset, 0);
    EXPECT_EQ(Range(Index::FromStart(0), Index::FromEnd(0)).GetOffsetAndLength(10).Length, 10);
    // An empty range at each end stays empty.
    EXPECT_EQ(Range(Index::FromStart(10), Index::FromStart(10)).GetOffsetAndLength(10).Length, 0);
    EXPECT_EQ(Range(Index::FromStart(0), Index::FromStart(0)).GetOffsetAndLength(10).Length, 0);
}

// The existing rejections must still reject: the repair is arithmetic-only and
// must not have widened what GetOffsetAndLength accepts.
TEST(RangeTest, GetOffsetAndLength_StillRejectsWhatItRejectedBefore) {
    EXPECT_THROW(Range(Index::FromStart(2), Index::FromStart(20)).GetOffsetAndLength(10),
                 System::ArgumentOutOfRangeException);
    EXPECT_THROW(Range(Index::FromEnd(20), Index::FromStart(5)).GetOffsetAndLength(10),
                 System::ArgumentOutOfRangeException);
}
