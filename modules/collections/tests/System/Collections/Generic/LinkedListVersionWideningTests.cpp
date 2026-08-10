// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Permanent regression suite for ticket #1788 (REMED-COLL-LINKEDLIST-VERSION-WIDEN).
//
// WHAT WAS WRONG. Ticket #1787 repaired every collection mutation counter it could without
// changing a public object layout. LinkedList<T> was one of two types where it could not go
// all the way: its members are exactly packed on LP64 -- head_ (shared_ptr) @0 (16), tail_
// (weak_ptr) @16 (16), count_ (intcs) @32 (4), version_ @36 (4) -- so sizeof was 40 with ZERO
// padding and an 8-byte counter makes it 48 in ANY member order. #1787 therefore gave it the
// 32-bit NarrowMutationCounter. That removed the signed-overflow undefined behaviour and it
// never had the assignment-transplant defect (#1769 had already given it a bumping
// operator=), but it left the 2^32 enumerator-snapshot ABA: after 2^32 effective mutations
// the counter returns to a value an outstanding Enumerator captured, and the equality guard
// silently accepts it. Reproduced before this ticket changed anything, in
// build-probe/1788_prefix_defects.log:
//
//     LinkedList<int>          snapshot=3 counter-2^32-later=3 guard-fired=0
//     LinkedList<std::string>  snapshot=2 counter-2^32-later=2 guard-fired=0
//     LinkedList<int> Reset()   guard-fired=0
//     defects-observed=3
//
// Because LinkedList<T>::Enumerator holds a raw data_t* into shared_ptr-owned node storage, a
// false positive there is a potential USE-AFTER-FREE, not merely a wrong answer. At around
// 10^8 increments per second the horizon is roughly 43 seconds of hot mutation, not centuries.
//
// WHAT CHANGED. Under explicit user approval for the object-layout consequence, version_ is
// now detail::MutationCounter (64-bit unsigned) and Enumerator's snapshot is
// detail::MutationVersion. Measured: sizeof(LinkedList<int>) 40 -> 48, alignof unchanged at 8,
// head_/tail_/count_ offsets unchanged, sizeof(Enumerator) UNCHANGED at 40 (the wider snapshot
// lands in padding it already had), 0 LinkedList symbols renamed, added or removed. The
// same probe post-fix reads defects-observed=0. Full record: docs/CollectionVersionCounterSweep.md
// section 8.1 and its ticket #1788 closure section.
//
// WHAT THIS SUITE PINS. Everything the widening could plausibly have broken, plus the
// boundary behaviour that could not be reached before. Reaching 2^32 real mutations is
// forbidden and unnecessary: near-boundary cases POSITION the counter through the one
// authoritative test-only seam (ticket #1800), which is exactly what that many real mutations
// would do. No test below performs more than a few dozen real mutations.
//
// The complementary assertion lives in CollectionVersionCounterTests.cpp, where
// LinkedListAdapter::kNarrowCounter flipped true -> false; that automatically converts #1787's
// deliberately pinned 2^32-residual assertion into the wide-family no-revalidation assertion,
// which is the "flip it on purpose" this ticket was required to perform.

#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/Collections/Generic/LinkedList.hpp"
#include "System/Collections/detail/MutationCounter.hpp"
#include "System/InvalidOperationException.hpp"

// The ONE authoritative definition of the seam (ticket #1800). Never write
// `template<> struct CollectionVersionAccess<...>` in a test translation unit.
#include "../../../support/CollectionVersionSeam.hpp"

using SharpRuntime::intcs;
using SharpRuntime::uintcs;
using SharpRuntime::ulongcs;

namespace G = System::Collections::Generic;

namespace {

template <typename T>
using Seam = SharpRuntime::Testing::CollectionVersionAccess<G::LinkedList<T>>;

template <typename T>
typename Seam<T>::value_type versionOf(const G::LinkedList<T>& l) {
    return Seam<T>::version(l);
}

template <typename T>
void positionVersion(G::LinkedList<T>& l, typename Seam<T>::value_type v) {
    Seam<T>::positionVersion(l, v);
}

// The three values around the horizon that used to matter, plus the step that used to alias.
constexpr ulongcs kU32Max = 4294967295ULL;         // UINT32_MAX
constexpr ulongcs kAliasStep = 4294967296ULL;      // 2^32 -- one full lap of the old counter
constexpr ulongcs kOldInt32Max = 2147483647ULL;    // where the pre-#1787 increment was UB

/** An owning enumerator handle, so no test leaks one when an assertion throws. */
template <typename T>
class Enumerating {
    G::IEnumerator<T>* e_;

public:
    explicit Enumerating(G::LinkedList<T>& l) : e_(l.GetEnumerator()) {}
    Enumerating(const Enumerating&) = delete;
    Enumerating& operator=(const Enumerating&) = delete;
    ~Enumerating() { delete e_; }
    G::IEnumerator<T>* operator->() const { return e_; }
    G::IEnumerator<T>* get() const { return e_; }
};

G::LinkedList<int> three() {
    G::LinkedList<int> l;
    l.AddLast(1);
    l.AddLast(2);
    l.AddLast(3);
    return l;
}

/** A non-trivial element type that counts its own live instances. */
struct Counted {
    static int alive;
    int value = 0;

    Counted() { ++alive; }
    explicit Counted(int v) : value(v) { ++alive; }
    Counted(const Counted& o) : value(o.value) { ++alive; }
    Counted& operator=(const Counted&) = default;
    ~Counted() { --alive; }
    bool operator==(const Counted& o) const { return value == o.value; }
};
int Counted::alive = 0;

/** A move-only element type; LinkedList<T> reaches it through the existing-node overloads. */
struct MoveOnly {
    std::unique_ptr<int> value;

    MoveOnly() = default;
    explicit MoveOnly(int v) : value(std::make_unique<int>(v)) {}
    MoveOnly(MoveOnly&&) = default;
    MoveOnly& operator=(MoveOnly&&) = default;
    MoveOnly(const MoveOnly&) = delete;
    MoveOnly& operator=(const MoveOnly&) = delete;
};

}  // namespace

// =====================================================================================
// The representation itself.
// =====================================================================================

TEST(LinkedListVersionWidening, TheCounterIsUnsignedAndSixtyFourBits) {
    using Value = typename Seam<int>::value_type;
    static_assert(std::is_unsigned_v<Value>,
                  "an unsigned counter is what makes the increment defined at its maximum");
    static_assert(std::is_same_v<Value, ulongcs>,
                  "ticket #1788 moved LinkedList<T> off the 32-bit NarrowMutationCounter");
    EXPECT_EQ(sizeof(Value), sizeof(ulongcs));
    EXPECT_GT(sizeof(Value), sizeof(uintcs));
    EXPECT_EQ(static_cast<ulongcs>(std::numeric_limits<Value>::max()),
              std::numeric_limits<ulongcs>::max());
}

TEST(LinkedListVersionWidening, TheCounterIsWideForEveryElementType) {
    static_assert(std::is_same_v<typename Seam<int>::value_type, ulongcs>);
    static_assert(std::is_same_v<typename Seam<std::string>::value_type, ulongcs>);
    static_assert(std::is_same_v<typename Seam<Counted>::value_type, ulongcs>);
    static_assert(std::is_same_v<typename Seam<MoveOnly>::value_type, ulongcs>);
    SUCCEED();
}

TEST(LinkedListVersionWidening, TheObjectGrewToFortyEightBytesOnLp64) {
    // The approved consequence, pinned so it cannot drift back or drift further unnoticed.
    // It was 40 before ticket #1788, with ZERO padding, which is precisely why the widening
    // needed approval at all.
    if constexpr (sizeof(void*) == 8) {
        EXPECT_EQ(sizeof(G::LinkedList<int>), 48u);
        EXPECT_EQ(sizeof(G::LinkedList<std::string>), 48u);
        EXPECT_EQ(sizeof(G::LinkedList<double>), 48u);
        EXPECT_EQ(alignof(G::LinkedList<int>), 8u);
        // The node handle and the STL iterators did NOT grow: neither carries a snapshot.
        EXPECT_EQ(sizeof(G::LinkedListNode<int>), 16u);
        EXPECT_EQ(sizeof(G::LinkedList<int>::iterator), 16u);
        EXPECT_EQ(sizeof(G::LinkedList<int>::const_iterator), 16u);
    } else {
        SUCCEED() << "the published figures are LP64-only";
    }
}

TEST(LinkedListVersionWidening, NoCounterOrCounterTypeLeaksThroughAPublicSurface) {
    // The counter is private and stays private: no public signature mentions it, and
    // getCountProperty() still returns a plain intcs by value.
    static_assert(std::is_same_v<
        decltype(std::declval<const G::LinkedList<int>&>().getCountProperty()), intcs>);
    static_assert(std::is_same_v<
        decltype(std::declval<G::LinkedList<int>&>().GetEnumerator()), G::IEnumerator<int>*>);
    static_assert(std::is_same_v<
        decltype(std::declval<G::LinkedList<int>&>().AddLast(std::declval<const int&>())),
        G::LinkedListNode<int>>);
    static_assert(std::is_same_v<
        decltype(std::declval<G::LinkedList<int>&>().Remove(std::declval<const int&>())), bool>);
    SUCCEED();
}

TEST(LinkedListVersionWidening, ValueSemanticsTraitsAreUnchanged) {
    using L = G::LinkedList<int>;
    static_assert(std::is_copy_constructible_v<L>);
    static_assert(std::is_copy_assignable_v<L>);
    static_assert(std::is_move_constructible_v<L>);
    static_assert(std::is_move_assignable_v<L>);
    static_assert(std::is_nothrow_move_constructible_v<L>);
    static_assert(!std::is_polymorphic_v<L>);
    static_assert(!std::is_trivially_copyable_v<L>);
    SUCCEED();
}

TEST(LinkedListVersionWidening, AFreshListStartsCountingFromZero) {
    G::LinkedList<int> l;
    EXPECT_EQ(versionOf(l), 0u);
    EXPECT_EQ(l.getCountProperty(), 0);
}

// =====================================================================================
// The defect this ticket closes: no low-32-bit ABA.
// =====================================================================================

TEST(LinkedListVersionWidening, NoStaleEnumeratorIsRevalidatedAcrossTheOld2Pow32Distance) {
    // Pre-fix this was guard-fired=0 at every multiple: the 32-bit field genuinely returned
    // to the snapshot. The counter now holds the distance instead of aliasing across it.
    for (const ulongcs multiple : {1ULL, 2ULL, 7ULL, 1000ULL}) {
        auto l = three();
        Enumerating<int> e(l);
        ASSERT_TRUE(e->MoveNext());
        const ulongcs snapshot = versionOf(l);
        positionVersion(l, snapshot + multiple * kAliasStep);
        EXPECT_THROW(e->MoveNext(), System::InvalidOperationException) << "multiple=" << multiple;
    }
}

TEST(LinkedListVersionWidening, ResetAlsoFailsFastAcrossTheOld2Pow32Distance) {
    auto l = three();
    Enumerating<int> e(l);
    ASSERT_TRUE(e->MoveNext());
    const ulongcs snapshot = versionOf(l);
    positionVersion(l, snapshot + kAliasStep);
    EXPECT_THROW(e->Reset(), System::InvalidOperationException);
}

TEST(LinkedListVersionWidening, ASnapshotSharingOnlyItsLowThirtyTwoBitsIsStillRejected) {
    // The sharpest form of the old defect: the live counter and the snapshot agree in every
    // one of the low 32 bits and differ only above them. A truncating comparison -- which is
    // what widening the container WITHOUT widening the snapshot would have produced -- accepts
    // this. An exact 64-bit comparison rejects it.
    auto l = three();
    Enumerating<int> e(l);
    ASSERT_TRUE(e->MoveNext());
    const ulongcs snapshot = versionOf(l);
    ASSERT_LT(snapshot, kAliasStep);

    for (const ulongcs high : {1ULL, 2ULL, 0xDEADBEEFULL}) {
        const ulongcs aliased = snapshot + high * kAliasStep;
        EXPECT_EQ(aliased & 0xFFFFFFFFULL, snapshot) << "the low 32 bits must match by design";
        EXPECT_NE(aliased, snapshot);
        positionVersion(l, aliased);
        EXPECT_THROW(e->MoveNext(), System::InvalidOperationException) << "high=" << high;
    }
}

TEST(LinkedListVersionWidening, TheHorizonIsNowTwoToTheSixtyFourNotTwoToTheThirtyTwo) {
    // A full lap of the OLD counter no longer returns to the snapshot; a full lap of the NEW
    // one does, and is still honestly reachable only by positioning.
    auto l = three();
    Enumerating<int> e(l);
    ASSERT_TRUE(e->MoveNext());
    const ulongcs snapshot = versionOf(l);

    positionVersion(l, snapshot + kAliasStep);
    EXPECT_THROW(e->MoveNext(), System::InvalidOperationException);

    // 2^64 laps back to the same value, which is modular arithmetic, not a promise of
    // infinity. Documented as a residual rather than claimed impossible.
    positionVersion(l, snapshot);
    EXPECT_NO_THROW(e->MoveNext());
}

// =====================================================================================
// Near-boundary behaviour: the exact positions the ticket named.
// =====================================================================================

TEST(LinkedListVersionWidening, MutationAtEachNamedBoundaryValueMovesForwardExactly) {
    const ulongcs starts[] = {0ULL,
                              1ULL,
                              kOldInt32Max - 1,
                              kOldInt32Max,
                              kOldInt32Max + 1,
                              kU32Max - 1,
                              kU32Max,
                              kU32Max + 1,
                              kAliasStep + 5,
                              std::numeric_limits<ulongcs>::max() - 1};
    for (const ulongcs start : starts) {
        auto l = three();
        positionVersion(l, start);
        ASSERT_NO_THROW(l.AddLast(9)) << "start=" << start;
        EXPECT_EQ(versionOf(l), start + 1) << "start=" << start;
    }
}

TEST(LinkedListVersionWidening, TheStepThatUsedToWrapNowJustAdvances) {
    // UINT32_MAX -> UINT32_MAX + 1 was the wrap to zero. It is now an ordinary increment,
    // and it is exactly here that the old counter lost the ability to distinguish snapshots.
    auto l = three();
    positionVersion(l, kU32Max);
    Enumerating<int> e(l);
    ASSERT_TRUE(e->MoveNext());
    l.AddLast(4);
    EXPECT_EQ(versionOf(l), kAliasStep);
    EXPECT_NE(versionOf(l), 0u) << "the 32-bit counter wrapped to 0 here";
    EXPECT_THROW(e->MoveNext(), System::InvalidOperationException);
}

TEST(LinkedListVersionWidening, AnEnumeratorTakenBeforeTheTransitionIsInvalidatedByIt) {
    auto l = three();
    positionVersion(l, kU32Max - 1);
    Enumerating<int> e(l);
    ASSERT_TRUE(e->MoveNext());

    l.AddLast(4);                       // reaches UINT32_MAX
    EXPECT_EQ(versionOf(l), kU32Max);
    EXPECT_THROW(e->MoveNext(), System::InvalidOperationException);

    Enumerating<int> e2(l);
    ASSERT_TRUE(e2->MoveNext());
    l.AddLast(5);                       // crosses the old 32-bit boundary
    EXPECT_EQ(versionOf(l), kAliasStep);
    EXPECT_THROW(e2->MoveNext(), System::InvalidOperationException);

    // And a fresh enumerator taken on the far side works normally.
    Enumerating<int> e3(l);
    int seen = 0;
    while (e3->MoveNext()) ++seen;
    EXPECT_EQ(seen, 5);
}

TEST(LinkedListVersionWidening, MutationPastTheOldBoundaryStaysExactAndKeepsInvalidating) {
    auto l = three();
    positionVersion(l, kAliasStep);
    for (int i = 0; i < 25; ++i) {
        Enumerating<int> e(l);
        ASSERT_TRUE(e->MoveNext());
        l.AddLast(100 + i);
        EXPECT_EQ(versionOf(l), kAliasStep + static_cast<ulongcs>(i) + 1) << "step " << i;
        EXPECT_THROW(e->MoveNext(), System::InvalidOperationException) << "step " << i;
    }
}

TEST(LinkedListVersionWidening, ExhaustionWrapsToZeroWithoutUndefinedBehaviour) {
    // Unsigned arithmetic is modulo 2^N by definition, so the increment is defined at the
    // maximum. Under UBSan this is the case that would report if the type were ever signed.
    auto l = three();
    positionVersion(l, std::numeric_limits<ulongcs>::max());
    Enumerating<int> e(l);
    ASSERT_TRUE(e->MoveNext());
    ASSERT_NO_THROW(l.AddLast(4));
    EXPECT_EQ(versionOf(l), 0u);
    EXPECT_THROW(e->MoveNext(), System::InvalidOperationException);
}

TEST(LinkedListVersionWidening, NoCounterValueIsReservedAsASentinel) {
    const ulongcs probes[] = {0ULL, 1ULL, kOldInt32Max, kU32Max, kAliasStep,
                              std::numeric_limits<ulongcs>::max()};
    for (const ulongcs v : probes) {
        auto l = three();
        positionVersion(l, v);
        EXPECT_EQ(l.getCountProperty(), 3) << "counter positioned at " << v;
        int seen = 0;
        Enumerating<int> e(l);
        while (e->MoveNext()) ++seen;
        EXPECT_EQ(seen, 3) << "counter positioned at " << v;
        EXPECT_TRUE(l.Contains(2));
        EXPECT_TRUE(static_cast<bool>(l.Find(2)));
    }
}

// =====================================================================================
// The mutation-version delta matrix: exactly what does and does not advance the counter.
// The widening must not change one entry of this table.
// =====================================================================================

namespace {

template <typename Op>
ulongcs deltaOf(Op op) {
    auto l = three();
    const ulongcs before = versionOf(l);
    op(l);
    return versionOf(l) - before;
}

}  // namespace

TEST(LinkedListVersionWidening, EveryEffectiveMutationAdvancesTheCounterByExactlyOne) {
    EXPECT_EQ(deltaOf([](auto& l) { l.AddFirst(9); }), 1u);
    EXPECT_EQ(deltaOf([](auto& l) { l.AddFirst(G::LinkedListNode<int>(9)); }), 1u);
    EXPECT_EQ(deltaOf([](auto& l) { l.AddLast(9); }), 1u);
    EXPECT_EQ(deltaOf([](auto& l) { l.AddLast(G::LinkedListNode<int>(9)); }), 1u);
    EXPECT_EQ(deltaOf([](auto& l) { l.AddBefore(l.getFirstProperty(), 9); }), 1u);
    EXPECT_EQ(deltaOf([](auto& l) {
        l.AddBefore(l.getFirstProperty(), G::LinkedListNode<int>(9)); }), 1u);
    EXPECT_EQ(deltaOf([](auto& l) { l.AddAfter(l.getFirstProperty(), 9); }), 1u);
    EXPECT_EQ(deltaOf([](auto& l) {
        l.AddAfter(l.getFirstProperty(), G::LinkedListNode<int>(9)); }), 1u);
    EXPECT_EQ(deltaOf([](auto& l) { l.AddAfter(l.getLastProperty(), 9); }), 1u);
    EXPECT_EQ(deltaOf([](auto& l) { EXPECT_TRUE(l.Remove(2)); }), 1u);
    EXPECT_EQ(deltaOf([](auto& l) { l.Remove(l.getFirstProperty()); }), 1u);
    EXPECT_EQ(deltaOf([](auto& l) { l.RemoveFirst(); }), 1u);
    EXPECT_EQ(deltaOf([](auto& l) { l.RemoveLast(); }), 1u);
    EXPECT_EQ(deltaOf([](auto& l) { l.Clear(); }), 1u);
}

TEST(LinkedListVersionWidening, EveryRejectedOrReadOnlyOperationAdvancesNothing) {
    EXPECT_EQ(deltaOf([](auto& l) { EXPECT_FALSE(l.Remove(987654)); }), 0u);
    EXPECT_EQ(deltaOf([](auto& l) { EXPECT_TRUE(l.Contains(2)); }), 0u);
    EXPECT_EQ(deltaOf([](auto& l) { EXPECT_TRUE(static_cast<bool>(l.Find(2))); }), 0u);
    EXPECT_EQ(deltaOf([](auto& l) { EXPECT_TRUE(static_cast<bool>(l.FindLast(2))); }), 0u);
    EXPECT_EQ(deltaOf([](auto& l) { EXPECT_EQ(l.getCountProperty(), 3); }), 0u);
    EXPECT_EQ(deltaOf([](auto& l) { int s = 0; for (int v : l) s += v; EXPECT_EQ(s, 6); }), 0u);
    EXPECT_EQ(deltaOf([](auto& l) {
        std::vector<int> d(3);
        l.CopyTo(d, 0);
        EXPECT_EQ(d[0], 1);
    }), 0u);

    // A node VALUE write is not a structural mutation and does not advance the counter. That
    // is the pre-existing documented contract; ticket #1788 changed the counter's width only
    // and deliberately did not revisit the mutation/no-op policy.
    EXPECT_EQ(deltaOf([](auto& l) { l.getFirstProperty().setValueProperty(42); }), 0u);

    // Rejected structural attempts move nothing either.
    EXPECT_EQ(deltaOf([](auto& l) {
        EXPECT_THROW(l.AddLast(l.getFirstProperty()), System::InvalidOperationException);
    }), 0u);
    EXPECT_EQ(deltaOf([](auto& l) {
        EXPECT_THROW(l.AddLast(G::LinkedListNode<int>()), System::ArgumentNullException);
    }), 0u);
    EXPECT_EQ(deltaOf([](auto& l) {
        G::LinkedList<int> other;
        other.AddLast(5);
        EXPECT_THROW(l.Remove(other.getFirstProperty()), System::InvalidOperationException);
    }), 0u);
}

TEST(LinkedListVersionWidening, AThrowingRemoveOnAnEmptyListAdvancesNothing) {
    G::LinkedList<int> l;
    const ulongcs before = versionOf(l);
    EXPECT_THROW(l.RemoveFirst(), System::InvalidOperationException);
    EXPECT_EQ(versionOf(l), before);
    EXPECT_THROW(l.RemoveLast(), System::InvalidOperationException);
    EXPECT_EQ(versionOf(l), before);
}

TEST(LinkedListVersionWidening, ClearOnAnAlreadyEmptyListStillAdvancesUnconditionally) {
    // The current contract, unchanged by this ticket and pinned so the widening is visibly
    // not the thing that decided it. It differs from the "advance on effective mutation" rule
    // the other Remove paths follow, and is left exactly as it was.
    G::LinkedList<int> l;
    const ulongcs before = versionOf(l);
    l.Clear();
    EXPECT_EQ(versionOf(l), before + 1);
    l.Clear();
    EXPECT_EQ(versionOf(l), before + 2);
}

TEST(LinkedListVersionWidening, DetachAndReattachIsTwoEffectiveMutations) {
    auto l = three();
    const ulongcs before = versionOf(l);
    G::LinkedListNode<int> n = l.getFirstProperty();
    l.Remove(n);
    l.AddLast(n);
    EXPECT_EQ(versionOf(l), before + 2);
    EXPECT_EQ(l.getCountProperty(), 3);
    EXPECT_EQ(l.getLastProperty().getValueProperty(), 1);
}

// =====================================================================================
// Assignment, copy and move -- ticket #1769's guarded operators, at the boundary.
// =====================================================================================

TEST(LinkedListVersionWidening, CopyAssignmentAdvancesTheDestinationAcrossTheBoundary) {
    auto a = three();
    G::LinkedList<int> b;
    b.AddLast(7);
    positionVersion(a, kU32Max);
    Enumerating<int> e(a);
    ASSERT_TRUE(e->MoveNext());
    a = b;
    EXPECT_GT(versionOf(a), kU32Max);
    EXPECT_THROW(e->MoveNext(), System::InvalidOperationException);
    EXPECT_EQ(a.getCountProperty(), 1);
}

TEST(LinkedListVersionWidening, MoveAssignmentAdvancesTheDestinationAcrossTheBoundary) {
    auto a = three();
    G::LinkedList<int> b;
    b.AddLast(7);
    positionVersion(a, kU32Max);
    Enumerating<int> e(a);
    ASSERT_TRUE(e->MoveNext());
    a = std::move(b);
    EXPECT_GT(versionOf(a), kU32Max);
    EXPECT_THROW(e->MoveNext(), System::InvalidOperationException);
    EXPECT_EQ(a.getCountProperty(), 1);
}

TEST(LinkedListVersionWidening, AssignmentWithAMatchingSourceCounterStillInvalidates) {
    auto a = three();
    G::LinkedList<int> b;
    b.AddLast(7);
    Enumerating<int> e(a);
    ASSERT_TRUE(e->MoveNext());
    const ulongcs snapshot = versionOf(a);
    positionVersion(b, snapshot);
    a = b;
    EXPECT_NE(versionOf(a), snapshot);
    EXPECT_THROW(e->MoveNext(), System::InvalidOperationException);
}

TEST(LinkedListVersionWidening, SelfAssignmentIsStillANoOp) {
    // LinkedList<T> declares its own operator= with an `if (this != &other)` guard, so
    // self-assignment destroys nothing and must invalidate nothing. It is the ONE collection
    // in this module where that holds; ticket #1788 does not disturb it.
    auto a = three();
    positionVersion(a, kAliasStep + 11);
    Enumerating<int> e(a);
    ASSERT_TRUE(e->MoveNext());

    a = a;  // NOLINT(clang-diagnostic-self-assign-overloaded)
    EXPECT_EQ(versionOf(a), kAliasStep + 11);
    EXPECT_NO_THROW(e->MoveNext());
    EXPECT_EQ(a.getCountProperty(), 3);
}

TEST(LinkedListVersionWidening, SelfMoveAssignmentIsStillANoOp) {
    auto a = three();
    positionVersion(a, kAliasStep + 11);
    Enumerating<int> e(a);
    ASSERT_TRUE(e->MoveNext());

    G::LinkedList<int>& alias = a;
    a = std::move(alias);  // NOLINT -- the degenerate case, guarded by #1769's operator=
    EXPECT_EQ(versionOf(a), kAliasStep + 11);
    EXPECT_NO_THROW(e->MoveNext());
    EXPECT_EQ(a.getCountProperty(), 3);
}

TEST(LinkedListVersionWidening, AnIndependentCopyDoesNotInvalidateTheOriginalsEnumerator) {
    auto a = three();
    positionVersion(a, kAliasStep + 3);
    Enumerating<int> e(a);
    ASSERT_TRUE(e->MoveNext());

    G::LinkedList<int> copy = a;
    EXPECT_EQ(versionOf(a), kAliasStep + 3) << "copy construction must not touch the source";
    EXPECT_NO_THROW(e->MoveNext());

    // Mutating the copy is likewise invisible to the source's enumerator: the two lists own
    // separate node chains and separate counters.
    copy.AddLast(4);
    EXPECT_EQ(versionOf(a), kAliasStep + 3);
    EXPECT_NO_THROW(e->MoveNext());
    EXPECT_EQ(a.getCountProperty(), 3);
    EXPECT_EQ(copy.getCountProperty(), 4);

    // And the copy's own enumerator is valid from the start.
    Enumerating<int> ec(copy);
    EXPECT_TRUE(ec->MoveNext());
}

TEST(LinkedListVersionWidening, AssignmentDoesNotDisturbTheSourcesOwnEnumerators) {
    auto a = three();
    G::LinkedList<int> b;
    b.AddLast(7);
    b.AddLast(8);
    positionVersion(b, kAliasStep);
    Enumerating<int> eb(b);
    ASSERT_TRUE(eb->MoveNext());
    a = b;
    EXPECT_EQ(versionOf(b), kAliasStep);
    EXPECT_NO_THROW(eb->MoveNext());
}

TEST(LinkedListVersionWidening, MoveConstructionFollowsTheEstablishedOwnershipContract) {
    // #1769's contract: the chain transfers, every node's owner is repointed at the
    // destination, the source is left empty, and the source's own counter advances -- which
    // is what invalidates any enumerator outstanding over the emptied source.
    auto a = three();
    positionVersion(a, kU32Max);
    G::LinkedListNode<int> retained = a.getFirstProperty();
    Enumerating<int> ea(a);
    ASSERT_TRUE(ea->MoveNext());

    G::LinkedList<int> moved = std::move(a);

    EXPECT_EQ(moved.getCountProperty(), 3);
    EXPECT_EQ(a.getCountProperty(), 0);
    EXPECT_EQ(retained.getListProperty(), &moved) << "#1769 repoints every node's owner";
    EXPECT_EQ(retained.getValueProperty(), 1);
    EXPECT_GT(versionOf(a), kU32Max) << "the emptied source must invalidate its enumerators";
    EXPECT_THROW(ea->MoveNext(), System::InvalidOperationException);
    EXPECT_EQ(versionOf(moved), 0u) << "a fresh object starts its own count";
}

// =====================================================================================
// Node lifetime (#1768 / #1769) is untouched by the widening -- verified at the boundary.
// =====================================================================================

TEST(LinkedListVersionWidening, RemovalThroughACopiedHandleInvalidatesIterationAtTheBoundary) {
    auto l = three();
    positionVersion(l, kU32Max - 1);
    G::LinkedListNode<int> first = l.getFirstProperty();
    G::LinkedListNode<int> alias = first;      // a second handle to the SAME node
    Enumerating<int> e(l);
    ASSERT_TRUE(e->MoveNext());

    l.Remove(alias);                           // removal through the copy

    EXPECT_EQ(versionOf(l), kU32Max);
    EXPECT_THROW(e->MoveNext(), System::InvalidOperationException);
    // Both handles observe one detached node that kept its value.
    EXPECT_EQ(first.getListProperty(), nullptr);
    EXPECT_EQ(alias.getListProperty(), nullptr);
    EXPECT_EQ(first.getValueProperty(), 1);
    EXPECT_TRUE(first == alias);
}

TEST(LinkedListVersionWidening, ReattachmentInvalidatesIterationAtTheBoundary) {
    auto l = three();
    G::LinkedListNode<int> detached(99);
    positionVersion(l, kU32Max);
    Enumerating<int> e(l);
    ASSERT_TRUE(e->MoveNext());

    l.AddLast(detached);

    EXPECT_EQ(versionOf(l), kAliasStep);
    EXPECT_THROW(e->MoveNext(), System::InvalidOperationException);
    EXPECT_EQ(detached.getListProperty(), &l);
    EXPECT_EQ(l.getCountProperty(), 4);
}

TEST(LinkedListVersionWidening, ClearAtTheBoundaryDetachesEveryRetainedHandle) {
    auto l = three();
    G::LinkedListNode<int> a = l.getFirstProperty();
    G::LinkedListNode<int> c = l.getLastProperty();
    positionVersion(l, kAliasStep - 1);
    Enumerating<int> e(l);
    ASSERT_TRUE(e->MoveNext());

    l.Clear();

    EXPECT_EQ(versionOf(l), kAliasStep);
    EXPECT_THROW(e->MoveNext(), System::InvalidOperationException);
    EXPECT_EQ(a.getListProperty(), nullptr);
    EXPECT_EQ(c.getListProperty(), nullptr);
    EXPECT_EQ(a.getValueProperty(), 1);
    EXPECT_EQ(c.getValueProperty(), 3);
    EXPECT_EQ(l.getCountProperty(), 0);
}

TEST(LinkedListVersionWidening, OwnerDestructionWithRetainedNodesIsStillSafeAtTheBoundary) {
    G::LinkedListNode<Counted> retained;
    const int aliveBefore = Counted::alive;
    {
        G::LinkedList<Counted> owner;
        owner.AddLast(Counted(1));
        owner.AddLast(Counted(2));
        positionVersion(owner, std::numeric_limits<ulongcs>::max() - 1);
        retained = owner.getFirstProperty();
        EXPECT_EQ(retained.getListProperty(), &owner);
        owner.AddLast(Counted(3));                    // reaches the maximum
        EXPECT_EQ(versionOf(owner), std::numeric_limits<ulongcs>::max());
    }
    // The node outlived its owner, kept its value, and reports no list.
    ASSERT_TRUE(static_cast<bool>(retained));
    EXPECT_EQ(retained.getListProperty(), nullptr);
    EXPECT_EQ(retained.getValueProperty().value, 1);
    EXPECT_FALSE(static_cast<bool>(retained.getNextProperty()));
    retained = G::LinkedListNode<Counted>();
    EXPECT_EQ(Counted::alive, aliveBefore) << "no node leaked and none was freed twice";
}

TEST(LinkedListVersionWidening, ADetachedNodeCanRejoinAnotherListAtTheBoundary) {
    auto donor = three();
    G::LinkedListNode<int> node = donor.getFirstProperty();
    donor.Remove(node);

    G::LinkedList<int> receiver;
    positionVersion(receiver, kAliasStep + 41);
    Enumerating<int> e(receiver);
    receiver.AddFirst(node);

    EXPECT_EQ(versionOf(receiver), kAliasStep + 42);
    EXPECT_THROW(e->MoveNext(), System::InvalidOperationException);
    EXPECT_EQ(node.getListProperty(), &receiver);
    EXPECT_EQ(receiver.getCountProperty(), 1);
    EXPECT_EQ(donor.getCountProperty(), 2);
}

TEST(LinkedListVersionWidening, DuplicateAttachmentAndCrossListInsertionAreStillRejected) {
    auto l = three();
    G::LinkedList<int> other;
    other.AddLast(7);
    positionVersion(l, kAliasStep);
    const ulongcs before = versionOf(l);

    EXPECT_THROW(l.AddLast(l.getFirstProperty()), System::InvalidOperationException);
    EXPECT_THROW(l.AddBefore(other.getFirstProperty(), 5), System::InvalidOperationException);
    EXPECT_THROW(l.AddAfter(other.getFirstProperty(), 5), System::InvalidOperationException);
    EXPECT_THROW(l.Remove(other.getFirstProperty()), System::InvalidOperationException);
    EXPECT_THROW(l.AddLast(G::LinkedListNode<int>()), System::ArgumentNullException);

    EXPECT_EQ(versionOf(l), before) << "not one rejected attempt may advance the counter";
    EXPECT_EQ(l.getCountProperty(), 3);
}

TEST(LinkedListVersionWidening, ALargeTeardownAtTheBoundaryFreesEveryNode) {
    // Bounded stand-in for #1769's 200,000-node teardown, positioned at the boundary so the
    // widened counter is exercised on the destructor path too. The teardown must stay
    // iterative: a recursive shared_ptr chain release would overflow the stack here.
    const int aliveBefore = Counted::alive;
    {
        G::LinkedList<Counted> l;
        positionVersion(l, std::numeric_limits<ulongcs>::max() - 10);
        for (int i = 0; i < 20000; ++i) l.AddLast(Counted(i));
        EXPECT_EQ(l.getCountProperty(), 20000);
        EXPECT_EQ(Counted::alive, aliveBefore + 20000);
        // The counter wrapped through zero during that loop, with defined arithmetic.
        EXPECT_LT(versionOf(l), 20000u);
    }
    EXPECT_EQ(Counted::alive, aliveBefore);
}

// =====================================================================================
// Element types.
// =====================================================================================

TEST(LinkedListVersionWidening, StringsInvalidateAcrossTheOldBoundary) {
    G::LinkedList<std::string> l;
    l.AddLast(std::string("alpha"));
    l.AddLast(std::string("beta"));
    positionVersion(l, kU32Max);
    Enumerating<std::string> e(l);
    ASSERT_TRUE(e->MoveNext());
    EXPECT_EQ(e->Current(), "alpha");

    l.AddLast(std::string("gamma"));
    EXPECT_EQ(versionOf(l), kAliasStep);
    EXPECT_THROW(e->MoveNext(), System::InvalidOperationException);

    // And the low-32-bit alias is rejected for a non-trivial element type too.
    Enumerating<std::string> e2(l);
    ASSERT_TRUE(e2->MoveNext());
    positionVersion(l, versionOf(l) + kAliasStep);
    EXPECT_THROW(e2->MoveNext(), System::InvalidOperationException);
}

TEST(LinkedListVersionWidening, ANonTriviallyCopyableElementTypeBehavesIdentically) {
    const int aliveBefore = Counted::alive;
    {
        G::LinkedList<Counted> l;
        l.AddLast(Counted(1));
        l.AddLast(Counted(2));
        positionVersion(l, kAliasStep - 1);
        Enumerating<Counted> e(l);
        ASSERT_TRUE(e->MoveNext());
        EXPECT_EQ(e->Current().value, 1);

        EXPECT_TRUE(l.Remove(Counted(2)));
        EXPECT_EQ(versionOf(l), kAliasStep);
        EXPECT_THROW(e->MoveNext(), System::InvalidOperationException);
    }
    EXPECT_EQ(Counted::alive, aliveBefore);
}

TEST(LinkedListVersionWidening, AMoveOnlyElementTypeReachesTheSameContract) {
    // LinkedList<T> supports a move-only T through the existing-node overloads, which take a
    // copyable HANDLE to a node the caller move-constructed. The value overloads need a copy
    // and are correctly not viable.
    G::LinkedList<MoveOnly> l;
    G::LinkedListNode<MoveOnly> first(MoveOnly(1));
    G::LinkedListNode<MoveOnly> second(MoveOnly(2));
    l.AddLast(first);
    l.AddLast(second);
    EXPECT_EQ(l.getCountProperty(), 2);

    positionVersion(l, kU32Max);
    Enumerating<MoveOnly> e(l);
    ASSERT_TRUE(e->MoveNext());
    EXPECT_EQ(*e->Current().value, 1);

    l.Remove(second);
    EXPECT_EQ(versionOf(l), kAliasStep);
    EXPECT_THROW(e->MoveNext(), System::InvalidOperationException);

    // The detached node kept its (move-only) value.
    EXPECT_EQ(second.getListProperty(), nullptr);
    EXPECT_EQ(*second.getValueProperty().value, 2);
}

// =====================================================================================
// The public STL iterators are deliberately NOT version-checked, before and after.
// =====================================================================================

TEST(LinkedListVersionWidening, ThePublicStlIteratorsStillCarryNoSnapshot) {
    // begin()/end() follow plain std::list-style invalidation rules and are documented that
    // way. They never grew a snapshot and were never fail-fast; only the GetEnumerator()
    // enumerator is. Pinned so nobody reads this ticket as having changed it.
    auto l = three();
    positionVersion(l, kAliasStep);
    auto it = l.begin();
    l.AddLast(4);
    EXPECT_NO_THROW((void)*it) << "the STL iterator is not version-checked, by design";
    EXPECT_EQ(*it, 1);

    std::vector<int> collected;
    for (int v : l) collected.push_back(v);
    EXPECT_EQ(collected.size(), 4u);

    auto last = l.end();
    --last;
    EXPECT_EQ(*last, 4);
}
