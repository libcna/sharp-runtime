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

#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Buffer.hpp"
#include "System/Exception.hpp"
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

// ===========================================================================
// SR-AUD-067 (CMS-A) -- raw Buffer::BlockCopy validates its signed metadata
// ===========================================================================

TEST(CoreMemorySafetyRawBufferTests, RawBlockCopy_NegativeCountIsRejected) {
    SharpRuntime::bytecs src[4] = {1, 2, 3, 4};
    SharpRuntime::bytecs dst[4] = {0, 0, 0, 0};
    EXPECT_THROW(System::Buffer::BlockCopy(src, 0, dst, 0, -1),
                 System::ArgumentOutOfRangeException);
    // No write happened: rejection precedes every byte of the memmove.
    EXPECT_EQ(dst[0], 0);
    EXPECT_EQ(dst[3], 0);
}

TEST(CoreMemorySafetyRawBufferTests, RawBlockCopy_NegativeSrcOffsetIsRejected) {
    SharpRuntime::bytecs src[4] = {1, 2, 3, 4};
    SharpRuntime::bytecs dst[4] = {0, 0, 0, 0};
    EXPECT_THROW(System::Buffer::BlockCopy(src, -4, dst, 0, 4),
                 System::ArgumentOutOfRangeException);
    EXPECT_EQ(dst[0], 0);
}

TEST(CoreMemorySafetyRawBufferTests, RawBlockCopy_NegativeDstOffsetIsRejected) {
    SharpRuntime::bytecs src[4] = {1, 2, 3, 4};
    SharpRuntime::bytecs dst[4] = {0, 0, 0, 0};
    EXPECT_THROW(System::Buffer::BlockCopy(src, 0, dst, -4, 4),
                 System::ArgumentOutOfRangeException);
    EXPECT_EQ(dst[0], 0);
}

TEST(CoreMemorySafetyRawBufferTests, RawBlockCopy_ParamNamesAndOrderMatchTheVectorOverload) {
    SharpRuntime::bytecs src[4] = {1, 2, 3, 4};
    SharpRuntime::bytecs dst[4] = {0, 0, 0, 0};
    // srcOffset is reported before dstOffset, which is reported before count --
    // the same order requireValidBlockCopyRange uses for the checked vector overload.
    auto paramOf = [&](intcs so, intcs dof, intcs n) {
        try { System::Buffer::BlockCopy(src, so, dst, dof, n); }
        catch (const System::ArgumentOutOfRangeException& e) { return std::string(e.getParamNameProperty()); }
        return std::string("<no throw>");
    };
    EXPECT_EQ(paramOf(-1, -1, -1), "srcOffset");
    EXPECT_EQ(paramOf(0, -1, -1), "dstOffset");
    EXPECT_EQ(paramOf(0, 0, -1), "count");
}

TEST(CoreMemorySafetyRawBufferTests, RawBlockCopy_ValidCallsAreUnchanged) {
    SharpRuntime::bytecs src[4] = {1, 2, 3, 4};
    SharpRuntime::bytecs dst[4] = {0, 0, 0, 0};
    System::Buffer::BlockCopy(src, 0, dst, 0, 4);
    EXPECT_EQ(dst[0], 1);
    EXPECT_EQ(dst[3], 4);

    SharpRuntime::bytecs dst2[4] = {9, 9, 9, 9};
    System::Buffer::BlockCopy(src, 1, dst2, 2, 2);
    EXPECT_EQ(dst2[0], 9);
    EXPECT_EQ(dst2[2], 2);
    EXPECT_EQ(dst2[3], 3);

    // count == 0 is legal and must stay legal.
    SharpRuntime::bytecs dst3[4] = {5, 5, 5, 5};
    System::Buffer::BlockCopy(src, 0, dst3, 0, 0);
    EXPECT_EQ(dst3[0], 5);
}

TEST(CoreMemorySafetyRawBufferTests, RawBlockCopy_OverlappingRegionsStillMove) {
    // memmove semantics were already correct here and must remain so.
    SharpRuntime::bytecs buf[5] = {1, 2, 3, 4, 5};
    System::Buffer::BlockCopy(buf, 0, buf, 1, 4);
    EXPECT_EQ(buf[0], 1);
    EXPECT_EQ(buf[1], 1);
    EXPECT_EQ(buf[2], 2);
    EXPECT_EQ(buf[3], 3);
    EXPECT_EQ(buf[4], 4);
}

TEST(CoreMemorySafetyRawBufferTests, RawBlockCopy_RejectionIsCatchableAsSystemException) {
    SharpRuntime::bytecs src[1] = {1};
    SharpRuntime::bytecs dst[1] = {0};
    EXPECT_THROW(System::Buffer::BlockCopy(src, 0, dst, 0, -1), System::Exception);
}

TEST(CoreMemorySafetyRawBufferTests, RawBlockCopy_NegativeOffsetIsRejectedEvenWithZeroCount) {
    // The case the old code "got away with": a zero count means memmove copies nothing,
    // so a negative offset produced no observable failure while still forming a pointer
    // before its own buffer. It is invalid metadata either way and is rejected either way.
    SharpRuntime::bytecs src[4] = {1, 2, 3, 4};
    SharpRuntime::bytecs dst[4] = {0, 0, 0, 0};
    EXPECT_THROW(System::Buffer::BlockCopy(src, -4, dst, 0, 0),
                 System::ArgumentOutOfRangeException);
    EXPECT_THROW(System::Buffer::BlockCopy(src, 0, dst, -4, 0),
                 System::ArgumentOutOfRangeException);
    EXPECT_THROW(System::Buffer::BlockCopy(src, SharpRuntime::INTCS_MIN, dst, 0, 0),
                 System::ArgumentOutOfRangeException);
}
