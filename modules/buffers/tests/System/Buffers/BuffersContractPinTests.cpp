// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Ticket #2061 — behaviour pins for every System::Buffers finding this batch did NOT
// repair, so a future approved option cannot land silently and a future reader cannot
// mistake a documented gap for an accident.
//
// These tests assert what the code does TODAY. Several of them pin behaviour that
// DIVERGES from .NET; each says so, names the blocked ticket that would change it, and
// must be updated together with that ticket. See docs/BuffersNamespaceReviewPlan.md.
//
// Nothing here approves, implements or preselects any part of #2056, #2057, #2058,
// #2059 or #2060.
#include <gtest/gtest.h>
#include <cstdint>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Buffers/ArrayBufferWriter.hpp"
#include "System/Buffers/ArrayPool.hpp"
#include "System/Buffers/IPinnable.hpp"
#include "System/Buffers/MemoryPool.hpp"
#include "System/Buffers/ReadOnlySequence.hpp"
#include "System/Buffers/ReadOnlySequenceSegment.hpp"
#include "System/Buffers/SearchValues.hpp"
#include "System/Buffers/StandardFormat.hpp"
#include "System/Buffers/Text/Utf8Parser.hpp"
#include "System/Span.hpp"

using SharpRuntime::intcs;
using System::Buffers::ArrayBufferWriter;
using System::Buffers::ArrayPool;
using System::Buffers::MemoryHandle;
using System::Buffers::MemoryPool;
using System::Buffers::MemoryPoolHeapOwner_;
using System::Buffers::ReadOnlySequence;
using System::Buffers::ReadOnlySequenceSegment;
using System::Buffers::SearchValues;
using System::Buffers::StandardFormat;
using System::Buffers::Text::Utf8Parser;

// ===========================================================================
// Object layout — §11 of the review plan.
//
// Every gated option in this namespace would have to move one of these. Pinning them
// as static_asserts means such an option cannot land without a visible, deliberate
// edit to this file.
// ===========================================================================

static_assert(sizeof(ReadOnlySequence<int>) == 32,
              "#2057/#2058 would change ReadOnlySequence<T>'s public object layout");
static_assert(sizeof(ReadOnlySequence<int>::Enumerator) == 16,
              "#2057/#2058 would change the enumerator's object layout");
static_assert(sizeof(MemoryPoolHeapOwner_<int>) == 32,
              "#2056's terminal disposed flag would take this to 40");
static_assert(sizeof(ArrayBufferWriter<char>) == 40,
              "#2051 must not have added state");
static_assert(sizeof(StandardFormat) == 2,
              "#2052 must not have added state");
static_assert(sizeof(MemoryHandle) == 24,
              "#2059's move-only or refcounted semantics would change this");

TEST(BuffersLayoutPinTests, LayoutsAreStaticallyAsserted) {
    SUCCEED() << "The static_asserts above are the test; this keeps it visible in the suite.";
}

// ===========================================================================
// SR-AUD-071 — no terminal disposed state (blocked #2056)
//
// DIVERGES FROM .NET: .NET's pool owner throws ObjectDisposedException from its
// Memory getter once disposed. This port returns an empty Memory.
// ===========================================================================

TEST(MemoryOwnerDisposedPinTests, GetMemoryAfterDispose_ReturnsEmptyAndDoesNotThrow) {
    auto owner = MemoryPool<int>::Shared().Rent(16);
    ASSERT_EQ(owner->getMemoryProperty().getLengthProperty(), 16);

    owner->Dispose();

    // .NET would throw ObjectDisposedException here. #2056.
    EXPECT_NO_THROW({
        auto after = owner->getMemoryProperty();
        EXPECT_EQ(after.getLengthProperty(), 0);
    });
}

TEST(MemoryOwnerDisposedPinTests, RepeatedDisposeIsIdempotent) {
    auto owner = MemoryPool<int>::Shared().Rent(8);
    owner->Dispose();
    EXPECT_NO_THROW(owner->Dispose());
    EXPECT_EQ(owner->getMemoryProperty().getLengthProperty(), 0);
}

TEST(MemoryOwnerDisposedPinTests, MemoryRetainedAcrossDispose_KeepsItsStaleLength) {
    // The length is what makes this dangerous: a retained view still claims to cover
    // storage the owner has released, so a caller that trusts it reads freed memory.
    // Reading THROUGH the view is deliberately not attempted here -- it is undefined
    // behaviour and would make this suite unrunnable under AddressSanitizer. #2056.
    auto owner = MemoryPool<int>::Shared().Rent(16);
    auto retained = owner->getMemoryProperty();
    ASSERT_EQ(retained.getLengthProperty(), 16);

    owner->Dispose();

    EXPECT_EQ(retained.getLengthProperty(), 16)
        << "if this becomes 0, the ownership half of #2056 has landed";
}

TEST(MemoryOwnerDisposedPinTests, ZeroLengthRentIsIndistinguishableFromADisposedOwner) {
    // This is precisely why #2056 needs a flag rather than a cleverly overloaded state.
    auto live = MemoryPool<int>::Shared().Rent(0);
    auto dead = MemoryPool<int>::Shared().Rent(4);
    dead->Dispose();
    EXPECT_EQ(live->getMemoryProperty().getLengthProperty(),
              dead->getMemoryProperty().getLengthProperty());
}

// ===========================================================================
// SR-AUD-074 — default versus Empty (blocked #2057)
//
// DIVERGES FROM .NET: .NET's default sequence enumerates NO segment and Empty
// enumerates one. Here both enumerate one.
// ===========================================================================

static int countSegments(const ReadOnlySequence<int>& seq) {
    auto e = seq.GetEnumerator();
    int n = 0;
    while (e.MoveNext()) ++n;
    return n;
}

TEST(ReadOnlySequenceDefaultPinTests, DefaultAndEmptyBothEnumerateExactlyOneSegment) {
    ReadOnlySequence<int> def;
    EXPECT_EQ(countSegments(def), 1) << ".NET yields 0 here; #2057";
    EXPECT_EQ(countSegments(ReadOnlySequence<int>::getEmpty()), 1);
}

TEST(ReadOnlySequenceDefaultPinTests, DefaultAndEmptyAgreeOnEveryOtherObservable) {
    ReadOnlySequence<int> def;
    auto empty = ReadOnlySequence<int>::getEmpty();
    EXPECT_EQ(def.getLengthProperty(), empty.getLengthProperty());
    EXPECT_EQ(def.getIsEmptyProperty(), empty.getIsEmptyProperty());
    EXPECT_EQ(def.getStartProperty().GetInteger(), empty.getStartProperty().GetInteger());
    EXPECT_EQ(def.getEndProperty().GetInteger(), empty.getEndProperty().GetInteger());
    EXPECT_TRUE(def.getIsSingleSegmentProperty());
}

TEST(ReadOnlySequenceDefaultPinTests, TheSingleEnumeratedSegmentIsEmpty) {
    ReadOnlySequence<int> def;
    auto e = def.GetEnumerator();
    ASSERT_TRUE(e.MoveNext());
    EXPECT_EQ(e.getCurrentProperty().getLengthProperty(), 0);
    EXPECT_FALSE(e.MoveNext());
}

// ===========================================================================
// SR-AUD-087 — the segment chain builds nothing (blocked #2058)
// ===========================================================================

namespace {
    /** A minimal concrete segment, the only way a consumer can use the protected setters. */
    class TestSegment : public ReadOnlySequenceSegment<int> {
    public:
        TestSegment(System::ReadOnlyMemory<int> memory, long long runningIndex) {
            setMemoryProperty(memory);
            setRunningIndexProperty(runningIndex);
        }
        void linkTo(TestSegment* next) { setNextProperty(next); }
    };
}

TEST(ReadOnlySequenceSegmentPinTests, ANodeChainCanBeBuiltButNotConsumed) {
    std::vector<int> a{1, 2};
    std::vector<int> b{3, 4, 5};
    TestSegment first(System::ReadOnlyMemory<int>(a.data(), 2), 0);
    TestSegment second(System::ReadOnlyMemory<int>(b.data(), 3), 2);
    first.linkTo(&second);

    // The node shape itself works...
    EXPECT_EQ(first.getNextProperty(), &second);
    EXPECT_EQ(second.getRunningIndexProperty(), 2LL);
    EXPECT_EQ(first.getMemoryProperty().getLengthProperty(), 2);

    // ...and there is nothing that turns it into a sequence. ReadOnlySequence<int> has
    // exactly three constructors -- default, std::vector and raw pointer/length -- and
    // none of them accepts a segment. #2058 would add one; until then this static_assert
    // is the pin, and it fails the moment such a constructor appears.
    static_assert(!std::is_constructible_v<ReadOnlySequence<int>,
                                           ReadOnlySequenceSegment<int>*, intcs,
                                           ReadOnlySequenceSegment<int>*, intcs>,
                  "a segment-chain constructor now exists -- #2058 has landed");
    static_assert(!std::is_constructible_v<ReadOnlySequence<int>,
                                           ReadOnlySequenceSegment<int>*>,
                  "a segment constructor now exists -- #2058 has landed");
}

TEST(ReadOnlySequenceSegmentPinTests, EverySequenceReportsASingleSegment) {
    EXPECT_TRUE(ReadOnlySequence<int>().getIsSingleSegmentProperty());
    EXPECT_TRUE(ReadOnlySequence<int>(std::vector<int>{1, 2, 3}).getIsSingleSegmentProperty());
}

// ===========================================================================
// SR-AUD-088 — MemoryHandle performs no RAII cleanup (blocked #2059)
// ===========================================================================

namespace {
    /** Counts Unpin calls so scope exit can be observed rather than assumed. */
    struct CountingPinnable : System::Buffers::IPinnable {
        int unpinCount = 0;
        int value      = 7;
        MemoryHandle Pin(intcs) override { return MemoryHandle(&value, this); }
        void Unpin() override { ++unpinCount; }
    };
}

TEST(MemoryHandlePinTests, ScopeExitDoesNotUnpin) {
    CountingPinnable p;
    {
        MemoryHandle handle = p.Pin(0);
        EXPECT_EQ(handle.getPointerProperty(), &p.value);
    }   // .NET-style RAII would unpin here. It does not. #2059.
    EXPECT_EQ(p.unpinCount, 0)
        << "if this becomes 1, MemoryHandle has grown a destructor -- #2059 has landed";
}

TEST(MemoryHandlePinTests, ExplicitDisposeUnpinsExactlyOnce) {
    CountingPinnable p;
    MemoryHandle handle = p.Pin(0);
    handle.Dispose();
    EXPECT_EQ(p.unpinCount, 1);
    handle.Dispose();   // already detached
    EXPECT_EQ(p.unpinCount, 1);
    EXPECT_EQ(handle.getPointerProperty(), nullptr);
}

TEST(MemoryHandlePinTests, ACopyStillReferencesTheSamePinnable) {
    // This is why #2059 cannot simply add a destructor: an unpinning destructor on a
    // freely copyable aggregate would unpin once per copy.
    CountingPinnable p;
    MemoryHandle original = p.Pin(0);
    MemoryHandle copy = original;
    copy.Dispose();
    original.Dispose();
    EXPECT_EQ(p.unpinCount, 2) << "two handles, two Unpins -- one pin";
}

// ===========================================================================
// SR-AUD-086 — the leading '+' asymmetry (deferred #2060)
//
// All four combinations pinned, so the verification lands against a measured baseline.
// ===========================================================================

TEST(Utf8ParserPlusSignPinTests, SignedDefaultAndDRejectALeadingPlus) {
    const uint8_t text[] = {'+', '4', '2'};
    System::ReadOnlySpan<uint8_t> src(text, 3);
    for (char format : {'\0', 'G', 'D'}) {
        int32_t value = 99;
        intcs consumed = 99;
        EXPECT_FALSE(Utf8Parser::TryParse(src, value, consumed, format)) << "format " << (int)format;
        EXPECT_EQ(value, 0);       // CCF-014: both outputs normalized on failure
        EXPECT_EQ(consumed, 0);
    }
}

TEST(Utf8ParserPlusSignPinTests, UnsignedDefaultAndDRejectALeadingPlus) {
    const uint8_t text[] = {'+', '4', '2'};
    System::ReadOnlySpan<uint8_t> src(text, 3);
    for (char format : {'\0', 'G', 'D'}) {
        uint32_t value = 99;
        intcs consumed = 99;
        EXPECT_FALSE(Utf8Parser::TryParse(src, value, consumed, format)) << "format " << (int)format;
        EXPECT_EQ(value, 0u);
        EXPECT_EQ(consumed, 0);
    }
}

TEST(Utf8ParserPlusSignPinTests, SignedNAcceptsALeadingPlus) {
    const uint8_t text[] = {'+', '4', '2'};
    System::ReadOnlySpan<uint8_t> src(text, 3);
    int32_t value = 0;
    intcs consumed = 0;
    ASSERT_TRUE(Utf8Parser::TryParse(src, value, consumed, 'N'));
    EXPECT_EQ(value, 42);
    EXPECT_EQ(consumed, 3);
}

TEST(Utf8ParserPlusSignPinTests, UnsignedNAcceptsALeadingPlusToo) {
    const uint8_t text[] = {'+', '4', '2'};
    System::ReadOnlySpan<uint8_t> src(text, 3);
    uint32_t value = 0;
    intcs consumed = 0;
    ASSERT_TRUE(Utf8Parser::TryParse(src, value, consumed, 'N'));
    EXPECT_EQ(value, 42u);
    EXPECT_EQ(consumed, 3);
}

TEST(Utf8ParserPlusSignPinTests, AMinusIsAcceptedBySignedDefaultAndDButNotByUnsigned) {
    const uint8_t text[] = {'-', '4', '2'};
    System::ReadOnlySpan<uint8_t> src(text, 3);
    int32_t signedValue = 0;
    intcs consumed = 0;
    ASSERT_TRUE(Utf8Parser::TryParse(src, signedValue, consumed));
    EXPECT_EQ(signedValue, -42);
    EXPECT_EQ(consumed, 3);

    uint32_t unsignedValue = 99;
    consumed = 99;
    EXPECT_FALSE(Utf8Parser::TryParse(src, unsignedValue, consumed));
    EXPECT_EQ(unsignedValue, 0u);
    EXPECT_EQ(consumed, 0);
}

// ===========================================================================
// Contracts this batch relies on and did not change
// ===========================================================================

TEST(ReadOnlySequenceCopyToPinTests, AShortDestinationIsLeftCompletelyUntouched) {
    // No partial write before the failure: the length check precedes the copy.
    ReadOnlySequence<int> seq(std::vector<int>{1, 2, 3});
    int destination[3] = {-1, -1, -1};
    System::Span<int> tooSmall(destination, 2);
    EXPECT_THROW(seq.CopyTo(tooSmall), System::ArgumentOutOfRangeException);
    EXPECT_EQ(destination[0], -1);
    EXPECT_EQ(destination[1], -1);
    EXPECT_EQ(destination[2], -1);
}

TEST(ReadOnlySequenceCopyToPinTests, AnExactlySizedDestinationSucceeds) {
    ReadOnlySequence<int> seq(std::vector<int>{1, 2, 3});
    int destination[3] = {0, 0, 0};
    seq.CopyTo(System::Span<int>(destination, 3));
    EXPECT_EQ(destination[0], 1);
    EXPECT_EQ(destination[2], 3);
}

TEST(ArrayPoolOwnershipPinTests, ReturnDoesNotTakeOwnershipAndDoesNotPool) {
    // Rent hands back a by-value vector, so the caller keeps writing to it after Return --
    // legal here, forbidden in .NET -- and Return never yields the storage to anyone else.
    auto& pool = ArrayPool<int>::Shared();
    auto buffer = pool.Rent(4);
    buffer[0] = 11;
    pool.Return(buffer, /*clearArray=*/false);
    EXPECT_EQ(buffer.size(), 4u);
    EXPECT_EQ(buffer[0], 11) << "Return did not clear, and the caller still owns the storage";

    pool.Return(buffer, /*clearArray=*/true);
    EXPECT_EQ(buffer[0], 0) << "clearArray zeroes the CALLER's vector";

    auto second = pool.Rent(4);
    EXPECT_NE(second.data(), buffer.data()) << "no reuse: every Rent allocates";
}

TEST(SearchValuesPinTests, IsImmutableAfterConstructionAndIndependentOfItsSource) {
    std::vector<int> source{1, 2, 3};
    SearchValues<int> values(source);
    source.clear();
    source.push_back(99);
    EXPECT_TRUE(values.Contains(1));
    EXPECT_TRUE(values.Contains(3));
    EXPECT_FALSE(values.Contains(99));
    EXPECT_EQ(values.GetValues().size(), 3u);
}

TEST(MemoryPoolPinTests, RentBoundsAreUnchanged) {
    auto& pool = MemoryPool<int>::Shared();
    EXPECT_EQ(pool.getMaxBufferSizeProperty(), MemoryPool<int>::MaxArrayLength);
    EXPECT_THROW((void)pool.Rent(-2), System::ArgumentOutOfRangeException);
    EXPECT_THROW((void)pool.Rent(MemoryPool<int>::MaxArrayLength + 1),
                 System::ArgumentOutOfRangeException);
    // Rent(MaxArrayLength) itself is deliberately NOT exercised: measured, it SUCCEEDS on
    // this platform -- 8 GB of zeroed ints under overcommit, 43 s -- so it is neither a
    // useful bound nor an acceptable cost in a permanent suite.
    // -1 is the "implementation chooses" sentinel and is sized in BYTES, not elements.
    auto defaulted = MemoryPool<int>::Shared().Rent();
    EXPECT_EQ(defaulted->getMemoryProperty().getLengthProperty(),
              1 + (4095 / static_cast<intcs>(sizeof(int))));
}
