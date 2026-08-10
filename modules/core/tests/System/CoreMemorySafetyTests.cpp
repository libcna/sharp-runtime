// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Permanent regressions for the bounded modules/core memory-safety family:
// SR-AUD-044, SR-AUD-045, SR-AUD-051, SR-AUD-054 and SR-AUD-067.
// The family's whole record -- door inventory, before/after sanitizer evidence,
// premise corrections and exclusions -- is docs/CoreMemorySafetyFamilyPlan.md.
#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "System/SpanSplitEnumerator.hpp"

using SharpRuntime::intcs;
using System::ReadOnlySpan;
using System::SpanSplitEnumerator;

// ===========================================================================
// SR-AUD-045 (CMS-D) -- an empty exact sequence is not a separator
// ===========================================================================

TEST(CoreMemorySafetySplitTests, EmptySequence_YieldsWholeSourceOnceThenFinishes) {
    std::vector<int> src{1, 2, 3};
    ReadOnlySpan<int> span(src);
    SpanSplitEnumerator<int> e(span, std::vector<int>{}, /*treatAsAny=*/false);

    ASSERT_TRUE(e.MoveNext());
    auto seg = e.getCurrentSpan();
    EXPECT_EQ(seg.getLengthProperty(), 3);
    EXPECT_EQ(seg[0], 1);
    EXPECT_EQ(seg[2], 3);
    EXPECT_FALSE(e.MoveNext());
    // Still false once finished -- the done_ latch must not be re-armed.
    EXPECT_FALSE(e.MoveNext());
}

TEST(CoreMemorySafetySplitTests, EmptySequence_CurrentRangeCoversTheWholeSource) {
    std::vector<int> src{1, 2, 3};
    ReadOnlySpan<int> span(src);
    SpanSplitEnumerator<int> e(span, std::vector<int>{}, false);
    ASSERT_TRUE(e.MoveNext());
    const System::Range r = e.getCurrentProperty();
    EXPECT_EQ(r.getStartProperty().getValueProperty(), 0);
    EXPECT_EQ(r.getEndProperty().getValueProperty(), 3);
}

TEST(CoreMemorySafetySplitTests, EmptySequence_RangeForTerminatesWithExactlyOneSegment) {
    std::vector<int> src{1, 2, 3};
    ReadOnlySpan<int> span(src);
    SpanSplitEnumerator<int> e(span, std::vector<int>{}, false);
    int segments = 0;
    int lastLength = -1;
    for (auto seg : e) {
        lastLength = static_cast<int>(seg.getLengthProperty());
        // Bounded so a regression fails the assertion instead of hanging the suite.
        if (++segments > 8) break;
    }
    EXPECT_EQ(segments, 1);
    EXPECT_EQ(lastLength, 3);
}

TEST(CoreMemorySafetySplitTests, EmptySequence_EmptySourceYieldsOneEmptySegment) {
    std::vector<int> src{};
    ReadOnlySpan<int> span(src);
    SpanSplitEnumerator<int> e(span, std::vector<int>{}, false);
    ASSERT_TRUE(e.MoveNext());
    EXPECT_EQ(e.getCurrentSpan().getLengthProperty(), 0);
    EXPECT_FALSE(e.MoveNext());
}

TEST(CoreMemorySafetySplitTests, EmptyAnyOfListKeepsItsOwnSemantics) {
    // Deliberately NOT changed by SR-AUD-045's repair: the empty any-of list already
    // terminated, yielding the whole source once, and that is a separate decision.
    std::vector<int> src{1, 2, 3};
    ReadOnlySpan<int> span(src);
    SpanSplitEnumerator<int> e(span, std::vector<int>{}, /*treatAsAny=*/true);
    ASSERT_TRUE(e.MoveNext());
    EXPECT_EQ(e.getCurrentSpan().getLengthProperty(), 3);
    EXPECT_FALSE(e.MoveNext());
}

TEST(CoreMemorySafetySplitTests, NonEmptySequenceIsUnchanged) {
    // Regression guard: the repair must not perturb the ordinary sequence mode.
    std::vector<int> src{1, 2, 3, 2, 4};
    ReadOnlySpan<int> span(src);
    {   // separator in the middle
        SpanSplitEnumerator<int> e(span, std::vector<int>{2}, false);
        std::vector<intcs> lengths;
        for (auto seg : e) {
            lengths.push_back(seg.getLengthProperty());
            if (lengths.size() > 8) break;
        }
        ASSERT_EQ(lengths.size(), 3u);
        EXPECT_EQ(lengths[0], 1);
        EXPECT_EQ(lengths[1], 1);
        EXPECT_EQ(lengths[2], 1);
    }
    {   // separator at the start
        std::vector<int> s2{2, 1, 3};
        ReadOnlySpan<int> sp2(s2);
        SpanSplitEnumerator<int> e(sp2, std::vector<int>{2}, false);
        ASSERT_TRUE(e.MoveNext());
        EXPECT_EQ(e.getCurrentSpan().getLengthProperty(), 0);
        ASSERT_TRUE(e.MoveNext());
        EXPECT_EQ(e.getCurrentSpan().getLengthProperty(), 2);
        EXPECT_FALSE(e.MoveNext());
    }
    {   // separator at the end
        std::vector<int> s3{1, 3, 2};
        ReadOnlySpan<int> sp3(s3);
        SpanSplitEnumerator<int> e(sp3, std::vector<int>{2}, false);
        ASSERT_TRUE(e.MoveNext());
        EXPECT_EQ(e.getCurrentSpan().getLengthProperty(), 2);
        ASSERT_TRUE(e.MoveNext());
        EXPECT_EQ(e.getCurrentSpan().getLengthProperty(), 0);
        EXPECT_FALSE(e.MoveNext());
    }
    {   // adjacent separators
        std::vector<int> s4{1, 2, 2, 3};
        ReadOnlySpan<int> sp4(s4);
        SpanSplitEnumerator<int> e(sp4, std::vector<int>{2}, false);
        std::vector<intcs> lengths;
        for (auto seg : e) {
            lengths.push_back(seg.getLengthProperty());
            if (lengths.size() > 8) break;
        }
        ASSERT_EQ(lengths.size(), 3u);
        EXPECT_EQ(lengths[0], 1);
        EXPECT_EQ(lengths[1], 0);
        EXPECT_EQ(lengths[2], 1);
    }
    {   // a sequence longer than the source still finds nothing and yields the source
        SpanSplitEnumerator<int> e(span, std::vector<int>{9, 9, 9, 9, 9, 9}, false);
        ASSERT_TRUE(e.MoveNext());
        EXPECT_EQ(e.getCurrentSpan().getLengthProperty(), 5);
        EXPECT_FALSE(e.MoveNext());
    }
}

TEST(CoreMemorySafetySplitTests, EmptySequenceOverNonTrivialElements) {
    std::vector<std::string> src{"x", "y"};
    ReadOnlySpan<std::string> span(src);
    SpanSplitEnumerator<std::string> e(span, std::vector<std::string>{}, false);
    ASSERT_TRUE(e.MoveNext());
    EXPECT_EQ(e.getCurrentSpan().getLengthProperty(), 2);
    EXPECT_FALSE(e.MoveNext());
}
