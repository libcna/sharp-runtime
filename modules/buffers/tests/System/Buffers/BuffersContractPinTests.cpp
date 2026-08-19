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
#include "System/SequencePosition.hpp"
#include <type_traits>
#include <cstring>
#include "System/ObjectDisposedException.hpp"
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

static_assert(sizeof(ReadOnlySequence<int>) == 40,
              "#2057 added exactly one member -- the has-a-buffer discriminator, 32 -> 40 -- "
              "under docs/StandingApprovals.md SA-3. #2058 is still open and any further data "
              "member here needs its own approval.");
static_assert(sizeof(ReadOnlySequence<int>::Enumerator) == 16,
              "#2057/#2058 would change the enumerator's object layout");
static_assert(sizeof(MemoryPoolHeapOwner_<int>) == 40,
              "#2056(a) added exactly one member -- the terminal disposed flag, 32 -> 40 -- "
              "under docs/StandingApprovals.md SA-3. Any further data member here is a new "
              "object-layout change and needs its own approval.");
static_assert(sizeof(ArrayBufferWriter<char>) == 40,
              "#2051 must not have added state");
static_assert(sizeof(StandardFormat) == 2,
              "#2052 must not have added state");
static_assert(sizeof(MemoryHandle) == 24,
              "#2059 made the two members PRIVATE, which is an access change and not a layout "
              "change -- 24 before and 24 after, so no consumer rebuilds. A destructor or "
              "refcounted semantics would change this, and #2059 measured the reference and "
              "declined both.");

TEST(BuffersLayoutPinTests, LayoutsAreStaticallyAsserted) {
    SUCCEED() << "The static_asserts above are the test; this keeps it visible in the suite.";
}

// ===========================================================================
// SR-AUD-071 — half (a) LANDED (#2056). Half (b) is still open, and is pinned
// as such rather than quietly bundled.
// ===========================================================================

TEST(MemoryOwnerDisposedPinTests, Fix2056_GetMemoryAfterDisposeThrows) {
    // .NET's ArrayMemoryPoolBuffer.Memory does exactly this -- ObjectDisposedException.ThrowIf
    // on a null array (ArrayMemoryPool.ArrayMemoryPoolBuffer.cs:18-25). Before #2056 this port
    // returned a ZERO-LENGTH Memory, which a caller could not tell from a live Rent(0).
    auto owner = MemoryPool<int>::Shared().Rent(16);
    ASSERT_EQ(owner->getMemoryProperty().getLengthProperty(), 16);

    owner->Dispose();
    EXPECT_THROW((void)owner->getMemoryProperty(), System::ObjectDisposedException);
}

TEST(MemoryOwnerDisposedPinTests, Fix2056_RepeatedDisposeIsStillIdempotent) {
    auto owner = MemoryPool<int>::Shared().Rent(8);
    owner->Dispose();
    EXPECT_NO_THROW(owner->Dispose());
    EXPECT_THROW((void)owner->getMemoryProperty(), System::ObjectDisposedException);
}

TEST(MemoryOwnerDisposedPinTests, Fix2056_AZeroLengthRentIsNowDistinguishableFromADisposedOwner) {
    // The row that made the flag necessary. A live Rent(0) answers; a disposed owner throws.
    auto live = MemoryPool<int>::Shared().Rent(0);
    auto dead = MemoryPool<int>::Shared().Rent(4);
    dead->Dispose();

    EXPECT_NO_THROW((void)live->getMemoryProperty());
    EXPECT_EQ(live->getMemoryProperty().getLengthProperty(), 0);
    EXPECT_THROW((void)dead->getMemoryProperty(), System::ObjectDisposedException);
}

TEST(MemoryOwnerDisposedPinTests, Pin2056b_MemoryRetainedAcrossDisposeStillKeepsItsStaleLength) {
    // HALF (b) IS STILL OPEN, and this pin says so rather than letting half a repair look
    // whole. A Memory<T> obtained BEFORE Dispose keeps a pointer and a length over storage the
    // owner has released. getMemoryProperty() cannot defend against it -- by the time the
    // caller holds the Memory, this object is no longer in the path -- and repairing it is a
    // Memory<T> ownership change in Core.Base.
    //
    // Reading THROUGH the view is deliberately not attempted: it is undefined behaviour and
    // would make this suite unrunnable under AddressSanitizer.
    auto owner = MemoryPool<int>::Shared().Rent(16);
    auto retained = owner->getMemoryProperty();
    ASSERT_EQ(retained.getLengthProperty(), 16);

    owner->Dispose();

    EXPECT_EQ(retained.getLengthProperty(), 16)
        << "if this becomes 0, the ownership half of #2056 has landed and this pin must be "
           "updated in that change";
}

TEST(MemoryOwnerDisposedPinTests, Fix2056_TheStorageLifetimeIsDELIBERATELYUnchanged) {
    // .NET needs no flag: it nulls _array and tests `array is null`. The port's natural
    // equivalent is a unique_ptr, which would even make the object SMALLER -- and it was
    // rejected on purpose. reset() frees the storage DETERMINISTICALLY, where clear() +
    // shrink_to_fit() is non-binding; with half (b) still open, that would turn a latent
    // use-after-free from "usually survives" into "always broken" while nothing yet fixes it.
    //
    // This case exists so that reasoning is a decision on the record rather than a comment: if
    // a future change adopts the null discriminator, it must confront half (b) at the same time.
    auto owner = MemoryPool<int>::Shared().Rent(16);
    auto retained = owner->getMemoryProperty();
    owner->Dispose();
    EXPECT_EQ(retained.getLengthProperty(), 16);
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

TEST(ReadOnlySequenceDefaultPinTests, Fix2057_DefaultEnumeratesNoSegmentsAndEmptyEnumeratesOne) {
    // #2057 LANDED. .NET's Empty is `new ReadOnlySequence<T>(Array.Empty<T>())`
    // (ReadOnlySequence.cs:26) -- buffer-backed, so it has a start object and yields ONE
    // segment. The default-constructed sequence has a null start object, and MoveNext returns
    // false immediately for it (ReadOnlySequence.cs:642-647). Both used to yield one here.
    ReadOnlySequence<int> def;
    EXPECT_EQ(countSegments(def), 0);
    EXPECT_EQ(countSegments(ReadOnlySequence<int>::getEmpty()), 1);
}

TEST(ReadOnlySequenceDefaultPinTests, Fix2057_DefaultAndEmptyStillAgreeOnEveryOtherObservable) {
    // The invariance row, and the reason the distinction needed a member: everything ELSE about
    // the two is identical, so only enumeration can tell them apart. .NET is the same.
    ReadOnlySequence<int> def;
    auto empty = ReadOnlySequence<int>::getEmpty();
    EXPECT_EQ(def.getLengthProperty(), empty.getLengthProperty());
    EXPECT_EQ(def.getIsEmptyProperty(), empty.getIsEmptyProperty());
    EXPECT_EQ(def.getStartProperty().GetInteger(), empty.getStartProperty().GetInteger());
    EXPECT_EQ(def.getEndProperty().GetInteger(), empty.getEndProperty().GetInteger());
    EXPECT_TRUE(def.getIsSingleSegmentProperty());
}

TEST(ReadOnlySequenceDefaultPinTests, Fix2057_EmptysSingleSegmentIsZeroLength) {
    auto empty = ReadOnlySequence<int>::getEmpty();
    auto e = empty.GetEnumerator();
    ASSERT_TRUE(e.MoveNext());
    EXPECT_EQ(e.getCurrentProperty().getLengthProperty(), 0);
    EXPECT_FALSE(e.MoveNext());
}

TEST(Fix2057_EveryBufferBackedConstructionYieldsASegment, IncludingTheDegenerateOnes) {
    // The discriminator must be set by EVERY buffer-taking constructor, not just the one the
    // test happened to use. `(nullptr, 0)` is the trap: it is a valid buffer-backed sequence
    // with no bytes, and it must still enumerate one segment.
    EXPECT_EQ(countSegments(ReadOnlySequence<int>(std::vector<int>{})), 1);
    EXPECT_EQ(countSegments(ReadOnlySequence<int>(std::vector<int>{1, 2, 3})), 1);
    EXPECT_EQ(countSegments(ReadOnlySequence<int>(static_cast<const int*>(nullptr), 0)), 1);
    std::vector<int> data{7, 8};
    EXPECT_EQ(countSegments(ReadOnlySequence<int>(data.data(), 2)), 1);
}

// ===========================================================================
// SR-AUD-087 — the segment chain builds nothing (#2058: DECLARED, not blocked)
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
    // exactly three constructors -- default, std::vector and raw pointer/length -- and none of
    // them accepts a segment.
    //
    // #2058 DECIDED on 2026-08-19: this is a DECLARED LIMITATION, not a pending repair. .NET has
    // ReadOnlySequence(startSegment, startIndex, endSegment, endIndex) (ReadOnlySequence.cs:94);
    // adding it here would be a public object-layout change on a type consumers hold by value,
    // plus multi-segment rewrites of First, six Slice overloads, two GetPosition overloads,
    // TryGet, ToArray, CopyTo, the enumerator and SequenceReader's snapshot. That was offered and
    // declined. These static_asserts are the declaration and they fail the moment it is withdrawn.
    static_assert(!std::is_constructible_v<ReadOnlySequence<int>,
                                           ReadOnlySequenceSegment<int>*, intcs,
                                           ReadOnlySequenceSegment<int>*, intcs>,
                  "a segment-chain constructor now exists -- #2058's declared limitation "
                  "has been withdrawn and the header note must be rewritten");
    static_assert(!std::is_constructible_v<ReadOnlySequence<int>,
                                           ReadOnlySequenceSegment<int>*>,
                  "a segment constructor now exists -- see above");
}

TEST(ReadOnlySequenceSegmentPinTests, EverySequenceReportsASingleSegment) {
    // In .NET this is a real question -- IsSingleSegment is `_startObject == _endObject`
    // (ReadOnlySequence.cs:41-45) -- and here it is a hard-coded true. #2058 declares that, so the
    // pin asserts it holds for EVERY constructor this port offers rather than for a sample: an
    // answer that is constant is only demonstrably constant if every door is tried.
    std::vector<int> raw{1, 2, 3};
    EXPECT_TRUE(ReadOnlySequence<int>().getIsSingleSegmentProperty());
    EXPECT_TRUE(ReadOnlySequence<int>(std::vector<int>{1, 2, 3}).getIsSingleSegmentProperty());
    EXPECT_TRUE(ReadOnlySequence<int>(std::vector<int>{}).getIsSingleSegmentProperty());
    EXPECT_TRUE(ReadOnlySequence<int>(raw.data(), 3).getIsSingleSegmentProperty());
    EXPECT_TRUE(ReadOnlySequence<int>(raw.data(), 0).getIsSingleSegmentProperty());
}

// ===========================================================================
// SR-AUD-088 — MemoryHandle performs no RAII cleanup (#2059 RESOLVED).
//
// THE FINDING'S PREMISE DOES NOT SURVIVE THE REFERENCE, and the pins below used to
// restate it. .NET's MemoryHandle is `public unsafe struct MemoryHandle : IDisposable`
// (MemoryHandle.cs:12) -- a value type with no finalizer -- so scope exit does not unpin
// THERE EITHER. `using var h = memory.Pin();` is a language construct that calls
// Dispose(); it is not something the type does.
//
// What SR-AUD-088 actually found was a DOC-COMMENT that promised "or let the destructor
// do it". That promise was the defect and it is gone. The behaviour it described was
// never wrong.
//
// #2059 therefore declines the destructor rather than deferring it: adding one would be
// a divergence from .NET, and the copy hazard below is the second, independent reason.
// What #2059 DID land is the divergence the ticket never named -- the two data members
// were public here and are private in .NET -- under SA-8, at zero migration sites.
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
    }   // No unpin here -- and .NET does not unpin here either (MemoryHandle.cs:12, a
        // struct with no finalizer). This asserts PARITY, not a known gap.
    EXPECT_EQ(p.unpinCount, 0)
        << "if this becomes 1, MemoryHandle has grown a destructor and now diverges from "
           ".NET -- #2059 measured that and declined it";
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
    // The SECOND reason #2059 declined the destructor, independent of the reference: an
    // unpinning destructor on a freely copyable handle would unpin once per copy for a
    // single pin. .NET has the same copy semantics and the same absence of a destructor.
    CountingPinnable p;
    MemoryHandle original = p.Pin(0);
    MemoryHandle copy = original;
    copy.Dispose();
    original.Dispose();
    EXPECT_EQ(p.unpinCount, 2) << "two handles, two Unpins -- one pin";
}

namespace {
    /** True iff `T::pointer_` is reachable from outside the type. */
    template <typename T>
    concept HasPublicPointerMember = requires(T h) { h.pointer_; };
    template <typename T>
    concept HasPublicPinnableMember = requires(T h) { h.pinnable_; };
}

TEST(MemoryHandlePinTests, TheRepresentationIsPrivateAsInDotNet) {
    // .NET publishes only `Pointer`, as a getter (MemoryHandle.cs:35). Both fields are
    // private there and are now private here.
    //
    // The parameter is DEPENDENT on purpose: gcc evaluates a non-dependent `requires`
    // eagerly and hard-errors on the access instead of yielding false (the #2299 trap).
    static_assert(!HasPublicPointerMember<MemoryHandle>,
                  "#2059: pointer_ is private -- read it with getPointerProperty()");
    static_assert(!HasPublicPinnableMember<MemoryHandle>,
                  "#2059: pinnable_ is private, and .NET publishes no accessor for it at all");

    // What did NOT change, asserted so the pin proves the boundary is an access change and
    // nothing else: the two public constructors, the getter, copyability and the size.
    CountingPinnable p;
    MemoryHandle handle(&p.value, &p);
    EXPECT_EQ(handle.getPointerProperty(), &p.value);
    MemoryHandle copied = handle;
    EXPECT_EQ(copied.getPointerProperty(), &p.value);
    EXPECT_EQ(MemoryHandle().getPointerProperty(), nullptr);
    static_assert(std::is_copy_constructible_v<MemoryHandle>, "still copyable");
    static_assert(sizeof(MemoryHandle) == 24, "an access change moves no layout");
    handle.Dispose();
    copied.Dispose();
}

// ===========================================================================
// SR-AUD-086 — the leading '+' asymmetry (#2060 RESOLVED).
//
// The finding made TWO claims about .NET and pinned all four combinations so the verification
// could land against a measured baseline. That was the right shape, and the reference settles
// both claims -- with only ONE of them a defect here:
//
//   * Signed D accepts '+'   (Utf8Parser.Integer.Signed.D.cs:16-31)   -> this port did NOT. FIXED.
//   * Unsigned D rejects '+' (Utf8Parser.Integer.Unsigned.D.cs -- no sign handling at all)
//                                                                     -> this port already did. KEPT.
//   * Signed N accepts '+'   (Utf8Parser.Integer.Signed.N.cs:24)      -> already right.
//   * Unsigned N accepts '+' (Utf8Parser.Integer.Unsigned.N.cs:18)    -> already right.
//
// So the "internal inconsistency" between unsigned D and unsigned N is .NET's OWN, and is
// reproduced deliberately rather than tidied away.
// ===========================================================================

TEST(Utf8ParserPlusSignPinTests, Fix2060_SignedDefaultAndDNowAcceptALeadingPlus) {
    const uint8_t text[] = {'+', '4', '2'};
    System::ReadOnlySpan<uint8_t> src(text, 3);
    for (char format : {'\0', 'G', 'D'}) {
        int32_t value = 0;
        intcs consumed = 0;
        ASSERT_TRUE(Utf8Parser::TryParse(src, value, consumed, format)) << "format " << (int)format;
        EXPECT_EQ(value, 42);
        EXPECT_EQ(consumed, 3) << "the '+' is consumed, just as a '-' is";
    }
}

TEST(Utf8ParserPlusSignPinTests, Fix2060_ASignWithNoDigitsIsStillNotAnInteger) {
    // Signed.D.cs:19-22 and :26-29 -- after consuming the sign, running out of input is a
    // FalseExit. "+" and "-" alone are not integers, and the '+' path must not be laxer than
    // the '-' path it was modelled on.
    for (const char* text : {"+", "-", "+x", "-x", "+ 1"}) {
        SCOPED_TRACE(text);
        System::ReadOnlySpan<uint8_t> src(reinterpret_cast<const uint8_t*>(text),
                                          static_cast<intcs>(std::strlen(text)));
        int32_t value = 99;
        intcs consumed = 99;
        EXPECT_FALSE(Utf8Parser::TryParse(src, value, consumed, 'D'));
        EXPECT_EQ(value, 0);       // CCF-014: both outputs normalized on failure
        EXPECT_EQ(consumed, 0);
    }
}

TEST(Utf8ParserPlusSignPinTests, Fix2060_UnsignedDefaultAndDStillRejectALeadingPlus_AndThatIsDotNets) {
    // NOT a defect, and NOT tidied. Utf8Parser.Integer.Unsigned.D.cs has no sign handling at
    // all -- it goes straight to ParserHelpers.IsDigit -- while Unsigned.N.cs:18 accepts '+'.
    // The asymmetry the finding called an inconsistency is .NET's own, and #2060's second
    // claim is therefore refuted rather than implemented.
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

// ---------------------------------------------------------------------------
// #2332 / SR-AUD-069 — SequencePosition's components are private
// ---------------------------------------------------------------------------

TEST(BuffersContractPinTests, Fix2332_TheComponentsArePrivateAndOnlyTheAccessorsRemain) {
    // .NET's SequencePosition is a readonly struct with private readonly fields, and documents
    // that its parts must not be interpreted by anything except the sequence that created it.
    // This port published both as MUTABLE data members, so a caller could rewrite a position
    // after the sequence handed it out -- to an unrelated segment, a dangling pointer, or an
    // offset the owner never produced.
    //
    // The compile-domain half is pinned by test/consumer/core_sequenceposition_private_negative
    // .cpp, which is where a rejection can actually be asserted. This row pins the observable
    // consequence: the type is no longer an aggregate, so the two spellings that depend on that
    // -- structured bindings and designated initialisers -- are gone.
    static_assert(!std::is_aggregate_v<System::SequencePosition>,
                  "#2332: private components mean this is not an aggregate");

    // WHAT DID NOT CHANGE, and had to not change: construction, reading, copying and comparison.
    // Brace initialisation with two arguments still compiles -- through the constructor rather
    // than as an aggregate -- which is what keeps every existing call site working.
    int segment = 0;
    const System::SequencePosition braced{&segment, 7};
    EXPECT_EQ(&segment, braced.GetObject());
    EXPECT_EQ(7, braced.GetInteger());

    const System::SequencePosition defaulted{};
    EXPECT_EQ(nullptr, defaulted.GetObject());
    EXPECT_EQ(0, defaulted.GetInteger());

    const System::SequencePosition copied = braced;
    EXPECT_TRUE(copied == braced);
    EXPECT_TRUE(copied.Equals(braced));
    static_assert(std::is_trivially_copyable_v<System::SequencePosition>,
                  "#2332 must not have cost trivial copyability");

    // The layout is untouched: making members private changes access, not storage.
    static_assert(sizeof(System::SequencePosition) == sizeof(void*) + sizeof(SharpRuntime::intcs) +
                                                      (alignof(void*) - sizeof(SharpRuntime::intcs)),
                  "#2332 is an access change, not a layout change");
}
