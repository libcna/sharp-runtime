// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Permanent regression suite for ticket #1789 (REMED-COLL-BITARRAY-VERSION-WIDEN).
//
// WHAT WAS WRONG. Ticket #1787 repaired every collection mutation counter it could without
// changing a public object layout. BitArray was one of two types where it could not go all
// the way -- and the harder of the two, because the constraint is in a PUBLIC nested class.
// BitArray::Enumerator was exactly packed on LP64: vptr @0 (8), arr_ @8 (8), version_ @16 (4),
// index_ @20 (4), current_ @24 (1), state_ @28 (4), sizeof 32. Nine bytes are needed after an
// eight-byte snapshot where eight are available, so no member order avoids sizeof 40. #1787
// therefore gave BitArray the 32-bit NarrowMutationCounter.
//
// That was never about undefined behaviour: BitArray is the ONE collection whose counter was
// already unsigned (std::uint32_t, diverging from .NET's signed int at BitArray.cs:44), so it
// never had the signed-overflow UB the other fourteen did -- #1787 confirmed that by
// observing no UBSan report for it where all fourteen others reported. What #1787 fixed here
// was the assignment transplant. What REMAINED was the 2^32 enumerator-snapshot ABA: after
// 2^32 effective mutations the counter returns to a value an outstanding Enumerator captured
// and the equality guard silently accepts it. Reproduced before this ticket changed anything,
// in build-probe/1789_prefix_defects.log:
//
//     counter-value-type-bytes=4
//     enumerator-snapshot-bytes=4
//     BitArray Set   snapshot=2 counter-2^32-later=2 truncated-onto-snapshot=1 guard-fired=0
//     BitArray Reset()     guard-fired=0
//     BitArray 7x2^32      snapshot=2 counter=2 low-words-match=1 guard-fired=0
//     defects-observed=3
//
// A false positive on BitArray is a wrong answer rather than a use-after-free -- the
// enumerator holds an index that is bounds-checked against the CURRENT length on every step.
// That is why this was P3 and not P1. It is still a silent breach of the fail-fast contract,
// reachable in roughly 43 seconds of hot mutation at ~10^8 increments per second.
//
// WHAT CHANGED. Under explicit user approval for the object-layout consequence, version_ is
// now detail::MutationCounter (64-bit unsigned) and Enumerator's snapshot is
// detail::MutationVersion. BOTH moved together, deliberately: widening the container alone
// would have made the guard a silent truncation and left the 2^32 alias in place while the
// code claimed otherwise. Measured: sizeof(BitArray::Enumerator) 32 -> 40, alignof unchanged
// at 8, arr_ still @8; sizeof(BitArray) UNCHANGED at 48 (the wider counter landed in the four
// bytes of tail padding the container already had), bits_ still @0; 0 BitArray symbols
// renamed, added or removed. The same probe post-fix reads defects-observed=0. Full record:
// docs/CollectionVersionCounterSweep.md section 20.
//
// WHAT THIS SUITE PINS. Everything the widening could plausibly have broken, plus the
// boundary behaviour that could not be reached before. Reaching 2^32 real mutations is
// forbidden and unnecessary: near-boundary cases POSITION the counter through the one
// authoritative test-only seam (ticket #1800), which is exactly what that many real mutations
// would do. No test below performs more than a few hundred real mutations.
//
// The complementary assertion lives in CollectionVersionCounterTests.cpp, where
// BitArrayAdapter::kNarrowCounter flipped true -> false; that automatically converts #1787's
// deliberately pinned 2^32-residual assertion into the wide-family no-revalidation assertion,
// which is the "flip it on purpose" this ticket was required to perform. BitArray was the
// LAST narrow adapter, so no collection in this repository retains a 2^32 horizon.

#include <any>
#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Collections/BitArray.hpp"
#include "System/Collections/detail/MutationCounter.hpp"
#include "System/InvalidOperationException.hpp"

// The ONE authoritative definition of the seam (ticket #1800). Never write
// `template<> struct CollectionVersionAccess<...>` in a test translation unit.
#include "../../support/CollectionVersionSeam.hpp"

using SharpRuntime::bytecs;
using SharpRuntime::intcs;
using SharpRuntime::uintcs;
using SharpRuntime::ulongcs;

namespace NG = System::Collections;

namespace {

using Seam = SharpRuntime::Testing::CollectionVersionAccess<NG::BitArray>;
using Value = typename Seam::value_type;

Value versionOf(const NG::BitArray& b) { return Seam::version(b); }
void positionVersion(NG::BitArray& b, Value v) { Seam::positionVersion(b, v); }

// The values around the horizon that used to matter, plus the step that used to alias.
constexpr ulongcs kU32Max = 4294967295ULL;       // UINT32_MAX
constexpr ulongcs kAliasStep = 4294967296ULL;    // 2^32 -- one full lap of the old counter
constexpr ulongcs kOldInt32Max = 2147483647ULL;  // where the pre-#1787 increment was UB for others

/** An owning enumerator handle, so no test leaks one when an assertion throws. */
class Enumerating {
    NG::IEnumerator* e_;

public:
    explicit Enumerating(NG::BitArray& b) : e_(b.GetEnumerator()) {}
    Enumerating(const Enumerating&) = delete;
    Enumerating& operator=(const Enumerating&) = delete;
    ~Enumerating() { delete e_; }
    NG::IEnumerator* operator->() const { return e_; }
    NG::IEnumerator* get() const { return e_; }
};

/** Eight bits with two set: the shape every delta case starts from. */
NG::BitArray eight() {
    NG::BitArray b(8);
    b.Set(1, true);
    b.Set(4, true);
    return b;
}

/** The bit pattern, read through the public API, so a mutation's EFFECT can be compared. */
std::vector<bool> patternOf(const NG::BitArray& b) {
    std::vector<bool> out;
    for (intcs i = 0; i < b.getLengthProperty(); ++i) out.push_back(b.Get(i));
    return out;
}

/** Walks an enumerator to exhaustion and returns what it saw. */
std::vector<bool> walk(NG::BitArray& b) {
    std::vector<bool> seen;
    Enumerating e(b);
    while (e->MoveNext()) seen.push_back(std::any_cast<bool>(e->getCurrentProperty()));
    return seen;
}

}  // namespace

// =====================================================================================
// The representation itself.
// =====================================================================================

TEST(BitArrayVersionWidening, TheCounterIsUnsignedAndSixtyFourBits) {
    static_assert(std::is_unsigned_v<Value>,
                  "an unsigned counter is what makes the increment defined at its maximum");
    static_assert(sizeof(Value) == sizeof(ulongcs));
    static_assert(std::is_same_v<Value, ulongcs>);
    EXPECT_EQ(sizeof(Value), 8u);
    EXPECT_EQ(std::numeric_limits<Value>::max(), std::numeric_limits<ulongcs>::max());
}

TEST(BitArrayVersionWidening, TheEnumeratorSnapshotIsExactlyAsWideAsTheCounter) {
    // A narrower snapshot would make the guard a silent truncation, which is the failure mode
    // this ticket refused to ship. It is asserted through the counter's public value_type
    // rather than by naming the private field, so the assertion survives any future
    // reshuffling of the header.
    using CounterValue = System::Collections::detail::MutationCounter::value_type;
    using SnapshotValue = System::Collections::detail::MutationVersion;
    static_assert(std::is_same_v<CounterValue, SnapshotValue>);
    static_assert(std::is_same_v<SnapshotValue, Value>);
    static_assert(sizeof(SnapshotValue) == sizeof(Value));
    SUCCEED();
}

TEST(BitArrayVersionWidening, ThePublicEnumeratorGrewToFortyBytesOnLp64) {
    // THE approved cost of this ticket, asserted rather than described. sizeof(BitArray)
    // itself is UNCHANGED at 48 -- the wider counter landed in the four bytes of tail padding
    // the container already had -- and that figure stays in
    // CollectionVersionCounterTests.cpp's PublishedObjectSizesAreUnchanged, where it is still
    // true.
    if constexpr (sizeof(void*) == 8) {
        EXPECT_EQ(sizeof(NG::BitArray::Enumerator), 40u);
        EXPECT_EQ(alignof(NG::BitArray::Enumerator), 8u);
        EXPECT_EQ(sizeof(NG::BitArray), 48u);
        EXPECT_EQ(alignof(NG::BitArray), 8u);
    } else {
        SUCCEED() << "published sizes are LP64-only";
    }
}

TEST(BitArrayVersionWidening, NoCounterOrCounterTypeLeaksThroughAPublicSurface) {
    static_assert(std::is_same_v<decltype(std::declval<const NG::BitArray&>()
                                              .getLengthProperty()), intcs>);
    static_assert(std::is_same_v<decltype(std::declval<const NG::BitArray&>()
                                              .getCountProperty()), intcs>);
    static_assert(std::is_same_v<decltype(std::declval<const NG::BitArray&>()
                                              .PopCount()), intcs>);
    static_assert(std::is_same_v<decltype(std::declval<NG::BitArray&>().GetEnumerator()),
                                 NG::IEnumerator*>);
    static_assert(std::is_same_v<decltype(std::declval<const NG::BitArray::Enumerator&>()
                                              .getCurrentProperty()), std::any>);
    SUCCEED();
}

TEST(BitArrayVersionWidening, ValueSemanticsTraitsAreUnchanged) {
    static_assert(std::is_copy_constructible_v<NG::BitArray>);
    static_assert(std::is_copy_assignable_v<NG::BitArray>);
    static_assert(std::is_move_constructible_v<NG::BitArray>);
    static_assert(std::is_move_assignable_v<NG::BitArray>);
    static_assert(std::is_nothrow_move_constructible_v<NG::BitArray>);
    static_assert(!std::is_polymorphic_v<NG::BitArray>);
    static_assert(!std::is_trivially_copyable_v<NG::BitArray>);
    static_assert(std::is_standard_layout_v<NG::BitArray>);
    // The enumerator is polymorphic before and after; that is what makes it an IEnumerator.
    static_assert(std::is_polymorphic_v<NG::BitArray::Enumerator>);
    static_assert(std::is_base_of_v<NG::IEnumerator, NG::BitArray::Enumerator>);
    SUCCEED();
}

// =====================================================================================
// The horizon that moved. This is what the ticket exists for.
// =====================================================================================

TEST(BitArrayVersionWidening, NoStaleEnumeratorIsRevalidatedAcrossTheOld2Pow32Distance) {
    for (const ulongcs multiple : {1ULL, 2ULL, 7ULL, 1000ULL}) {
        NG::BitArray b = eight();
        Enumerating e(b);
        ASSERT_TRUE(e->MoveNext());
        const Value base = versionOf(b);
        positionVersion(b, static_cast<Value>(base + multiple * kAliasStep));
        EXPECT_THROW(e->MoveNext(), System::InvalidOperationException)
            << "multiple=" << multiple;
    }
}

TEST(BitArrayVersionWidening, ResetAlsoFailsFastAcrossTheOld2Pow32Distance) {
    NG::BitArray b = eight();
    Enumerating e(b);
    ASSERT_TRUE(e->MoveNext());
    const Value base = versionOf(b);
    positionVersion(b, static_cast<Value>(base + kAliasStep));
    EXPECT_THROW(e->Reset(), System::InvalidOperationException);
}

TEST(BitArrayVersionWidening, ASnapshotSharingOnlyItsLowThirtyTwoBitsIsStillRejected) {
    // The exact truncation a container-only widening would have left behind: the counter's low
    // 32 bits match the snapshot and the high word does not. The old field could not tell them
    // apart. This asserts BOTH halves -- that the low words really do match, so the case is
    // the one being claimed, and that the guard fires anyway.
    NG::BitArray b = eight();
    Enumerating e(b);
    ASSERT_TRUE(e->MoveNext());
    const Value snapshot = versionOf(b);
    const Value aliased = static_cast<Value>(snapshot + 3ULL * kAliasStep);
    positionVersion(b, aliased);

    EXPECT_EQ(static_cast<uintcs>(versionOf(b)), static_cast<uintcs>(snapshot))
        << "the positioned value must share its low 32 bits with the snapshot";
    EXPECT_NE(versionOf(b), snapshot) << "and must differ in the high word";
    EXPECT_THROW(e->MoveNext(), System::InvalidOperationException);
}

TEST(BitArrayVersionWidening, TheHorizonIsNowTwoToTheSixtyFourNotTwoToTheThirtyTwo) {
    // Stated as a BOUND, not as an impossibility. The counter is modular; at its maximum it
    // wraps to zero, and an enumerator snapshotted at the maximum is still invalidated by
    // that wrap.
    NG::BitArray b = eight();
    positionVersion(b, std::numeric_limits<Value>::max());
    Enumerating e(b);
    ASSERT_TRUE(e->MoveNext());
    ASSERT_NO_THROW(b.Set(0, true));
    EXPECT_EQ(versionOf(b), Value{0});
    EXPECT_THROW(e->MoveNext(), System::InvalidOperationException);
}

TEST(BitArrayVersionWidening, TheStepThatUsedToWrapNowJustAdvances) {
    NG::BitArray b = eight();
    positionVersion(b, static_cast<Value>(kU32Max));
    b.Set(0, true);
    EXPECT_EQ(versionOf(b), kAliasStep) << "UINT32_MAX + 1 must be 2^32, not 0";
    EXPECT_NE(versionOf(b), Value{0});
}

TEST(BitArrayVersionWidening, MutationAtEachNamedBoundaryValueMovesForwardExactly) {
    const ulongcs starts[] = {0ULL,
                              1ULL,
                              kOldInt32Max - 1,
                              kOldInt32Max,
                              kOldInt32Max + 1,
                              kU32Max - 1,
                              kU32Max,
                              kAliasStep,
                              kAliasStep + 5,
                              std::numeric_limits<Value>::max() - 1};
    for (const ulongcs start : starts) {
        NG::BitArray b = eight();
        positionVersion(b, static_cast<Value>(start));
        ASSERT_NO_THROW(b.Set(2, true)) << "start=" << start;
        EXPECT_EQ(versionOf(b), static_cast<Value>(start + 1)) << "start=" << start;
    }
}

TEST(BitArrayVersionWidening, AnEnumeratorTakenBeforeTheTransitionIsInvalidatedByIt) {
    // Snapshot at UINT32_MAX - 1, then step across UINT32_MAX and past it. The enumerator must
    // stay invalid for every step, and must never come back.
    NG::BitArray b = eight();
    positionVersion(b, static_cast<Value>(kU32Max - 1));
    Enumerating e(b);
    ASSERT_TRUE(e->MoveNext());
    for (int i = 0; i < 5; ++i) {
        b.Set(i % 8, (i % 2) == 0);
        EXPECT_THROW(e->MoveNext(), System::InvalidOperationException) << "step " << i;
    }
    EXPECT_EQ(versionOf(b), static_cast<Value>(kU32Max - 1 + 5));
}

TEST(BitArrayVersionWidening, MutationPastTheOldBoundaryStaysExactAndKeepsInvalidating) {
    NG::BitArray b = eight();
    positionVersion(b, static_cast<Value>(kAliasStep));
    for (int i = 0; i < 25; ++i) {
        Enumerating e(b);
        ASSERT_TRUE(e->MoveNext());
        b.Set(i % 8, (i % 3) == 0);
        EXPECT_EQ(versionOf(b), static_cast<Value>(kAliasStep + static_cast<ulongcs>(i) + 1));
        EXPECT_THROW(e->MoveNext(), System::InvalidOperationException) << "step " << i;
    }
}

TEST(BitArrayVersionWidening, EveryMutationFamilyInvalidatesAcrossTheOldBoundary) {
    // Each mutating member, exercised with the counter positioned exactly at the old 32-bit
    // horizon, so the widening is proved not to have disturbed any one of them.
    struct Case {
        const char* name;
        void (*apply)(NG::BitArray&);
    };
    const Case cases[] = {
        {"Set", [](NG::BitArray& b) { b.Set(0, true); }},
        {"SetAll", [](NG::BitArray& b) { b.SetAll(true); }},
        {"Not", [](NG::BitArray& b) { b.Not(); }},
        {"And", [](NG::BitArray& b) { NG::BitArray o(8, true); b.And(o); }},
        {"Or", [](NG::BitArray& b) { NG::BitArray o(8, true); b.Or(o); }},
        {"Xor", [](NG::BitArray& b) { NG::BitArray o(8, true); b.Xor(o); }},
        {"LeftShift", [](NG::BitArray& b) { b.LeftShift(1); }},
        {"RightShift", [](NG::BitArray& b) { b.RightShift(1); }},
        {"setLengthProperty", [](NG::BitArray& b) { b.setLengthProperty(16); }},
        {"copy assignment", [](NG::BitArray& b) { NG::BitArray o(8, true); b = o; }},
        {"move assignment", [](NG::BitArray& b) { NG::BitArray o(8, true); b = std::move(o); }},
    };
    for (const Case& c : cases) {
        NG::BitArray b = eight();
        positionVersion(b, static_cast<Value>(kU32Max));
        Enumerating e(b);
        ASSERT_TRUE(e->MoveNext()) << c.name;
        c.apply(b);
        EXPECT_EQ(versionOf(b), kAliasStep) << c.name;
        EXPECT_THROW(e->MoveNext(), System::InvalidOperationException) << c.name;
    }
}

TEST(BitArrayVersionWidening, ExhaustionWrapsToZeroWithoutUndefinedBehaviour) {
    // BitArray never had signed-overflow UB -- its counter was already unsigned before #1787.
    // The widening must not introduce any, and the wrap must stay defined.
    NG::BitArray b = eight();
    positionVersion(b, std::numeric_limits<Value>::max());
    ASSERT_NO_THROW(b.SetAll(true));
    EXPECT_EQ(versionOf(b), Value{0});
    ASSERT_NO_THROW(b.SetAll(false));
    EXPECT_EQ(versionOf(b), Value{1});
}

TEST(BitArrayVersionWidening, NoCounterValueIsReservedAsASentinel) {
    const ulongcs probes[] = {0ULL, 1ULL, kOldInt32Max, kU32Max, kAliasStep,
                              std::numeric_limits<Value>::max()};
    for (const ulongcs v : probes) {
        NG::BitArray b = eight();
        positionVersion(b, static_cast<Value>(v));
        EXPECT_EQ(b.getLengthProperty(), 8) << "counter at " << v;
        EXPECT_EQ(b.PopCount(), 2) << "counter at " << v;
        EXPECT_EQ(walk(b).size(), 8u) << "counter at " << v;
    }
}

// =====================================================================================
// The mutation-version delta matrix -- identical before and after the widening.
// =====================================================================================

namespace {

/** Runs @p op on a fresh eight() and returns the counter delta. */
template <typename F>
ulongcs deltaOf(F&& op) {
    NG::BitArray b = eight();
    const ulongcs before = versionOf(b);
    op(b);
    return versionOf(b) - before;
}

}  // namespace

TEST(BitArrayVersionWidening, EveryEffectiveMutationAdvancesTheCounterByExactlyOne) {
    EXPECT_EQ(deltaOf([](NG::BitArray& b) { b.Set(0, true); }), 1u) << "Set false->true";
    EXPECT_EQ(deltaOf([](NG::BitArray& b) { b.Set(1, false); }), 1u) << "Set true->false";
    EXPECT_EQ(deltaOf([](NG::BitArray& b) { b.SetAll(true); }), 1u);
    EXPECT_EQ(deltaOf([](NG::BitArray& b) { b.SetAll(false); }), 1u);
    EXPECT_EQ(deltaOf([](NG::BitArray& b) { b.Not(); }), 1u);
    EXPECT_EQ(deltaOf([](NG::BitArray& b) { NG::BitArray o(8, true); b.And(o); }), 1u);
    EXPECT_EQ(deltaOf([](NG::BitArray& b) { NG::BitArray o(8, true); b.Or(o); }), 1u);
    EXPECT_EQ(deltaOf([](NG::BitArray& b) { NG::BitArray o(8, true); b.Xor(o); }), 1u);
    EXPECT_EQ(deltaOf([](NG::BitArray& b) { b.LeftShift(1); }), 1u);
    EXPECT_EQ(deltaOf([](NG::BitArray& b) { b.RightShift(1); }), 1u);
    EXPECT_EQ(deltaOf([](NG::BitArray& b) { b.setLengthProperty(16); }), 1u) << "grow";
    EXPECT_EQ(deltaOf([](NG::BitArray& b) { b.setLengthProperty(4); }), 1u) << "shrink";
    EXPECT_EQ(deltaOf([](NG::BitArray& b) { b.setLengthProperty(0); }), 1u) << "shrink to zero";
}

TEST(BitArrayVersionWidening, AMutationThatChangesNoBitStillAdvancesTheCounter) {
    // The established contract, shared with .NET, which bumps unconditionally in Set,
    // SetAll, the Length setter, and the count<=0 early return of both shifts
    // (BitArray.cs:341, :360, :665, :521, :584). This ticket changed a WIDTH; it deliberately
    // did not revisit the mutation/no-op policy. Pinned so the next reader sees a decision.
    EXPECT_EQ(deltaOf([](NG::BitArray& b) { b.Set(1, true); }), 1u) << "true -> true";
    EXPECT_EQ(deltaOf([](NG::BitArray& b) { b.Set(0, false); }), 1u) << "false -> false";
    EXPECT_EQ(deltaOf([](NG::BitArray& b) { b.LeftShift(0); }), 1u) << "a zero-count shift";
    EXPECT_EQ(deltaOf([](NG::BitArray& b) { b.RightShift(0); }), 1u) << "a zero-count shift";
    EXPECT_EQ(deltaOf([](NG::BitArray& b) { b.setLengthProperty(8); }), 1u) << "same length";
    EXPECT_EQ(deltaOf([](NG::BitArray& b) { b.And(b); }), 1u) << "And with itself";
    EXPECT_EQ(deltaOf([](NG::BitArray& b) { b.Or(b); }), 1u) << "Or with itself";

    // ... and each of those really did leave the bit pattern alone, so the case is the one
    // being claimed rather than an accidental mutation.
    NG::BitArray b = eight();
    const std::vector<bool> before = patternOf(b);
    b.Set(1, true);
    b.Set(0, false);
    b.LeftShift(0);
    b.RightShift(0);
    b.setLengthProperty(8);
    EXPECT_EQ(patternOf(b), before);
    EXPECT_EQ(versionOf(b), 7u) << "two from eight(), five no-effect mutations";
}

TEST(BitArrayVersionWidening, EveryRejectedOrReadOnlyOperationAdvancesNothing) {
    // Rejected: the argument check runs BEFORE the bump in every case.
    auto rejects = [](auto&& op) {
        NG::BitArray b = eight();
        const ulongcs before = versionOf(b);
        EXPECT_THROW(op(b), System::ArgumentException);
        EXPECT_EQ(versionOf(b), before);
    };
    rejects([](NG::BitArray& b) { b.Set(8, true); });
    rejects([](NG::BitArray& b) { b.Set(-1, true); });
    rejects([](NG::BitArray& b) { b.setLengthProperty(-1); });
    rejects([](NG::BitArray& b) { b.LeftShift(-1); });
    rejects([](NG::BitArray& b) { b.RightShift(-1); });
    rejects([](NG::BitArray& b) { NG::BitArray o(5); b.And(o); });
    rejects([](NG::BitArray& b) { NG::BitArray o(5); b.Or(o); });
    rejects([](NG::BitArray& b) { NG::BitArray o(5); b.Xor(o); });

    // Read-only.
    EXPECT_EQ(deltaOf([](NG::BitArray& b) { (void)b.Get(3); }), 0u);
    EXPECT_EQ(deltaOf([](NG::BitArray& b) { const NG::BitArray& c = b; (void)c[3]; }), 0u);
    EXPECT_EQ(deltaOf([](NG::BitArray& b) { (void)b.getLengthProperty(); }), 0u);
    EXPECT_EQ(deltaOf([](NG::BitArray& b) { (void)b.getCountProperty(); }), 0u);
    EXPECT_EQ(deltaOf([](NG::BitArray& b) { (void)b.getIsReadOnlyProperty(); }), 0u);
    EXPECT_EQ(deltaOf([](NG::BitArray& b) { (void)b.getIsSynchronizedProperty(); }), 0u);
    EXPECT_EQ(deltaOf([](NG::BitArray& b) { (void)b.getSyncRootProperty(); }), 0u);
    EXPECT_EQ(deltaOf([](NG::BitArray& b) { (void)b.PopCount(); }), 0u);
    EXPECT_EQ(deltaOf([](NG::BitArray& b) { (void)b.HasAllSet(); }), 0u);
    EXPECT_EQ(deltaOf([](NG::BitArray& b) { (void)b.HasAnySet(); }), 0u);
    EXPECT_EQ(deltaOf([](NG::BitArray& b) { (void)b.Clone(); }), 0u);
    EXPECT_EQ(deltaOf([](NG::BitArray& b) { std::vector<bool> d; b.CopyTo(d); }), 0u);
    EXPECT_EQ(deltaOf([](NG::BitArray& b) { std::vector<bytecs> d; b.CopyTo(d); }), 0u);
    EXPECT_EQ(deltaOf([](NG::BitArray& b) { int n = 0; for (bool v : b) n += v; (void)n; }), 0u);
    EXPECT_EQ(deltaOf([](NG::BitArray& b) { (void)walk(b); }), 0u) << "enumerating";
}

TEST(BitArrayVersionWidening, ARejectedMutationLeavesAnOutstandingEnumeratorValid) {
    NG::BitArray b = eight();
    positionVersion(b, static_cast<Value>(kAliasStep));
    Enumerating e(b);
    ASSERT_TRUE(e->MoveNext());
    EXPECT_THROW(b.Set(99, true), System::ArgumentOutOfRangeException);
    EXPECT_THROW(b.setLengthProperty(-4), System::ArgumentOutOfRangeException);
    EXPECT_THROW(b.LeftShift(-2), System::ArgumentOutOfRangeException);
    {
        NG::BitArray mismatched(3);
        EXPECT_THROW(b.And(mismatched), System::ArgumentException);
    }
    EXPECT_EQ(versionOf(b), kAliasStep);
    EXPECT_NO_THROW(e->MoveNext());
}

// =====================================================================================
// Copy, move and assignment -- ticket #1787's repair, at the new width.
// =====================================================================================

TEST(BitArrayVersionWidening, CopyAssignmentAdvancesTheDestinationAcrossTheBoundary) {
    NG::BitArray a = eight();
    NG::BitArray b(8, true);
    positionVersion(a, static_cast<Value>(kU32Max));
    Enumerating e(a);
    ASSERT_TRUE(e->MoveNext());
    a = b;
    EXPECT_EQ(versionOf(a), kAliasStep);
    EXPECT_THROW(e->MoveNext(), System::InvalidOperationException);
    EXPECT_TRUE(a.HasAllSet()) << "assignment still replaces the contents";
}

TEST(BitArrayVersionWidening, MoveAssignmentAdvancesTheDestinationAcrossTheBoundary) {
    NG::BitArray a = eight();
    NG::BitArray b(8, true);
    positionVersion(a, static_cast<Value>(kU32Max));
    Enumerating e(a);
    ASSERT_TRUE(e->MoveNext());
    a = std::move(b);
    EXPECT_EQ(versionOf(a), kAliasStep);
    EXPECT_THROW(e->MoveNext(), System::InvalidOperationException);
    EXPECT_TRUE(a.HasAllSet());
}

TEST(BitArrayVersionWidening, AssignmentWithAMatchingSourceCounterStillInvalidates) {
    // The exact pre-#1787 defect shape, which needed no overflow at all: the two counters
    // merely had to be equal, and the implicitly declared operator= copied the source's value
    // into the destination. It is re-pinned here at the new width because the widening rewrote
    // the field it operates on.
    NG::BitArray a = eight();
    NG::BitArray b(8, true);
    positionVersion(a, static_cast<Value>(kAliasStep + 11));
    positionVersion(b, static_cast<Value>(kAliasStep + 11));
    Enumerating e(a);
    ASSERT_TRUE(e->MoveNext());
    a = b;
    EXPECT_EQ(versionOf(a), kAliasStep + 12) << "the destination advanced, it did not adopt";
    EXPECT_EQ(versionOf(b), kAliasStep + 11) << "the source is untouched";
    EXPECT_THROW(e->MoveNext(), System::InvalidOperationException);
}

TEST(BitArrayVersionWidening, SelfAssignmentAdvancesAsThisCollectionDocuments) {
    // BitArray has NO `if (this != &other)` guard -- it uses the implicitly declared
    // operators -- so self-assignment bumps and invalidates. #1787 section 6.2 argues why
    // that is the safe answer rather than merely a conservative one, and LinkedList<T> is the
    // ONE collection that behaves the other way. Unchanged by this ticket.
    NG::BitArray a = eight();
    positionVersion(a, static_cast<Value>(kAliasStep));
    Enumerating e(a);
    ASSERT_TRUE(e->MoveNext());
    NG::BitArray& alias = a;
    a = alias;
    EXPECT_EQ(versionOf(a), kAliasStep + 1);
    EXPECT_EQ(a.getLengthProperty(), 8) << "self copy assignment preserves the contents";
    EXPECT_EQ(a.PopCount(), 2);
    EXPECT_THROW(e->MoveNext(), System::InvalidOperationException);
}

TEST(BitArrayVersionWidening, SelfMoveAssignmentAdvancesAndIsMemorySafe) {
    // Self MOVE assignment of the backing std::vector<bool> leaves it in a valid but
    // unspecified state -- libstdc++ empties it. That is pre-existing standard-library
    // behaviour, not something this ticket introduced or can fix from here; it is recorded
    // because a test that did not state it would be hiding it. What matters for this ticket is
    // that the counter still advances and that nothing is read out of bounds afterwards.
    NG::BitArray a = eight();
    positionVersion(a, static_cast<Value>(kAliasStep));
    Enumerating e(a);
    ASSERT_TRUE(e->MoveNext());
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wself-move"
#endif
    a = std::move(a);
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
    EXPECT_EQ(versionOf(a), kAliasStep + 1);
    EXPECT_THROW(e->MoveNext(), System::InvalidOperationException);
    EXPECT_GE(a.getLengthProperty(), 0) << "and the object is still usable";
    EXPECT_NO_THROW((void)walk(a));
}

TEST(BitArrayVersionWidening, AnIndependentCopyDoesNotInvalidateTheOriginalsEnumerator) {
    NG::BitArray a = eight();
    positionVersion(a, static_cast<Value>(kAliasStep + 3));
    NG::BitArray copy = a;
    EXPECT_EQ(versionOf(copy), kAliasStep + 3) << "copy construction inherits, as .NET Clone does";

    Enumerating e(a);
    ASSERT_TRUE(e->MoveNext());
    copy.SetAll(true);
    EXPECT_EQ(versionOf(copy), kAliasStep + 4);
    EXPECT_EQ(versionOf(a), kAliasStep + 3);
    EXPECT_NO_THROW(e->MoveNext()) << "the original's enumerator is untouched";
    EXPECT_FALSE(a.HasAllSet()) << "and the original's bits are untouched";
}

TEST(BitArrayVersionWidening, AssignmentDoesNotDisturbTheSourcesOwnEnumerators) {
    NG::BitArray a = eight();
    NG::BitArray b(8, true);
    positionVersion(b, static_cast<Value>(kAliasStep));
    Enumerating onSource(b);
    ASSERT_TRUE(onSource->MoveNext());
    a = b;
    EXPECT_EQ(versionOf(b), kAliasStep);
    EXPECT_NO_THROW(onSource->MoveNext());
}

TEST(BitArrayVersionWidening, MoveConstructionInheritsTheCounterAndTransfersTheBits) {
    NG::BitArray a(8, true);
    positionVersion(a, static_cast<Value>(kAliasStep + 9));
    NG::BitArray moved = std::move(a);
    EXPECT_EQ(versionOf(moved), kAliasStep + 9);
    EXPECT_EQ(moved.getLengthProperty(), 8);
    EXPECT_TRUE(moved.HasAllSet());
}

TEST(BitArrayVersionWidening, CloneStartsAFreshCounterAtZero) {
    // Clone() goes through BitArray(const std::vector<bool>&), which default-constructs the
    // counter. That is the pre-existing contract and differs from copy construction; recorded
    // so the widening is measured not to have moved it.
    NG::BitArray a = eight();
    positionVersion(a, static_cast<Value>(kAliasStep + 99));
    NG::BitArray c = a.Clone();
    EXPECT_EQ(versionOf(c), Value{0});
    EXPECT_EQ(patternOf(c), patternOf(a)) << "and the bits are equal";

    // A clone is independent in both directions.
    Enumerating e(c);
    ASSERT_TRUE(e->MoveNext());
    a.SetAll(true);
    EXPECT_NO_THROW(e->MoveNext());
}

// =====================================================================================
// Ordinary behaviour at ordinary sizes -- the widening must be invisible here.
// =====================================================================================

TEST(BitArrayVersionWidening, AnEmptyBitArrayEnumeratesToCompletionAndStaysExact) {
    NG::BitArray b(intcs{0});
    EXPECT_EQ(versionOf(b), Value{0}) << "a fresh BitArray starts counting from zero";
    EXPECT_EQ(b.getLengthProperty(), 0);
    Enumerating e(b);
    EXPECT_FALSE(e->MoveNext());
    EXPECT_FALSE(e->MoveNext());
    EXPECT_EQ(versionOf(b), Value{0});
}

TEST(BitArrayVersionWidening, AMutationOnAnEmptyBitArrayInvalidatesItsEnumerator) {
    // A zero-length BitArray has no bit to Set, but it does have SetAll, Not, the shifts and
    // the Length setter -- each of which is an effective mutation by the established contract.
    for (int op = 0; op < 4; ++op) {
        NG::BitArray b(intcs{0});
        positionVersion(b, static_cast<Value>(kAliasStep));
        Enumerating e(b);
        EXPECT_FALSE(e->MoveNext());
        switch (op) {
            case 0: b.SetAll(true); break;
            case 1: b.Not(); break;
            case 2: b.LeftShift(1); break;
            case 3: b.setLengthProperty(1); break;
            default: break;
        }
        EXPECT_EQ(versionOf(b), kAliasStep + 1) << "op " << op;
        // MoveNext short-circuits once the enumerator is after the last element, which for a
        // zero-length array it already is; Reset is the path that reaches the guard.
        EXPECT_THROW(e->Reset(), System::InvalidOperationException) << "op " << op;
    }
}

TEST(BitArrayVersionWidening, ASingleBitBehavesAtTheBoundary) {
    NG::BitArray b(1);
    positionVersion(b, static_cast<Value>(kU32Max));
    EXPECT_EQ(walk(b), (std::vector<bool>{false}));
    b.Set(0, true);
    EXPECT_EQ(versionOf(b), kAliasStep);
    EXPECT_EQ(walk(b), (std::vector<bool>{true}));
}

TEST(BitArrayVersionWidening, WordBoundarySizesEnumerateExactlyAtTheBoundary) {
    // 31/32/33 and 63/64/65 straddle both the 32-bit and 64-bit word boundaries of the
    // backing std::vector<bool>, which is where an off-by-one in the index path would show.
    for (const intcs len : {intcs{7}, intcs{8}, intcs{31}, intcs{32}, intcs{33}, intcs{63},
                            intcs{64}, intcs{65}, intcs{127}, intcs{128}, intcs{129}}) {
        NG::BitArray b(len, true);
        positionVersion(b, static_cast<Value>(kU32Max - 1));
        const std::vector<bool> seen = walk(b);
        EXPECT_EQ(static_cast<intcs>(seen.size()), len) << "len=" << len;
        for (bool v : seen) EXPECT_TRUE(v) << "len=" << len;
        EXPECT_EQ(b.PopCount(), len) << "len=" << len;

        Enumerating e(b);
        ASSERT_TRUE(e->MoveNext()) << "len=" << len;
        b.Set(len - 1, false);
        EXPECT_EQ(versionOf(b), kU32Max) << "len=" << len;
        EXPECT_THROW(e->MoveNext(), System::InvalidOperationException) << "len=" << len;
    }
}

TEST(BitArrayVersionWidening, EveryValuePatternSurvivesTheBoundary) {
    constexpr intcs kLen = 96;
    struct Pattern {
        const char* name;
        bool (*bit)(intcs);
    };
    const Pattern patterns[] = {
        {"all false", [](intcs) { return false; }},
        {"all true", [](intcs) { return true; }},
        {"alternating", [](intcs i) { return (i % 2) == 0; }},
        {"sparse", [](intcs i) { return (i % 17) == 0; }},
    };
    for (const Pattern& p : patterns) {
        NG::BitArray b(kLen);
        for (intcs i = 0; i < kLen; ++i)
            if (p.bit(i)) b.Set(i, true);
        positionVersion(b, static_cast<Value>(kAliasStep - 1));

        std::vector<bool> expected;
        for (intcs i = 0; i < kLen; ++i) expected.push_back(p.bit(i));
        EXPECT_EQ(walk(b), expected) << p.name;
        EXPECT_EQ(patternOf(b), expected) << p.name;

        // Not() at the boundary flips every bit and advances exactly once.
        b.Not();
        EXPECT_EQ(versionOf(b), kAliasStep) << p.name;
        for (intcs i = 0; i < kLen; ++i) EXPECT_EQ(b.Get(i), !p.bit(i)) << p.name << " i=" << i;
    }
}

TEST(BitArrayVersionWidening, ALargeBoundedBitArrayEnumeratesExactlyAcrossTheWrap) {
    // Bounded on purpose: 100,000 bits and ~100,000 mutations, positioned so that the counter
    // WRAPS from the 64-bit maximum mid-loop. No test may perform 2^32 real mutations.
    constexpr intcs kLen = 100000;
    NG::BitArray b(kLen);
    positionVersion(b, std::numeric_limits<Value>::max() - 50);
    for (intcs i = 0; i < kLen; i += 3) b.Set(i, true);

    intcs expected = 0;
    for (intcs i = 0; i < kLen; i += 3) ++expected;
    EXPECT_EQ(b.PopCount(), expected);

    const std::vector<bool> seen = walk(b);
    ASSERT_EQ(static_cast<intcs>(seen.size()), kLen);
    for (intcs i = 0; i < kLen; ++i) EXPECT_EQ(seen[static_cast<size_t>(i)], (i % 3) == 0)
        << "i=" << i;
}

TEST(BitArrayVersionWidening, CopyToBothOverloadsStayExactAtTheBoundary) {
    NG::BitArray b(16);
    b.Set(0, true);
    b.Set(9, true);
    b.Set(15, true);
    positionVersion(b, static_cast<Value>(kAliasStep + 4));

    std::vector<bool> bools;
    b.CopyTo(bools);
    EXPECT_EQ(bools, patternOf(b));

    std::vector<bytecs> bytes;
    b.CopyTo(bytes);
    ASSERT_EQ(bytes.size(), 2u);
    EXPECT_EQ(bytes[0], static_cast<bytecs>(0x01));
    EXPECT_EQ(bytes[1], static_cast<bytecs>(0x82));
    EXPECT_EQ(versionOf(b), kAliasStep + 4) << "CopyTo advances nothing";
}

// =====================================================================================
// The enumerator's own contract -- unchanged by this ticket, and pinned to prove it.
// =====================================================================================

TEST(BitArrayVersionWidening, TheEnumeratorStateMachineIsUnchangedAtTheBoundary) {
    // Ticket #1767's lifecycle guard: Current throws before the first MoveNext and after the
    // last element. Ticket #1793's owning std::any return is what it throws around.
    NG::BitArray b = eight();
    positionVersion(b, static_cast<Value>(kAliasStep));
    Enumerating e(b);
    EXPECT_THROW((void)e->getCurrentProperty(), System::InvalidOperationException);
    ASSERT_TRUE(e->MoveNext());
    EXPECT_NO_THROW((void)e->getCurrentProperty());
    while (e->MoveNext()) {}
    EXPECT_THROW((void)e->getCurrentProperty(), System::InvalidOperationException);
    EXPECT_NO_THROW(e->Reset());
    EXPECT_THROW((void)e->getCurrentProperty(), System::InvalidOperationException)
        << "Reset returns to the before-first state";
    EXPECT_TRUE(e->MoveNext());
}

TEST(BitArrayVersionWidening, TheBoxedCurrentIsAnOwningCopyAtTheBoundary) {
    // Ticket #1793: getCurrentProperty() returns an owning std::any, so the box survives any
    // later mutation of the collection -- including one that invalidates the enumerator.
    NG::BitArray b = eight();
    positionVersion(b, static_cast<Value>(kAliasStep));
    Enumerating e(b);
    ASSERT_TRUE(e->MoveNext());
    ASSERT_TRUE(e->MoveNext());  // index 1, which eight() sets true
    const std::any boxed = e->getCurrentProperty();
    ASSERT_EQ(boxed.type(), typeid(bool));
    b.SetAll(false);
    EXPECT_TRUE(std::any_cast<bool>(boxed)) << "the box is unaffected by the mutation";
    EXPECT_THROW(e->MoveNext(), System::InvalidOperationException);
    EXPECT_TRUE(std::any_cast<bool>(boxed)) << "and by the invalidation";
}

TEST(BitArrayVersionWidening, ReadingWithoutMutatingNeverInvalidatesAtTheBoundary) {
    NG::BitArray b(32, true);
    positionVersion(b, static_cast<Value>(kU32Max));
    Enumerating e(b);
    int seen = 0;
    while (e->MoveNext()) {
        (void)b.Get(0);
        (void)b.PopCount();
        (void)b.HasAllSet();
        ++seen;
    }
    EXPECT_EQ(seen, 32);
    EXPECT_EQ(versionOf(b), kU32Max);
    EXPECT_NO_THROW(e->Reset());
}

TEST(BitArrayVersionWidening, TwoEnumeratorsOverOneArrayAreIndependentUntilAMutation) {
    NG::BitArray b(4, true);
    positionVersion(b, static_cast<Value>(kAliasStep));
    Enumerating first(b);
    Enumerating second(b);
    ASSERT_TRUE(first->MoveNext());
    ASSERT_TRUE(second->MoveNext());
    ASSERT_TRUE(second->MoveNext());
    EXPECT_NO_THROW(first->MoveNext());
    b.Set(0, false);
    EXPECT_THROW(first->MoveNext(), System::InvalidOperationException);
    EXPECT_THROW(second->MoveNext(), System::InvalidOperationException);
}

TEST(BitArrayVersionWidening, TheDirectlyNamedPublicEnumeratorBehavesLikeTheInterfaceOne) {
    // BitArray::Enumerator is PUBLIC, which is the entire reason this ticket needed approval:
    // a consumer can name one, store one by value, and observe its size. Exercised directly
    // here, not only through IEnumerator*.
    NG::BitArray b = eight();
    positionVersion(b, static_cast<Value>(kAliasStep));
    NG::BitArray::Enumerator direct(&b);
    ASSERT_TRUE(direct.MoveNext());
    EXPECT_FALSE(std::any_cast<bool>(direct.getCurrentProperty()));
    ASSERT_TRUE(direct.MoveNext());
    EXPECT_TRUE(std::any_cast<bool>(direct.getCurrentProperty()));
    b.SetAll(false);
    EXPECT_THROW(direct.MoveNext(), System::InvalidOperationException);
    EXPECT_THROW(direct.Reset(), System::InvalidOperationException);
}

TEST(BitArrayVersionWidening, ThePublicStlIteratorsStillCarryNoSnapshot) {
    // begin()/end() return raw std::vector<bool> iterators and are deliberately NOT
    // version-checked, exactly as before. Pre-existing, documented, and pinned so a reader
    // does not mistake this ticket for having changed it.
    NG::BitArray b(4, true);
    positionVersion(b, static_cast<Value>(kAliasStep));
    int n = 0;
    for (bool v : b) n += v;
    EXPECT_EQ(n, 4);
    EXPECT_EQ(versionOf(b), kAliasStep) << "range-for advances nothing";
    EXPECT_EQ(sizeof(decltype(b.begin())), sizeof(decltype(b.end())));
}

TEST(BitArrayVersionWidening, EnumerationThroughTheIEnumeratorInterfaceIsUnchanged) {
    NG::BitArray b(3);
    b.Set(1, true);
    positionVersion(b, static_cast<Value>(kAliasStep));
    NG::IEnumerator* e = b.GetEnumerator();
    std::vector<bool> seen;
    while (e->MoveNext()) seen.push_back(std::any_cast<bool>(e->getCurrentProperty()));
    EXPECT_EQ(seen, (std::vector<bool>{false, true, false}));
    e->Reset();
    seen.clear();
    while (e->MoveNext()) seen.push_back(std::any_cast<bool>(e->getCurrentProperty()));
    EXPECT_EQ(seen, (std::vector<bool>{false, true, false}));
    delete e;
    EXPECT_EQ(versionOf(b), kAliasStep);
}
