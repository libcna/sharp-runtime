// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Permanent regression suite for ticket #1786 (REMED-COLL-VERSION-COUNTER-OVERFLOW).
//
// SortedSet<T>::State::version was a SharpRuntime::intcs -- int32_t -- that started at 0 and was
// only ever incremented. Every consumer compares it for EQUALITY alone: the per-view Count cache
// tests `cachedCountVersion_ == currentVersion` and Iterator::checkVersion tests
// `version_ != state_->version`. That produced four distinct defects, each reproduced against the
// real production header before this ticket changed anything
// (build-probe-sortedset/probe12_version_overflow.cpp, pre-fix logs probe12_prefix_*.log):
//
//   1. `++version` at INTCS_MAX is signed-integer overflow -- undefined behaviour. UBSan:
//      "SortedSet.hpp:425:20: runtime error: signed integer overflow: 2147483647 + 1 cannot be
//      represented in type 'int'" inside SortedSet<int>::Add.
//   2. 2^32 mutations later the counter returns to a value an outstanding Iterator captured, and
//      the guard silently accepts the stale iterator (probe: dereference yielded 10 with no throw).
//   3. The same wrap silently revalidates a stale cached view Count (probe: answered 4 when the
//      range held 3).
//   4. The cache's "never computed" sentinel was -1, which the counter itself reaches after
//      2^32 - 1 mutations, so a view that had NEVER computed its Count answered its
//      zero-initialised cache (probe: answered 0 when the range held 5).
//
// The repair widens the shared counter and the Iterator snapshot to SharpRuntime::ulongcs
// (uint64_t) -- unsigned arithmetic is modulo 2^n, so the increment is defined for every
// representable prior value, and a repeat needs 2^64 mutations. The Count cache tag has to stay
// 32 bits to hold the published object layout, so it is stored BIASED BY ONE and compared
// widened back to 64 bits: a never-filled cache holds 0 and `version + 1` is never 0, a tag
// filled at version V matches only while the counter is exactly V, and once the counter outgrows
// the tag nothing matches and the cache stops being written, so a view's Count becomes an O(k)
// recomputation. The bias is what keeps the read path branch-free -- the range-check alternative
// measured +1 ns on every Count call. sizeof, alignof, every member offset, and the mangled
// GetViewBetween symbol are unchanged (build-probe-sortedset/probe13_member_offsets.cpp).
//
// Reaching those transitions through the public API would need billions of real mutations, so the
// near-boundary cases below position the counter through the test-only access seam
// SharpRuntime::Testing::SortedSetVersionAccess<T>, which SortedSet.hpp declares and befriends and
// which is DEFINED ONLY HERE. It grants access and no behaviour; nothing in the library or in a
// consumer can reach it.
#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/Collections/Generic/SortedSet.hpp"
#include "System/InvalidOperationException.hpp"

using SharpRuntime::intcs;
using SharpRuntime::uintcs;
using SharpRuntime::ulongcs;
using System::Collections::Generic::SortedSet;

// =====================================================================================
// The test-only access seam. Defined in exactly one translation unit, this one.
// =====================================================================================
namespace SharpRuntime::Testing {

template<typename T>
struct SortedSetVersionAccess {
    using Set = System::Collections::Generic::SortedSet<T>;

    /** The shared mutation counter, as any handle onto the state sees it. */
    static ulongcs version(const Set& s) { return s.state_->version; }

    /** Positions the shared counter, standing in for that many effective mutations. */
    static void positionVersion(Set& s, ulongcs value) { s.state_->version = value; }

    /** The handle's own Count-cache tag; equals notCached() until the cache is first filled. */
    static uintcs cachedTag(const Set& s) {
        return s.cachedCountVersion_.load(std::memory_order_acquire);
    }

    /** The handle's own cached count, meaningful only when cachedTag() is not notCached(). */
    static intcs cachedCount(const Set& s) {
        return s.cachedCount_.load(std::memory_order_relaxed);
    }

    static constexpr ulongcs maxCacheableVersion() { return Set::kMaxCacheableVersion; }
    static constexpr uintcs notCached() { return Set::kCountNotCached; }

    /** The tag a cache filled at @p version holds: the counter biased by one. */
    static constexpr uintcs tagFor(ulongcs version) { return Set::countCacheTag(version); }
};

} // namespace SharpRuntime::Testing

namespace {

using Access = SharpRuntime::Testing::SortedSetVersionAccess<int>;
using StringAccess = SharpRuntime::Testing::SortedSetVersionAccess<std::string>;

/** The counter distance at which the pre-#1786 32-bit counter could no longer tell two states apart. */
constexpr ulongcs kOldAliasStride = 1ull << 32;

/** The counter value at which the pre-#1786 32-bit counter's increment was undefined behaviour. */
constexpr ulongcs kOldUbBoundary = static_cast<ulongcs>(SharpRuntime::INTCS_MAX);

SortedSet<int> range(int firstInclusive, int lastInclusive) {
    SortedSet<int> s;
    for (int i = firstInclusive; i <= lastInclusive; ++i) s.Add(i);
    return s;
}

/** Element type providing ONLY operator<, the ordering contract SortedSet<T> documents. */
struct LessOnly {
    int value = 0;
    bool operator<(const LessOnly& other) const { return value < other.value; }
};

} // namespace

// =====================================================================================
// 1. Ordinary version behaviour -- the contract ticket 1713 established, unchanged
// =====================================================================================

TEST(SortedSetVersionOverflowTests, AFreshOwningSetStartsAtVersionZero) {
    SortedSet<int> s;
    EXPECT_EQ(Access::version(s), 0u);
    EXPECT_FALSE(s.getIsViewProperty());

    SortedSet<int> fromList{3, 1, 2};
    // The initializer-list constructor builds the std::set directly and bumps nothing.
    EXPECT_EQ(Access::version(fromList), 0u);
    EXPECT_EQ(fromList.getCountProperty(), 3);
}

TEST(SortedSetVersionOverflowTests, AddIncrementsExactlyOncePerInsertion) {
    SortedSet<int> s;
    EXPECT_TRUE(s.Add(1));
    EXPECT_EQ(Access::version(s), 1u);
    EXPECT_TRUE(s.Add(2));
    EXPECT_EQ(Access::version(s), 2u);

    // A rejected duplicate is not a structural modification.
    EXPECT_FALSE(s.Add(2));
    EXPECT_EQ(Access::version(s), 2u);
}

TEST(SortedSetVersionOverflowTests, RemoveIncrementsOnlyWhenItErases) {
    SortedSet<int> s = range(1, 3);
    const ulongcs before = Access::version(s);

    EXPECT_FALSE(s.Remove(99));                  // absent
    EXPECT_EQ(Access::version(s), before);

    EXPECT_TRUE(s.Remove(2));
    EXPECT_EQ(Access::version(s), before + 1);

    EXPECT_FALSE(s.Remove(2));                   // already gone
    EXPECT_EQ(Access::version(s), before + 1);
}

TEST(SortedSetVersionOverflowTests, ClearIncrementsOnlyWhenSomethingWasRemoved) {
    SortedSet<int> s = range(1, 5);
    const ulongcs afterFill = Access::version(s);

    s.Clear();
    EXPECT_EQ(Access::version(s), afterFill + 1);

    s.Clear();                                   // already empty
    EXPECT_EQ(Access::version(s), afterFill + 1);

    // A view's Clear bumps only when its range held something.
    SortedSet<int> t = range(1, 10);
    SortedSet<int> emptyRange = t.GetViewBetween(50, 60);
    const ulongcs beforeViewClear = Access::version(t);
    emptyRange.Clear();
    EXPECT_EQ(Access::version(t), beforeViewClear);

    SortedSet<int> populated = t.GetViewBetween(3, 7);
    populated.Clear();
    EXPECT_EQ(Access::version(t), beforeViewClear + 1);
}

TEST(SortedSetVersionOverflowTests, ViewAndParentShareExactlyOneCounter) {
    SortedSet<int> s = range(1, 10);
    SortedSet<int> view = s.GetViewBetween(3, 7);
    SortedSet<int> nested = view.GetViewBetween(4, 6);

    // GetViewBetween itself is not a mutation.
    const ulongcs afterViews = Access::version(s);
    EXPECT_EQ(Access::version(view), afterViews);
    EXPECT_EQ(Access::version(nested), afterViews);

    EXPECT_TRUE(nested.Remove(5));
    EXPECT_EQ(Access::version(s), afterViews + 1);
    EXPECT_EQ(Access::version(view), afterViews + 1);
    EXPECT_EQ(Access::version(nested), afterViews + 1);

    // An out-of-range view Remove changes nothing at all.
    EXPECT_FALSE(view.Remove(9));
    EXPECT_EQ(Access::version(s), afterViews + 1);

    // An out-of-range view Add throws and writes nothing.
    EXPECT_THROW((void)view.Add(99), System::ArgumentOutOfRangeException);
    EXPECT_EQ(Access::version(s), afterViews + 1);
}

TEST(SortedSetVersionOverflowTests, SetAlgebraBumpsOncePerEffectiveElementMutation) {
    SortedSet<int> s = range(1, 5);
    const ulongcs start = Access::version(s);

    s.UnionWith(range(4, 7));                    // adds 6 and 7 only
    EXPECT_EQ(Access::version(s), start + 2);

    s.ExceptWith(range(1, 2));                   // removes 1 and 2
    EXPECT_EQ(Access::version(s), start + 4);

    s.IntersectWith(range(3, 6));                // removes 7
    EXPECT_EQ(Access::version(s), start + 5);

    s.SymmetricExceptWith(range(3, 3));          // removes 3
    EXPECT_EQ(Access::version(s), start + 6);
    EXPECT_EQ(s.getCountProperty(), 3);          // 4, 5, 6
}

TEST(SortedSetVersionOverflowTests, CopyMoveAndAssignmentRebindTheCounterCorrectly) {
    SortedSet<int> s = range(1, 5);
    SortedSet<int> view = s.GetViewBetween(2, 4);

    // A copied owning set deep-clones, counter included.
    SortedSet<int> clone = s;
    EXPECT_EQ(Access::version(clone), Access::version(s));
    EXPECT_TRUE(clone.Add(99));
    EXPECT_EQ(Access::version(clone), Access::version(s) + 1);   // independent from here on
    EXPECT_FALSE(s.Contains(99));

    // A copied view is another handle onto the same counter.
    SortedSet<int> viewCopy = view;
    EXPECT_EQ(Access::version(viewCopy), Access::version(s));
    EXPECT_TRUE(viewCopy.Remove(3));
    EXPECT_EQ(Access::version(view), Access::version(s));
    EXPECT_EQ(Access::version(viewCopy), Access::version(s));

    // A moved-from object is a valid, empty owning set with a fresh counter.
    SortedSet<int> moved = std::move(clone);
    EXPECT_EQ(Access::version(clone), 0u);                       // NOLINT(bugprone-use-after-move)
    EXPECT_EQ(clone.getCountProperty(), 0);

    // Assignment rebinds this handle to the other state's counter.
    SortedSet<int> other = range(100, 110);
    const ulongcs otherVersion = Access::version(other);
    SortedSet<int> target = range(1, 3);
    target = other;
    EXPECT_EQ(Access::version(target), otherVersion);
}

// =====================================================================================
// 2. The old int32 undefined-behaviour boundary
// =====================================================================================

TEST(SortedSetVersionOverflowTests, TheCounterIsSixtyFourBitUnsigned) {
    SortedSet<int> s;
    static_assert(std::is_same_v<decltype(Access::version(s)), ulongcs>,
                  "the shared mutation counter must be SharpRuntime::ulongcs");
    static_assert(!std::numeric_limits<ulongcs>::is_signed,
                  "an unsigned counter is what makes every increment defined");
    static_assert(sizeof(ulongcs) == 8);
    EXPECT_EQ(std::numeric_limits<ulongcs>::max(), 18446744073709551615ull);
}

TEST(SortedSetVersionOverflowTests, IncrementAtTheOldInt32BoundaryIsDefinedAndMovesForward) {
    SortedSet<int> s = range(1, 3);
    Access::positionVersion(s, kOldUbBoundary);
    ASSERT_EQ(Access::version(s), kOldUbBoundary);

    // Pre-#1786 this exact call was UBSan-reported signed-integer overflow inside Add.
    EXPECT_TRUE(s.Add(100));
    EXPECT_EQ(Access::version(s), kOldUbBoundary + 1);
    EXPECT_GT(Access::version(s), kOldUbBoundary);           // forward, never backwards
    EXPECT_EQ(s.getCountProperty(), 4);

    // And it keeps separating states from each other on the far side.
    EXPECT_TRUE(s.Remove(100));
    EXPECT_EQ(Access::version(s), kOldUbBoundary + 2);
    s.Clear();
    EXPECT_EQ(Access::version(s), kOldUbBoundary + 3);
}

TEST(SortedSetVersionOverflowTests, MutationAtAndPastEveryOldBoundaryNeverThrows) {
    for (const ulongcs at : {kOldUbBoundary - 1, kOldUbBoundary, kOldUbBoundary + 1,
                             kOldAliasStride - 2, kOldAliasStride - 1, kOldAliasStride,
                             kOldAliasStride + 1}) {
        SortedSet<int> s = range(1, 3);
        Access::positionVersion(s, at);
        EXPECT_NO_THROW({
            EXPECT_TRUE(s.Add(50));
            EXPECT_TRUE(s.Remove(50));
            s.Clear();
        }) << "at counter value " << at;
        EXPECT_EQ(Access::version(s), at + 3);
        EXPECT_EQ(s.getCountProperty(), 0);
    }
}

TEST(SortedSetVersionOverflowTests, TheCounterStillWrapsOnlyAtItsOwnMaximum) {
    // Exhaustion behaviour, stated exactly: the increment is modular, so at ULONGCS_MAX it
    // becomes 0 with no undefined behaviour, no throw, and no termination. Reaching it needs 2^64
    // effective mutations on one state -- over 580 years at one per nanosecond -- so this is the
    // documented terminal case, not a reachable one.
    SortedSet<int> s = range(1, 3);
    Access::positionVersion(s, SharpRuntime::ULONGCS_MAX);

    auto atMax = s.begin();
    ASSERT_EQ(*atMax, 1);

    EXPECT_NO_THROW((void)s.Add(4));
    EXPECT_EQ(Access::version(s), 0u);

    // The wrap still separates this state from the snapshot taken before it.
    EXPECT_THROW((void)*atMax, System::InvalidOperationException);
    EXPECT_EQ(s.getCountProperty(), 4);
}

// =====================================================================================
// 3. Iterator and enumerator invalidation across the old wrap point
// =====================================================================================

TEST(SortedSetVersionOverflowTests, AnIteratorIsNotRevalidatedByTheOldWrapDistance) {
    SortedSet<int> s = range(10, 12);
    const ulongcs snapshot = Access::version(s);

    auto it = s.begin();
    ASSERT_EQ(*it, 10);

    // A genuine structural modification. std::set::insert does not invalidate iterators, so the
    // version guard is the only thing standing between `it` and a changed container.
    ASSERT_TRUE(s.Add(20));
    EXPECT_THROW((void)*it, System::InvalidOperationException);

    // Stand in for 2^32 further effective mutations: the distance at which the old 32-bit
    // counter came back to `snapshot` and silently accepted this iterator.
    Access::positionVersion(s, snapshot + kOldAliasStride);
    EXPECT_THROW((void)*it, System::InvalidOperationException);
    EXPECT_THROW((void)++it, System::InvalidOperationException);
    EXPECT_THROW((void)it.operator->(), System::InvalidOperationException);

    // Several further multiples of the old stride, and the old UB boundary itself.
    for (const ulongcs offset : {kOldAliasStride, 2 * kOldAliasStride, 7 * kOldAliasStride}) {
        Access::positionVersion(s, snapshot + offset);
        EXPECT_THROW((void)*it, System::InvalidOperationException) << "offset " << offset;
    }
    Access::positionVersion(s, kOldUbBoundary);
    EXPECT_THROW((void)*it, System::InvalidOperationException);

    // Returning the counter to the snapshot value is the only thing that revalidates it, and no
    // sequence of mutations can do that any more.
    Access::positionVersion(s, snapshot);
    EXPECT_NO_THROW((void)*it);
}

TEST(SortedSetVersionOverflowTests, AnIteratorTakenAtTheBoundaryIsInvalidatedByTheNextMutation) {
    SortedSet<int> s = range(1, 3);
    Access::positionVersion(s, kOldUbBoundary);

    auto atBoundary = s.begin();
    ASSERT_EQ(*atBoundary, 1);
    EXPECT_NO_THROW((void)++atBoundary);

    ASSERT_TRUE(s.Add(4));                                // crosses the old boundary
    EXPECT_THROW((void)*atBoundary, System::InvalidOperationException);

    // A snapshot taken after the crossing works normally.
    auto afterBoundary = s.begin();
    int sum = 0;
    for (auto w = s.begin(); w != s.end(); ++w) sum += *w;
    EXPECT_EQ(sum, 10);
    EXPECT_EQ(*afterBoundary, 1);
}

TEST(SortedSetVersionOverflowTests, ParentAndViewIteratorsInvalidateEachOtherPastTheBoundary) {
    SortedSet<int> s = range(1, 10);
    Access::positionVersion(s, kOldAliasStride + 5);
    SortedSet<int> view = s.GetViewBetween(3, 7);
    SortedSet<int> nested = view.GetViewBetween(4, 6);

    auto parentIt = s.begin();
    auto viewIt = view.begin();
    auto nestedIt = nested.begin();
    ASSERT_EQ(*parentIt, 1);
    ASSERT_EQ(*viewIt, 3);
    ASSERT_EQ(*nestedIt, 4);

    // A rejected duplicate is not a structural modification, at any counter value.
    ASSERT_FALSE(nested.Add(5));                          // 5 is already in range and present
    ASSERT_EQ(Access::version(s), kOldAliasStride + 5);   // nothing bumped
    EXPECT_NO_THROW((void)*parentIt);
    EXPECT_NO_THROW((void)*viewIt);
    EXPECT_NO_THROW((void)*nestedIt);

    // One real mutation through any handle invalidates every outstanding iterator.

    ASSERT_TRUE(s.Add(20));
    EXPECT_THROW((void)*parentIt, System::InvalidOperationException);
    EXPECT_THROW((void)*viewIt, System::InvalidOperationException);
    EXPECT_THROW((void)*nestedIt, System::InvalidOperationException);
}

TEST(SortedSetVersionOverflowTests, RangeForOverAViewPastTheBoundaryStillFailsFast) {
    SortedSet<int> s = range(1, 10);
    Access::positionVersion(s, kOldUbBoundary - 1);
    SortedSet<int> view = s.GetViewBetween(3, 7);

    EXPECT_THROW({
        for (const int x : view) { (void)x; s.Add(100); }
    }, System::InvalidOperationException);

    // The counter crossed the old boundary during that loop and stayed well-defined.
    EXPECT_GE(Access::version(s), kOldUbBoundary);
    EXPECT_TRUE(s.Contains(100));
}

// =====================================================================================
// 4. The Count cache: exact tag, explicit horizon, collision-free sentinel
// =====================================================================================

TEST(SortedSetVersionOverflowTests, TheCacheSentinelLiesOutsideTheCacheableRange) {
    static_assert(std::is_same_v<decltype(Access::notCached()), uintcs>);
    EXPECT_EQ(Access::notCached(), 0u);
    EXPECT_EQ(Access::maxCacheableVersion(),
              static_cast<ulongcs>(SharpRuntime::UINTCS_MAX) - 1u);

    // The tag is the counter biased by one, so no counter value can produce the "never filled"
    // tag: that is what makes the single equality test in getCountProperty() exact. The
    // pre-#1786 sentinel was -1, whose bit pattern IS a value the old 32-bit counter reached.
    for (const ulongcs v : {ulongcs{0}, ulongcs{1}, kOldUbBoundary, kOldAliasStride - 2,
                            Access::maxCacheableVersion()}) {
        EXPECT_NE(Access::tagFor(v), Access::notCached()) << "at counter value " << v;
        EXPECT_EQ(static_cast<ulongcs>(Access::tagFor(v)), v + 1u) << "at counter value " << v;
    }
    // Past its reach the tag no longer identifies the counter, which is why the cache stops
    // being written there rather than storing something that could later be believed.
    EXPECT_LT(static_cast<ulongcs>(SharpRuntime::UINTCS_MAX),
              Access::maxCacheableVersion() + 2u);
}

TEST(SortedSetVersionOverflowTests, ANeverFilledCacheIsNotMistakenForAWarmOne) {
    SortedSet<int> s = range(1, 10);
    SortedSet<int> view = s.GetViewBetween(3, 7);
    ASSERT_EQ(Access::cachedTag(view), Access::notCached());
    ASSERT_EQ(Access::cachedCount(view), 0);

    // 0xFFFFFFFF is the pre-#1786 `-1` sentinel's bit pattern, reached after 2^32 - 1 effective
    // mutations. A cold cache must never answer its zero-initialised count there.
    Access::positionVersion(s, static_cast<ulongcs>(SharpRuntime::UINTCS_MAX));
    EXPECT_EQ(view.getCountProperty(), 5);
    EXPECT_FALSE(view.getIsEmptyProperty());

    // Nor at any other counter value, warm or cold.
    for (const ulongcs at : {ulongcs{0}, kOldUbBoundary, kOldAliasStride,
                             Access::maxCacheableVersion()}) {
        SortedSet<int> t = range(1, 10);
        SortedSet<int> cold = t.GetViewBetween(3, 7);
        Access::positionVersion(t, at);
        EXPECT_EQ(cold.getCountProperty(), 5) << "at counter value " << at;
    }
}

TEST(SortedSetVersionOverflowTests, ACachedCountIsNotRevalidatedByTheOldWrapDistance) {
    SortedSet<int> s = range(1, 10);
    SortedSet<int> view = s.GetViewBetween(3, 7);
    ASSERT_EQ(view.getCountProperty(), 5);

    const ulongcs warm = Access::version(s);
    ASSERT_EQ(Access::cachedTag(view), Access::tagFor(warm));
    ASSERT_EQ(Access::cachedCount(view), 5);

    // Remove two in-range elements without reading Count in between, then stand the counter
    // exactly 2^32 mutations on from the fill -- the pre-#1786 false-hit point.
    ASSERT_TRUE(s.Remove(4));
    ASSERT_TRUE(s.Remove(5));
    Access::positionVersion(s, warm + kOldAliasStride);

    EXPECT_EQ(view.getCountProperty(), 3);
    EXPECT_EQ(s.getCountProperty(), 8);

    // Several further multiples of the old stride.
    ASSERT_TRUE(s.Remove(6));
    for (const ulongcs offset : {2 * kOldAliasStride, 5 * kOldAliasStride}) {
        Access::positionVersion(s, warm + offset);
        EXPECT_EQ(view.getCountProperty(), 2) << "offset " << offset;
    }
}

TEST(SortedSetVersionOverflowTests, TheCacheIsExactUpToTheHorizonAndUsedThere) {
    SortedSet<int> s = range(1, 10);
    SortedSet<int> view = s.GetViewBetween(3, 7);

    Access::positionVersion(s, Access::maxCacheableVersion() - 1u);
    EXPECT_EQ(view.getCountProperty(), 5);
    EXPECT_EQ(Access::cachedTag(view), Access::tagFor(Access::maxCacheableVersion() - 1u));

    // Exactly on the horizon the tag still identifies the counter, so the cache is filled.
    Access::positionVersion(s, Access::maxCacheableVersion());
    EXPECT_EQ(view.getCountProperty(), 5);
    EXPECT_EQ(Access::cachedTag(view), Access::tagFor(Access::maxCacheableVersion()));
    EXPECT_EQ(Access::cachedCount(view), 5);

    // A repeated read on the horizon is a cache hit and must agree.
    EXPECT_EQ(view.getCountProperty(), 5);
    EXPECT_EQ(view.getCountProperty(), 5);
}

TEST(SortedSetVersionOverflowTests, PastTheHorizonTheCacheIsNeitherReadNorWritten) {
    SortedSet<int> s = range(1, 10);
    SortedSet<int> view = s.GetViewBetween(3, 7);

    Access::positionVersion(s, Access::maxCacheableVersion());
    ASSERT_EQ(view.getCountProperty(), 5);
    const uintcs tagOnTheHorizon = Access::cachedTag(view);

    // One past the horizon: correct answer, and the cache is left exactly as it was.
    Access::positionVersion(s, Access::maxCacheableVersion() + 1u);
    EXPECT_EQ(view.getCountProperty(), 5);
    EXPECT_EQ(Access::cachedTag(view), tagOnTheHorizon);
    EXPECT_EQ(Access::cachedCount(view), 5);

    // A mutation past the horizon is tracked exactly, still without touching the cache.
    ASSERT_TRUE(s.Remove(4));
    EXPECT_EQ(view.getCountProperty(), 4);
    EXPECT_EQ(Access::cachedTag(view), tagOnTheHorizon);
    EXPECT_EQ(Access::cachedCount(view), 5);          // the stale 5 is never consulted again

    ASSERT_TRUE(view.Add(4));
    EXPECT_EQ(view.getCountProperty(), 5);
    ASSERT_TRUE(view.Remove(3));
    EXPECT_EQ(view.getCountProperty(), 4);
    view.Clear();
    EXPECT_EQ(view.getCountProperty(), 0);
    EXPECT_TRUE(view.getIsEmptyProperty());
    EXPECT_EQ(s.getCountProperty(), 5);               // 1, 2, 8, 9, 10
}

TEST(SortedSetVersionOverflowTests, CountTracksEveryMutationFarPastTheHorizon) {
    SortedSet<int> s;
    SortedSet<int> view = s.GetViewBetween(0, 1000);
    Access::positionVersion(s, 3 * kOldAliasStride + 7);

    intcs expected = 0;
    for (int i = 0; i < 200; ++i) {
        ASSERT_TRUE(s.Add(i));
        ++expected;
        ASSERT_EQ(view.getCountProperty(), expected) << "after Add(" << i << ")";
    }
    for (int i = 0; i < 200; i += 2) {
        ASSERT_TRUE(s.Remove(i));
        --expected;
        ASSERT_EQ(view.getCountProperty(), expected) << "after Remove(" << i << ")";
    }
    EXPECT_EQ(view.getCountProperty(), 100);
    EXPECT_EQ(s.getCountProperty(), 100);
    EXPECT_EQ(Access::version(s), 3 * kOldAliasStride + 7 + 300);
}

TEST(SortedSetVersionOverflowTests, NestedAndOverlappingViewsAreExactPastTheHorizon) {
    SortedSet<int> s = range(1, 20);
    Access::positionVersion(s, Access::maxCacheableVersion() + 100u);

    SortedSet<int> outer = s.GetViewBetween(5, 15);
    SortedSet<int> inner = outer.GetViewBetween(8, 12);
    SortedSet<int> left = s.GetViewBetween(1, 12);
    SortedSet<int> right = s.GetViewBetween(8, 20);

    ASSERT_EQ(outer.getCountProperty(), 11);
    ASSERT_EQ(inner.getCountProperty(), 5);
    ASSERT_EQ(left.getCountProperty(), 12);
    ASSERT_EQ(right.getCountProperty(), 13);

    ASSERT_TRUE(inner.Remove(10));
    EXPECT_EQ(inner.getCountProperty(), 4);
    EXPECT_EQ(outer.getCountProperty(), 10);
    EXPECT_EQ(left.getCountProperty(), 11);
    EXPECT_EQ(right.getCountProperty(), 12);
    EXPECT_EQ(s.getCountProperty(), 19);

    ASSERT_TRUE(outer.Add(10));
    EXPECT_EQ(inner.getCountProperty(), 5);
    EXPECT_EQ(left.getCountProperty(), 12);
    EXPECT_EQ(right.getCountProperty(), 13);

    // A mutation outside a view's range must not disturb its count.
    ASSERT_TRUE(s.Remove(2));
    EXPECT_EQ(left.getCountProperty(), 11);
    EXPECT_EQ(right.getCountProperty(), 13);
    EXPECT_EQ(inner.getCountProperty(), 5);
}

TEST(SortedSetVersionOverflowTests, AnOwningFullSetCountIsUnaffectedByTheHorizon) {
    SortedSet<int> s = range(1, 50);
    for (const ulongcs at : {ulongcs{0}, kOldUbBoundary, Access::maxCacheableVersion(),
                             Access::maxCacheableVersion() + 1u, 9 * kOldAliasStride}) {
        Access::positionVersion(s, at);
        EXPECT_EQ(s.getCountProperty(), 50) << "at counter value " << at;
        EXPECT_FALSE(s.getIsEmptyProperty());
        // The owning-set path never touches the cache, at any counter value.
        EXPECT_EQ(Access::cachedTag(s), Access::notCached());
    }
}

TEST(SortedSetVersionOverflowTests, CopyMoveAndAssignmentCarryNoCacheAcrossTheHorizon) {
    SortedSet<int> s = range(1, 10);
    SortedSet<int> big = s.GetViewBetween(1, 10);
    SortedSet<int> small = s.GetViewBetween(4, 5);
    Access::positionVersion(s, Access::maxCacheableVersion() - 5u);
    ASSERT_EQ(big.getCountProperty(), 10);
    ASSERT_EQ(small.getCountProperty(), 2);

    // Rebinding a warm handle must not let it answer the value it cached for another range.
    big = small;
    EXPECT_EQ(Access::cachedTag(big), Access::notCached());
    EXPECT_EQ(big.getCountProperty(), 2);

    // The same across the horizon.
    Access::positionVersion(s, Access::maxCacheableVersion() + 5u);
    SortedSet<int> wide = s.GetViewBetween(1, 10);
    ASSERT_EQ(wide.getCountProperty(), 10);
    SortedSet<int> narrow = s.GetViewBetween(4, 5);
    wide = narrow;
    EXPECT_EQ(wide.getCountProperty(), 2);

    SortedSet<int> moved = std::move(wide);
    EXPECT_EQ(moved.getCountProperty(), 2);
    EXPECT_EQ(wide.getCountProperty(), 0);                 // NOLINT(bugprone-use-after-move)
    EXPECT_EQ(Access::cachedTag(wide), Access::notCached());  // NOLINT(bugprone-use-after-move)

    SortedSet<int> copied = moved;
    EXPECT_EQ(Access::cachedTag(copied), Access::notCached());
    EXPECT_EQ(copied.getCountProperty(), 2);
}

TEST(SortedSetVersionOverflowTests, AViewOutlivingItsParentIsExactPastTheHorizon) {
    SortedSet<int> view;
    {
        SortedSet<int> owner = range(1, 10);
        SharpRuntime::Testing::SortedSetVersionAccess<int>::positionVersion(
            owner, Access::maxCacheableVersion() + 42u);
        view = owner.GetViewBetween(3, 7);
        ASSERT_EQ(view.getCountProperty(), 5);
    }
    EXPECT_TRUE(view.getIsViewProperty());
    EXPECT_EQ(view.getCountProperty(), 5);
    EXPECT_TRUE(view.Remove(4));
    EXPECT_EQ(view.getCountProperty(), 4);
    EXPECT_GT(Access::version(view), Access::maxCacheableVersion());
}

// =====================================================================================
// 5. Non-trivial and minimal element types at the boundary
// =====================================================================================

TEST(SortedSetVersionOverflowTests, NonTrivialElementTypeAcrossTheBoundary) {
    SortedSet<std::string> s;
    for (const char* w : {"alpha", "bravo", "charlie", "delta", "echo", "foxtrot"}) s.Add(w);
    StringAccess::positionVersion(s, kOldUbBoundary);

    SortedSet<std::string> view = s.GetViewBetween("bravo", "echo");
    EXPECT_EQ(view.getCountProperty(), 4);

    auto it = view.begin();
    ASSERT_EQ(*it, "bravo");
    EXPECT_TRUE(s.Add("golf"));                            // crosses the old boundary
    EXPECT_EQ(StringAccess::version(s), kOldUbBoundary + 1);
    EXPECT_THROW((void)*it, System::InvalidOperationException);

    StringAccess::positionVersion(s, StringAccess::maxCacheableVersion() + 3u);
    EXPECT_EQ(view.getCountProperty(), 4);
    EXPECT_TRUE(s.Remove("charlie"));
    EXPECT_EQ(view.getCountProperty(), 3);
    EXPECT_EQ(s.getCountProperty(), 6);
}

TEST(SortedSetVersionOverflowTests, LessOnlyElementTypeAcrossTheBoundary) {
    using LessOnlyAccess = SharpRuntime::Testing::SortedSetVersionAccess<LessOnly>;

    SortedSet<LessOnly> s;
    for (int i = 1; i <= 10; ++i) s.Add(LessOnly{i});
    LessOnlyAccess::positionVersion(s, LessOnlyAccess::maxCacheableVersion());

    SortedSet<LessOnly> view = s.GetViewBetween(LessOnly{3}, LessOnly{7});
    EXPECT_EQ(view.getCountProperty(), 5);

    EXPECT_TRUE(s.Remove(LessOnly{5}));                    // crosses the horizon
    EXPECT_GT(LessOnlyAccess::version(s), LessOnlyAccess::maxCacheableVersion());
    EXPECT_EQ(view.getCountProperty(), 4);
    EXPECT_TRUE(view.Add(LessOnly{5}));
    EXPECT_EQ(view.getCountProperty(), 5);
}

// =====================================================================================
// 6. Source and layout compatibility with tickets #1783 and #1784
// =====================================================================================

// The exact public signatures ticket #1786 must not change: this repair is entirely internal.
// A changed return type, parameter list, or const-qualification is a compile error here.
static_assert(std::is_same_v<decltype(&SortedSet<int>::getCountProperty),
                             intcs (SortedSet<int>::*)() const>,
              "getCountProperty must stay a const member returning intcs by value");
static_assert(std::is_same_v<decltype(&SortedSet<int>::GetViewBetween),
                             SortedSet<int> (SortedSet<int>::*)(const int&, const int&)>,
              "GetViewBetween must stay non-const and return SortedSet<T> by value (#1783)");
static_assert(std::is_same_v<decltype(&SortedSet<int>::Add), bool (SortedSet<int>::*)(const int&)>);
static_assert(std::is_same_v<decltype(&SortedSet<int>::Remove),
                             bool (SortedSet<int>::*)(const int&)>);
static_assert(std::is_same_v<decltype(&SortedSet<int>::Clear), void (SortedSet<int>::*)()>);
static_assert(std::is_same_v<decltype(&SortedSet<int>::getIsEmptyProperty),
                             bool (SortedSet<int>::*)() const>);
static_assert(std::is_same_v<decltype(&SortedSet<int>::getIsViewProperty),
                             bool (SortedSet<int>::*)() const>);

// The value-semantics traits #1783 published and #1784 preserved.
static_assert(std::is_copy_constructible_v<SortedSet<int>>);
static_assert(std::is_copy_assignable_v<SortedSet<int>>);
static_assert(std::is_nothrow_move_constructible_v<SortedSet<int>>);
static_assert(std::is_nothrow_move_assignable_v<SortedSet<int>>);
static_assert(!std::is_polymorphic_v<SortedSet<int>>);
static_assert(!std::is_trivially_copyable_v<SortedSet<int>>);
static_assert(std::is_nothrow_move_constructible_v<SortedSet<std::string>>);

// The Count-cache fields keep the widths that hold the published object layout. Widening the tag
// to match the 64-bit counter is exactly what this ticket could NOT do without an ABI break, and
// these assertions are what stop a later change from doing it by accident.
static_assert(sizeof(std::atomic<uintcs>) == sizeof(intcs),
              "the Count cache tag must stay exactly as wide as the field it replaced");
static_assert(alignof(std::atomic<uintcs>) == alignof(intcs));
static_assert(sizeof(std::atomic<intcs>) == sizeof(intcs));

TEST(SortedSetVersionOverflowTests, PublishedObjectLayoutIsUnchangedByTheWiderCounter) {
    // The figures ticket #1783 published and ticket #1784 preserved byte for byte. Asserted at
    // run time and skipped elsewhere rather than static_asserted, matching this repository's rule
    // that it keeps no permanent architecture-specific sizeof assertions; the authoritative
    // cross-check is build-probe-sortedset/probe13_member_offsets.cpp, which compares every
    // member OFFSET against the pre-#1786 header and finds them identical.
    if constexpr (sizeof(void*) != 8 || sizeof(std::size_t) != 8) {
        GTEST_SKIP() << "layout figures were published for LP64/LLP64 64-bit builds only";
    } else {
        EXPECT_EQ(sizeof(SortedSet<int>), 40u);
        EXPECT_EQ(sizeof(SortedSet<std::string>), 104u);
        EXPECT_EQ(sizeof(SortedSet<int>::Iterator), 40u);
        EXPECT_EQ(alignof(SortedSet<int>), 8u);
        EXPECT_EQ(alignof(SortedSet<std::string>), 8u);
        // The wider snapshot fits in padding the Iterator already had -- that is the whole reason
        // this repair needed no layout approval.
        EXPECT_GE(sizeof(SortedSet<int>::Iterator), sizeof(void*) * 2 + sizeof(void*) * 2);
    }
}

TEST(SortedSetVersionOverflowTests, TheCounterIsNotVisibleThroughThePublicSurface) {
    // The repair must remain an implementation detail: Count still answers a plain intcs and no
    // 64-bit counter or atomic leaks into the public API.
    SortedSet<int> s = range(1, 10);
    SortedSet<int> view = s.GetViewBetween(3, 7);

    static_assert(std::is_same_v<decltype(view.getCountProperty()), intcs>,
                  "Count must be returned by value as a plain intcs");
    intcs count = view.getCountProperty();
    count += s.getCountProperty();
    EXPECT_EQ(count, 15);

    const std::vector<int> contents = view.ToVector();
    EXPECT_EQ(contents, (std::vector<int>{3, 4, 5, 6, 7}));
    EXPECT_EQ(view.getMinProperty(), 3);
    EXPECT_EQ(view.getMaxProperty(), 7);
}
