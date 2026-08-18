// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Permanent regressions for the bounded modules/core memory-safety family:
// SR-AUD-044, SR-AUD-045, SR-AUD-051, SR-AUD-054 and SR-AUD-067.
// The family's whole record -- door inventory, before/after sanitizer evidence,
// premise corrections and exclusions -- is docs/CoreMemorySafetyFamilyPlan.md.
#include <gtest/gtest.h>

#include <functional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "System/ArgumentException.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Array.hpp"
#include "System/ArraySegment.hpp"
#include "System/Buffer.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/Memory.hpp"
#include "System/MemoryExtensions.hpp"
#include "System/ReadOnlyMemory.hpp"
#include "System/Span.hpp"
#include "System/Exception.hpp"
#include "System/SpanSplitEnumerator.hpp"

using SharpRuntime::intcs;
using System::ArraySegment;
using System::Memory;
using System::ReadOnlyMemory;
using System::ReadOnlySpan;
using System::Span;
using System::SpanSplitEnumerator;

namespace {

    // Four distinguishable heap-allocating values, long enough that libstdc++ cannot apply
    // the short-string optimisation -- so a byte copy really does duplicate an owning
    // pointer, and a lost element really is lost.
    std::vector<std::string> abcd() {
        return {"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
                "cccccccccccccccccccccccccccccccccccc",
                "dddddddddddddddddddddddddddddddddddd"};
    }

    // Compresses those four values to their initial letters, so an expectation reads as the
    // .NET answer it is checking ("aabc", "bcdd", ...).
    std::string shape(const std::vector<std::string>& v) {
        std::string s;
        for (const auto& e : v) s.push_back(e.empty() ? '_' : e[0]);
        return s;
    }

} // namespace

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

// ===========================================================================
// SR-AUD-051 (CMS-A) -- raw Array::Copy validates, copies elements, and moves
// ===========================================================================

TEST(CoreMemorySafetyRawArrayTests, RawCopy_NegativeArgumentsAreRejected) {
    int src[4] = {1, 2, 3, 4};
    int dst[4] = {0, 0, 0, 0};
    EXPECT_THROW(System::Array::Copy<int>(src, 0, dst, 0, -1),
                 System::ArgumentOutOfRangeException);
    EXPECT_THROW(System::Array::Copy<int>(src, -4, dst, 0, 4),
                 System::ArgumentOutOfRangeException);
    EXPECT_THROW(System::Array::Copy<int>(src, 0, dst, -4, 4),
                 System::ArgumentOutOfRangeException);
    EXPECT_THROW(System::Array::Copy<int>(src, SharpRuntime::INTCS_MIN, dst, 0, 0),
                 System::ArgumentOutOfRangeException);
    // Nothing was written by any rejected call.
    EXPECT_EQ(dst[0], 0);
    EXPECT_EQ(dst[3], 0);
}

TEST(CoreMemorySafetyRawArrayTests, RawCopy_ParamNamesAndValidationOrder) {
    int src[4] = {1, 2, 3, 4};
    int dst[4] = {0, 0, 0, 0};
    auto paramOf = [&](intcs si, intcs di, intcs n) {
        try { System::Array::Copy<int>(src, si, dst, di, n); }
        catch (const System::ArgumentOutOfRangeException& e) { return std::string(e.getParamNameProperty()); }
        return std::string("<no throw>");
    };
    EXPECT_EQ(paramOf(-1, -1, -1), "srcIndex");
    EXPECT_EQ(paramOf(0, -1, -1), "dstIndex");
    EXPECT_EQ(paramOf(0, 0, -1), "length");
}

TEST(CoreMemorySafetyRawArrayTests, RawCopy_NullIsRejectedOnlyWhenItWouldBeTouched) {
    int buf[2] = {1, 2};
    // A null buffer with a positive length is rejected before any dereference.
    EXPECT_THROW(System::Array::Copy<int>(nullptr, 0, buf, 0, 1), System::ArgumentNullException);
    EXPECT_THROW(System::Array::Copy<int>(buf, 0, static_cast<int*>(nullptr), 0, 1),
                 System::ArgumentNullException);
    // A null buffer with a zero length is the ordinary C++ empty-range idiom and is
    // accepted -- a documented, deliberate divergence from .NET's unconditional
    // ArgumentNullException for a null array.
    EXPECT_NO_THROW(System::Array::Copy<int>(static_cast<const int*>(nullptr), 0,
                                             static_cast<int*>(nullptr), 0, 0));
}

TEST(CoreMemorySafetyRawArrayTests, RawCopy_CopiesElementsNotBytesForOwningTypes) {
    // Before #2213 this memcpy'd sizeof(std::string) bytes, duplicating each owning
    // pointer and producing an ASan-confirmed double-free at destruction.
    std::string src[2] = {"a string long enough to force a heap allocation 1",
                          "a string long enough to force a heap allocation 2"};
    std::string dst[2] = {"another string long enough to force a heap alloc 3",
                          "another string long enough to force a heap alloc 4"};
    System::Array::Copy<std::string>(src, 0, dst, 0, 2);
    EXPECT_EQ(dst[0], src[0]);
    EXPECT_EQ(dst[1], src[1]);
    // Distinct storage: assigning through one must not be visible through the other.
    dst[0] = "changed";
    EXPECT_NE(dst[0], src[0]);
}

TEST(CoreMemorySafetyRawArrayTests, RawCopy_IsOverlapSafeInBothDirections) {
    {   // right overlap -- was ASan memcpy-param-overlap, and lost the tail
        std::vector<int> v{1, 2, 3, 4};
        System::Array::Copy<int>(v.data(), 0, v.data(), 1, 3);
        EXPECT_EQ(v, (std::vector<int>{1, 1, 2, 3}));
    }
    {   // left overlap -- already correct, must stay correct
        std::vector<int> v{1, 2, 3, 4};
        System::Array::Copy<int>(v.data(), 1, v.data(), 0, 3);
        EXPECT_EQ(v, (std::vector<int>{2, 3, 4, 4}));
    }
    {   // exact self-copy is a no-op
        std::vector<int> v{1, 2, 3, 4};
        System::Array::Copy<int>(v.data(), 0, v.data(), 0, 4);
        EXPECT_EQ(v, (std::vector<int>{1, 2, 3, 4}));
    }
    {   // adjacent, not overlapping
        std::vector<int> v{1, 2, 3, 4};
        System::Array::Copy<int>(v.data(), 0, v.data(), 2, 2);
        EXPECT_EQ(v, (std::vector<int>{1, 2, 1, 2}));
    }
    {   // right overlap with an owning element type
        std::vector<std::string> v = abcd();
        System::Array::Copy<std::string>(v.data(), 0, v.data(), 1, 3);
        EXPECT_EQ(shape(v), "aabc");
    }
    {   // LEFT overlap with an owning element type.
        //
        // This assertion is not redundant with the `int` left-overlap case above, and the
        // reason is worth stating: libstdc++ lowers BOTH std::copy and std::copy_backward
        // over a trivially copyable type to __builtin_memmove, which is correct in both
        // directions. So an `int` test cannot tell a direction-SELECTING copy apart from an
        // unconditionally backward one -- exactly the masking the audit noted for the
        // forward case. Only an owning element type, whose assignment really happens
        // element by element, discriminates. Measured: with an unconditional
        // std::copy_backward the `int` case still passes and this one yields "ddd d".
        std::vector<std::string> v = abcd();
        System::Array::Copy<std::string>(v.data(), 1, v.data(), 0, 3);
        EXPECT_EQ(shape(v), "bcdd");
    }
}

TEST(CoreMemorySafetyRawArrayTests, RawCopy_OrdinaryDisjointCopiesAreUnchanged) {
    int src[4] = {1, 2, 3, 4};
    int dst[4] = {9, 9, 9, 9};
    System::Array::Copy<int>(src, 1, dst, 2, 2);
    EXPECT_EQ(dst[0], 9);
    EXPECT_EQ(dst[1], 9);
    EXPECT_EQ(dst[2], 2);
    EXPECT_EQ(dst[3], 3);

    int dst2[4] = {7, 7, 7, 7};
    System::Array::Copy<int>(src, 0, dst2, 0, 0);   // zero length is legal
    EXPECT_EQ(dst2[0], 7);

    int dst3[1] = {0};
    System::Array::Copy<int>(src, 3, dst3, 0, 1);   // one element
    EXPECT_EQ(dst3[0], 4);
}

TEST(CoreMemorySafetyRawArrayTests, RawCopy_RejectionIsCatchableAsSystemException) {
    int src[1] = {1};
    int dst[1] = {0};
    EXPECT_THROW(System::Array::Copy<int>(src, 0, dst, 0, -1), System::Exception);
    EXPECT_THROW(System::Array::Copy<int>(nullptr, 0, dst, 0, 1), System::Exception);
}

// ---------------------------------------------------------------------------
// SR-AUD-051 (extended) -- the generic Buffer templates require a trivially
// copyable element type. The REJECTION of a non-trivially-copyable T is a
// compile-time claim and lives in
// test/consumer/core_buffer_trivially_copyable_negative.cpp; what a runtime test
// can pin is that the requirement is exactly is_trivially_copyable, and that
// every type which satisfies it still works.
// ---------------------------------------------------------------------------

namespace {
    struct TriviallyCopyablePoint { int x; int y; };
    enum class TriviallyCopyableEnum : SharpRuntime::bytecs { A = 1, B = 2 };
    struct OwningElement { std::string s; };
}

TEST(CoreMemorySafetyBufferGenericTests, RequirementIsExactlyTriviallyCopyable) {
    static_assert(std::is_trivially_copyable_v<TriviallyCopyablePoint>);
    static_assert(std::is_trivially_copyable_v<TriviallyCopyableEnum>);
    static_assert(std::is_trivially_copyable_v<bool>);
    static_assert(std::is_trivially_copyable_v<double>);
    // The types the constraint exists to reject:
    static_assert(!std::is_trivially_copyable_v<std::string>);
    static_assert(!std::is_trivially_copyable_v<OwningElement>);
    static_assert(!std::is_trivially_copyable_v<std::vector<int>>);
    SUCCEED();
}

TEST(CoreMemorySafetyBufferGenericTests, TriviallyCopyableElementTypesStillWork) {
    std::vector<TriviallyCopyablePoint> src{{1, 2}, {3, 4}};
    std::vector<TriviallyCopyablePoint> dst{{0, 0}, {0, 0}};
    EXPECT_EQ(System::Buffer::ByteLength(src),
              static_cast<intcs>(2 * sizeof(TriviallyCopyablePoint)));
    System::Buffer::BlockCopy(src, 0, dst, 0,
                              static_cast<intcs>(2 * sizeof(TriviallyCopyablePoint)));
    EXPECT_EQ(dst[1].y, 4);

    std::vector<TriviallyCopyableEnum> e{TriviallyCopyableEnum::A, TriviallyCopyableEnum::B};
    EXPECT_EQ(System::Buffer::ByteLength(e), 2);
    EXPECT_EQ(System::Buffer::GetByte(e, 1), 2);
    System::Buffer::SetByte(e, 0, 2);
    EXPECT_EQ(e[0], TriviallyCopyableEnum::B);

    std::vector<double> d{1.0, 2.0};
    EXPECT_EQ(System::Buffer::ByteLength(d), 16);
}

// ===========================================================================
// SR-AUD-054 (CMS-B) -- the default ArraySegment is a guarded invalid state
// ===========================================================================

namespace {
    const char* kNullArrayMessage = "The underlying array is null.";

    std::string messageOf(const std::function<void()>& call) {
        try { call(); }
        catch (const System::InvalidOperationException& e) { return std::string(e.what()); }
        catch (const System::Exception& e) { return std::string("<wrong type> ") + e.what(); }
        return std::string("<no throw>");
    }
}

TEST(CoreMemorySafetyDefaultSegmentTests, SliceOverloadsRejectTheDefaultSegment) {
    // Both used to bind a reference to the null array pointer (UBSan) and then read
    // through it (ASan SEGV). Slice(0, 0) was not named by the audit at all.
    ArraySegment<int> seg;
    EXPECT_THROW((void)seg.Slice(0), System::InvalidOperationException);
    EXPECT_THROW((void)seg.Slice(0, 0), System::InvalidOperationException);
    EXPECT_EQ(messageOf([&] { (void)seg.Slice(0); }), kNullArrayMessage);
    EXPECT_EQ(messageOf([&] { (void)seg.Slice(0, 0); }), kNullArrayMessage);
}

TEST(CoreMemorySafetyDefaultSegmentTests, IndexersRejectWithInvalidOperationNotOutOfRange) {
    // These already threw -- with the WRONG type, because count_ == 0 made the range
    // check fire before the null dereference could. .NET checks the default state first.
    ArraySegment<int> seg;
    const ArraySegment<int> constSeg;
    EXPECT_THROW((void)seg[0], System::InvalidOperationException);
    EXPECT_THROW((void)constSeg[0], System::InvalidOperationException);
    EXPECT_EQ(messageOf([&] { (void)seg[0]; }), kNullArrayMessage);
    EXPECT_EQ(messageOf([&] { (void)constSeg[5]; }), kNullArrayMessage);
    // Even an index that is out of range on its own terms reports the default state first.
    EXPECT_THROW((void)seg[-1], System::InvalidOperationException);
}

TEST(CoreMemorySafetyDefaultSegmentTests, CopyToFormsRejectTheDefaultSegment) {
    ArraySegment<int> seg;
    std::vector<int> dst{7, 7, 7};
    EXPECT_THROW(seg.CopyTo(dst), System::InvalidOperationException);
    EXPECT_THROW(seg.CopyTo(dst, 1), System::InvalidOperationException);
    ArraySegment<int> otherDefault;
    EXPECT_THROW(seg.CopyTo(otherDefault), System::InvalidOperationException);
    // The destination was neither resized nor written by any rejected call.
    EXPECT_EQ(dst.size(), 3u);
    EXPECT_EQ(dst[0], 7);
}

TEST(CoreMemorySafetyDefaultSegmentTests, CopyToSegmentChecksSourceBeforeDestination) {
    // .NET's order: this.ThrowInvalidOperationIfDefault(), then
    // destination.ThrowInvalidOperationIfDefault(), then the length check.
    std::vector<int> v{1, 2, 3};
    ArraySegment<int> valid(v);
    ArraySegment<int> defaulted;

    // A valid source with a default destination is still rejected.
    EXPECT_THROW(valid.CopyTo(defaulted), System::InvalidOperationException);
    // A default source with a valid destination is rejected too...
    ArraySegment<int> validDst(v);
    EXPECT_THROW(defaulted.CopyTo(validDst), System::InvalidOperationException);
    // ...and when the destination is also too short, the default state still wins,
    // proving the guard runs before the length check.
    std::vector<int> small{0};
    ArraySegment<int> shortDst(small);
    EXPECT_THROW(defaulted.CopyTo(shortDst), System::InvalidOperationException);
    // A valid source with a short valid destination reports the length instead.
    EXPECT_THROW(valid.CopyTo(shortDst), System::ArgumentException);
}

TEST(CoreMemorySafetyDefaultSegmentTests, ToArraySearchAndIndexOfRejectTheDefaultSegment) {
    ArraySegment<int> seg;
    EXPECT_THROW((void)seg.ToArray(), System::InvalidOperationException);
    EXPECT_THROW((void)seg.Contains(1), System::InvalidOperationException);
    EXPECT_THROW((void)seg.IndexOf(1), System::InvalidOperationException);
    EXPECT_EQ(messageOf([&] { (void)seg.ToArray(); }), kNullArrayMessage);
}

TEST(CoreMemorySafetyDefaultSegmentTests, TheUnguardedDoorsStillAnswer) {
    // .NET's Array/Offset/Count/Equals/GetHashCode do NOT call
    // ThrowInvalidOperationIfDefault, and neither do these. Guarding them would be an
    // over-correction, so it is asserted that they are NOT guarded.
    ArraySegment<int> seg;
    EXPECT_EQ(seg.getArrayProperty(), nullptr);
    EXPECT_EQ(seg.getOffsetProperty(), 0);
    EXPECT_EQ(seg.getCountProperty(), 0);
    ArraySegment<int> other;
    EXPECT_TRUE(seg.Equals(other));
    EXPECT_TRUE(seg == other);
    EXPECT_FALSE(seg != other);
    EXPECT_NO_THROW((void)seg.GetHashCode());
    EXPECT_NO_THROW((void)ArraySegment<int>::getEmpty());
    // ArraySegment<T>::Empty is a real, non-default segment over a shared empty vector,
    // so every guarded door works on it.
    auto empty = ArraySegment<int>::getEmpty();
    EXPECT_NO_THROW((void)empty.ToArray());
    EXPECT_NO_THROW((void)empty.Slice(0));
    EXPECT_FALSE(empty.Contains(1));
}

TEST(CoreMemorySafetyDefaultSegmentTests, NonDefaultSegmentsAreUnaffected) {
    std::vector<int> v{10, 20, 30, 40, 50};
    ArraySegment<int> seg(v, 1, 3);
    EXPECT_EQ(seg[0], 20);
    EXPECT_EQ(seg.Slice(1).getCountProperty(), 2);
    EXPECT_EQ(seg.Slice(1, 1).getCountProperty(), 1);
    EXPECT_EQ(seg.ToArray(), (std::vector<int>{20, 30, 40}));
    EXPECT_TRUE(seg.Contains(30));
    EXPECT_EQ(seg.IndexOf(40), 2);
    // #2328: the destination is sized by the caller now -- CopyTo rejects a short one rather
    // than growing it, matching .NET's Array.Copy.
    std::vector<int> dst(3);
    seg.CopyTo(dst);
    EXPECT_EQ(dst, (std::vector<int>{20, 30, 40}));
    // The out-of-range diagnostics on a real segment are unchanged.
    EXPECT_THROW((void)seg[3], System::ArgumentOutOfRangeException);
    EXPECT_THROW((void)seg.Slice(4), System::ArgumentOutOfRangeException);
}

TEST(CoreMemorySafetyDefaultSegmentTests, Fix2215_TheEnumerationDoorRejectsADefaultSegment) {
    // #2215 SHIPPED, AND THIS PIN IS INVERTED. begin()/end() are the port's GetEnumerator()
    // counterpart, and .NET's GetEnumerator() calls ThrowInvalidOperationIfDefault()
    // (`ArraySegment.cs:95-99`). They were the LAST unguarded door in the type: #2214 guarded
    // the other ten and could not touch these two, because guarding them means dropping their
    // noexcept -- a public signature change, now covered by docs/StandingApprovals.md SA-10.
    static_assert(!noexcept(std::declval<ArraySegment<int>&>().begin()),
                  "#2215: begin() must not be noexcept -- it throws for a default segment");
    static_assert(!noexcept(std::declval<ArraySegment<int>&>().end()),
                  "#2215: end() must not be noexcept");
    static_assert(!noexcept(std::declval<const ArraySegment<int>&>().begin()),
                  "#2215: the const overload too");
    static_assert(!noexcept(std::declval<const ArraySegment<int>&>().end()),
                  "#2215: the const overload too");

    ArraySegment<int> seg;
    EXPECT_THROW((void)seg.begin(), System::InvalidOperationException);
    EXPECT_THROW((void)seg.end(), System::InvalidOperationException);

    const ArraySegment<int>& cseg = seg;
    EXPECT_THROW((void)cseg.begin(), System::InvalidOperationException);
    EXPECT_THROW((void)cseg.end(), System::InvalidOperationException);

    // THE BEHAVIOUR THAT CHANGED, stated as the loop a caller actually writes: this used to run
    // zero times and report nothing. Silent emptiness is indistinguishable from a genuinely
    // empty segment, which is exactly why .NET refuses it.
    EXPECT_THROW({ for (int& x : seg) { (void)x; } }, System::InvalidOperationException);

    // ...and the message is the type's own, shared with the other ten doors #2214 guarded, so
    // the family cannot drift into two sentences for one condition.
    try {
        (void)seg.begin();
        ADD_FAILURE() << "expected InvalidOperationException";
    } catch (const System::InvalidOperationException& e) {
        EXPECT_STREQ(e.what(), "The underlying array is null.");
    }
}

TEST(CoreMemorySafetyDefaultSegmentTests, Fix2215_ANonDefaultSegmentIteratesExactlyAsBefore) {
    // The narrowing must be confined to the DEFAULT segment. An ordinary one -- including a
    // legitimately EMPTY one, which is the case most easily broken by a careless guard -- keeps
    // iterating and keeps its pointers.
    std::vector<int> data{10, 20, 30, 40};
    ArraySegment<int> seg(data, 1, 2);
    EXPECT_EQ(seg.end() - seg.begin(), 2);
    std::vector<int> seen;
    for (int& x : seg) seen.push_back(x);
    EXPECT_EQ(seen, (std::vector<int>{20, 30}));

    ArraySegment<int> empty(data, 2, 0);
    EXPECT_NO_THROW((void)empty.begin());
    EXPECT_EQ(empty.begin(), empty.end()) << "an empty segment is not a default segment";
    int iterations = 0;
    for (int& x : empty) { (void)x; ++iterations; }
    EXPECT_EQ(0, iterations);

    // An empty segment over an EMPTY vector is the sharpest form: data() may legitimately be
    // null there, and the guard must still let it through, because the ARRAY is present.
    std::vector<int> none;
    ArraySegment<int> overEmpty(none, 0, 0);
    EXPECT_NO_THROW((void)overEmpty.begin());
    EXPECT_NO_THROW((void)overEmpty.end());
}

// ===========================================================================
// SR-AUD-044 (CMS-C) -- every copy door preserves the whole source on overlap
// ===========================================================================
//
// EVERY direction assertion below uses std::string, not int, and that is load
// bearing: libstdc++ lowers BOTH std::copy and std::copy_backward over a
// trivially copyable type to __builtin_memmove, which is correct in either
// direction, so an int assertion cannot tell a direction-SELECTING copy from an
// unconditionally backward one. Measured while mutation-testing #2213: an
// unconditional std::copy_backward passed every int case. int is still covered,
// as a value regression, but never as the direction evidence.

namespace {

    // An element type that counts its copy-assignments, so a repair cannot silently
    // change how many element operations a copy performs.
    struct Counted {
        int value = 0;
        static int assignments;
        Counted() = default;
        explicit Counted(int v) : value(v) {}
        Counted(const Counted&) = default;
        Counted& operator=(const Counted& o) { ++assignments; value = o.value; return *this; }
        bool operator==(const Counted& o) const { return value == o.value; }
    };
    int Counted::assignments = 0;

} // namespace

TEST(CoreMemorySafetyOverlapTests, SpanCopyToAndTryCopyTo) {
    {   // right overlap
        auto v = abcd();
        Span<std::string> whole(v);
        whole.Slice(0, 3).CopyTo(whole.Slice(1, 3));
        EXPECT_EQ(shape(v), "aabc");
    }
    {   // left overlap -- already correct, must stay correct
        auto v = abcd();
        Span<std::string> whole(v);
        whole.Slice(1, 3).CopyTo(whole.Slice(0, 3));
        EXPECT_EQ(shape(v), "bcdd");
    }
    {   // right overlap through TryCopyTo
        auto v = abcd();
        Span<std::string> whole(v);
        EXPECT_TRUE(whole.Slice(0, 3).TryCopyTo(whole.Slice(1, 3)));
        EXPECT_EQ(shape(v), "aabc");
    }
    {   // left overlap through TryCopyTo
        auto v = abcd();
        Span<std::string> whole(v);
        EXPECT_TRUE(whole.Slice(1, 3).TryCopyTo(whole.Slice(0, 3)));
        EXPECT_EQ(shape(v), "bcdd");
    }
}

TEST(CoreMemorySafetyOverlapTests, ReadOnlySpanCopyToAndTryCopyTo) {
    {
        auto v = abcd();
        Span<std::string> whole(v);
        ReadOnlySpan<std::string>(whole.Slice(0, 3)).CopyTo(whole.Slice(1, 3));
        EXPECT_EQ(shape(v), "aabc");
    }
    {
        auto v = abcd();
        Span<std::string> whole(v);
        ReadOnlySpan<std::string>(whole.Slice(1, 3)).CopyTo(whole.Slice(0, 3));
        EXPECT_EQ(shape(v), "bcdd");
    }
    {
        auto v = abcd();
        Span<std::string> whole(v);
        EXPECT_TRUE(ReadOnlySpan<std::string>(whole.Slice(0, 3)).TryCopyTo(whole.Slice(1, 3)));
        EXPECT_EQ(shape(v), "aabc");
    }
    {
        auto v = abcd();
        Span<std::string> whole(v);
        EXPECT_TRUE(ReadOnlySpan<std::string>(whole.Slice(1, 3)).TryCopyTo(whole.Slice(0, 3)));
        EXPECT_EQ(shape(v), "bcdd");
    }
}

TEST(CoreMemorySafetyOverlapTests, MemoryAndReadOnlyMemoryCopyTo) {
    {
        auto v = abcd();
        Memory<std::string> src(v, 0, 3), dst(v, 1, 3);
        src.CopyTo(dst);
        EXPECT_EQ(shape(v), "aabc");
    }
    {
        auto v = abcd();
        Memory<std::string> src(v, 1, 3), dst(v, 0, 3);
        src.CopyTo(dst);
        EXPECT_EQ(shape(v), "bcdd");
    }
    {
        auto v = abcd();
        Memory<std::string> src(v, 0, 3), dst(v, 1, 3);
        EXPECT_TRUE(src.TryCopyTo(dst));
        EXPECT_EQ(shape(v), "aabc");
    }
    {   // ReadOnlyMemory forwards to the ReadOnlySpan doors
        auto v = abcd();
        Memory<std::string> src(v, 0, 3), dst(v, 1, 3);
        ReadOnlyMemory<std::string> ro = src;
        ro.CopyTo(dst);
        EXPECT_EQ(shape(v), "aabc");
    }
    {
        auto v = abcd();
        Memory<std::string> src(v, 0, 3), dst(v, 1, 3);
        ReadOnlyMemory<std::string> ro = src;
        EXPECT_TRUE(ro.TryCopyTo(dst));
        EXPECT_EQ(shape(v), "aabc");
    }
}

TEST(CoreMemorySafetyOverlapTests, StaticMemoryExtensionsCopyTo) {
    {
        auto v = abcd();
        Span<std::string> whole(v);
        System::MemoryExtensions::CopyTo<std::string>(
            ReadOnlySpan<std::string>(whole.Slice(0, 3)), whole.Slice(1, 3));
        EXPECT_EQ(shape(v), "aabc");
    }
    {
        auto v = abcd();
        Span<std::string> whole(v);
        System::MemoryExtensions::CopyTo<std::string>(whole.Slice(1, 3), whole.Slice(0, 3));
        EXPECT_EQ(shape(v), "bcdd");
    }
}

TEST(CoreMemorySafetyOverlapTests, ArrayCopyVectorOverloads) {
    {   // five-argument, right overlap: src and dst name the SAME vector
        auto v = abcd();
        System::Array::Copy<std::string>(v, 0, v, 1, 3);
        EXPECT_EQ(shape(v), "aabc");
    }
    {   // five-argument, left overlap
        auto v = abcd();
        System::Array::Copy<std::string>(v, 1, v, 0, 3);
        EXPECT_EQ(shape(v), "bcdd");
    }
    {   // three-argument, same vector: a degenerate self-copy that must not corrupt
        auto v = abcd();
        System::Array::Copy<std::string>(v, v, 3);
        EXPECT_EQ(shape(v), "abcd");
    }
    {   // ConstrainedCopy forwards to the five-argument overload
        auto v = abcd();
        System::Array::ConstrainedCopy<std::string>(v, 0, v, 1, 3);
        EXPECT_EQ(shape(v), "aabc");
    }
}

TEST(CoreMemorySafetyOverlapTests, ArraySegmentCopyToBothForms) {
    {   // segment -> the same backing vector, right overlap
        auto v = abcd();
        ArraySegment<std::string> src(v, 0, 3);
        src.CopyTo(v, 1);
        EXPECT_EQ(shape(v), "aabc");
    }
    {   // segment -> the same backing vector, left overlap
        auto v = abcd();
        ArraySegment<std::string> src(v, 1, 3);
        src.CopyTo(v, 0);
        EXPECT_EQ(shape(v), "bcdd");
    }
    {   // segment -> overlapping segment, right
        auto v = abcd();
        ArraySegment<std::string> src(v, 0, 3), dst(v, 1, 3);
        src.CopyTo(dst);
        EXPECT_EQ(shape(v), "aabc");
    }
    {   // segment -> overlapping segment, left
        auto v = abcd();
        ArraySegment<std::string> src(v, 1, 3), dst(v, 0, 3);
        src.CopyTo(dst);
        EXPECT_EQ(shape(v), "bcdd");
    }
}

TEST(CoreMemorySafetyOverlapTests, DegenerateAndBoundaryShapes) {
    {   // exact self: source == destination
        auto v = abcd();
        Span<std::string> whole(v);
        whole.CopyTo(whole);
        EXPECT_EQ(shape(v), "abcd");
    }
    {   // adjacent, not overlapping: destination begins exactly at the source end
        auto v = abcd();
        Span<std::string> whole(v);
        whole.Slice(0, 2).CopyTo(whole.Slice(2, 2));
        EXPECT_EQ(shape(v), "abab");
    }
    {   // zero length
        auto v = abcd();
        Span<std::string> whole(v);
        whole.Slice(1, 0).CopyTo(whole.Slice(2, 0));
        EXPECT_EQ(shape(v), "abcd");
    }
    {   // one element, overlapping by construction is impossible; disjoint one-element
        auto v = abcd();
        Span<std::string> whole(v);
        whole.Slice(0, 1).CopyTo(whole.Slice(3, 1));
        EXPECT_EQ(shape(v), "abca");
    }
    {   // zero-length slice at the very end
        auto v = abcd();
        Span<std::string> whole(v);
        whole.Slice(4, 0).CopyTo(whole.Slice(4, 0));
        EXPECT_EQ(shape(v), "abcd");
    }
    {   // nested slices still land on the right elements
        auto v = abcd();
        Span<std::string> whole(v);
        whole.Slice(0, 4).Slice(0, 3).CopyTo(whole.Slice(0, 4).Slice(1, 3));
        EXPECT_EQ(shape(v), "aabc");
    }
}

TEST(CoreMemorySafetyOverlapTests, NonOverlappingCopiesKeepTheirPreviousResults) {
    // Regression guard: the direction selection must not perturb the ordinary case.
    std::vector<int> src{1, 2, 3, 4};
    std::vector<int> dst(4, 0);
    Span<int>(src).CopyTo(Span<int>(dst));
    EXPECT_EQ(dst, (std::vector<int>{1, 2, 3, 4}));

    std::vector<int> dst2(4, 9);
    System::Array::Copy<int>(src, 1, dst2, 2, 2);
    EXPECT_EQ(dst2, (std::vector<int>{9, 9, 2, 3}));

    // #2328 replaced SR-AUD-055's resize with a rejection, so the destination is sized here.
    // #2214 had deliberately PRESERVED the resize while repairing this file's default-state and
    // overlap defects; that preservation was correct then and is superseded now.
    std::vector<int> dst3(4, 0);
    ArraySegment<int>(src, 0, 3).CopyTo(dst3, 1);
    EXPECT_EQ(dst3, (std::vector<int>{0, 1, 2, 3}));

    std::vector<int> dst3Short;
    EXPECT_THROW(ArraySegment<int>(src, 0, 3).CopyTo(dst3Short, 1), System::ArgumentException);
    EXPECT_TRUE(dst3Short.empty()) << "#2328: a rejected copy must not grow the destination";

    std::vector<int> short1(1, 0);
    EXPECT_THROW(Span<int>(src).CopyTo(Span<int>(short1)), System::ArgumentException);
    EXPECT_FALSE(Span<int>(src).TryCopyTo(Span<int>(short1)));
    EXPECT_EQ(short1[0], 0);   // a false TryCopyTo still writes nothing
}

TEST(CoreMemorySafetyOverlapTests, ElementAssignmentCountIsExactlyTheLength) {
    // An observable element type: the direction changes, the amount of work does not.
    std::vector<Counted> v{Counted(1), Counted(2), Counted(3), Counted(4)};
    Span<Counted> whole(v);

    Counted::assignments = 0;
    whole.Slice(0, 3).CopyTo(whole.Slice(1, 3));           // backward branch
    EXPECT_EQ(Counted::assignments, 3);
    EXPECT_EQ(v[0].value, 1);
    EXPECT_EQ(v[1].value, 1);
    EXPECT_EQ(v[2].value, 2);
    EXPECT_EQ(v[3].value, 3);

    std::vector<Counted> w{Counted(1), Counted(2), Counted(3), Counted(4)};
    Span<Counted> whole2(w);
    Counted::assignments = 0;
    whole2.Slice(1, 3).CopyTo(whole2.Slice(0, 3));         // forward branch
    EXPECT_EQ(Counted::assignments, 3);
    EXPECT_EQ(w[0].value, 2);
    EXPECT_EQ(w[3].value, 4);

    // source == destination short-circuits to no work at all
    Counted::assignments = 0;
    whole2.CopyTo(whole2);
    EXPECT_EQ(Counted::assignments, 0);
}

TEST(CoreMemorySafetyOverlapTests, ViewsAreTriviallyCopyableValuesWithNoInvalidatingOperation) {
    // The move/copy matrix for this family reduces to this property: every view is a
    // trivially copyable value describing (owner, offset, length), so copying, moving,
    // move-assigning or destroying ONE view cannot invalidate ANOTHER. Pinning it with a
    // static_assert means a future member addition that breaks the reasoning fails the
    // build rather than the argument.
    static_assert(std::is_trivially_copyable_v<Span<int>>);
    static_assert(std::is_trivially_copyable_v<ReadOnlySpan<int>>);
    static_assert(std::is_trivially_copyable_v<Memory<int>>);
    static_assert(std::is_trivially_copyable_v<ReadOnlyMemory<int>>);
    static_assert(std::is_trivially_copyable_v<ArraySegment<int>>);

    std::vector<int> v{1, 2, 3, 4};
    ArraySegment<int> a(v, 1, 2);
    ArraySegment<int> b = a;                 // copy construction
    ArraySegment<int> c = std::move(a);      // move construction
    EXPECT_TRUE(b.Equals(c));
    ArraySegment<int> d;
    d = b;                                   // copy assignment over a default
    EXPECT_TRUE(d.Equals(b));
    ArraySegment<int> e(v, 0, 4);
    e = std::move(d);                        // move assignment over a populated target
    EXPECT_TRUE(e.Equals(b));
    // A moved-from view of a trivially copyable type still describes the same region.
    EXPECT_TRUE(a.Equals(c));
    // Destroying one view leaves the others usable.
    { ArraySegment<int> scoped = b; EXPECT_EQ(scoped.getCountProperty(), 2); }
    EXPECT_EQ(b[0], 2);
    EXPECT_EQ(c[1], 3);
}
