// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Permanent regression suite for ticket #1784 (REMED-COLL-SORTEDSET-VIEW-COUNT-RACE).
//
// Ticket #1783 gave a bounded SortedSet<T> view the lazy Count cache .NET's TreeSubSet keeps
// (its `count`/`_countVersion` pair), but held it in two plain `mutable intcs` fields that the
// `const` getCountProperty() wrote. Two threads reading Count through one view object
// therefore performed conflicting non-atomic accesses -- a C++ data race and undefined
// behaviour, even though both calls are observationally read-only. ThreadSanitizer reported
// "Read of size 4 ... Previous write of size 4 ... in getCountProperty() const" at
// SortedSet.hpp:315/:317 (build-probe-sortedset/probe10_prefix_same-view-count.log).
//
// Ticket #1784 made the two cache fields std::atomic with a release/acquire publication
// protocol: the count is stored first, the version is stored last with `release`, and a reader
// that observes the version with `acquire` therefore also observes the matching count. The
// object layout, the cache, and every #1783 behaviour are unchanged.
//
// This file pins three things permanently:
//   1. the full functional Count matrix, so the repair cannot silently change a Count result;
//   2. the cache-sensitive properties -- no stale value survives a mutation, no assignment or
//      move inherits a cached value computed for state it no longer refers to, and repeated
//      Count calls change nothing observable;
//   3. source and layout compatibility with #1783.
//
// The deterministic ThreadSanitizer reproduction itself lives in the repository-local,
// gitignored probe build-probe-sortedset/probe10_tsan_count_race.cpp, because TSan is not part
// of normal CTest. The one multithreaded case below is deliberately assertion-deterministic
// rather than timing-based: every observation must equal an exactly known count, so it either
// always passes or has found a real defect.
#include <gtest/gtest.h>

#include <atomic>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/Collections/Generic/SortedSet.hpp"

using SharpRuntime::intcs;
using System::Collections::Generic::SortedSet;

namespace {

/** Element type providing ONLY operator<, the ordering contract SortedSet<T> documents. */
struct LessOnly {
    int value = 0;
    bool operator<(const LessOnly& other) const { return value < other.value; }
};

/** Element type whose std::less ordering is DESCENDING -- the custom-comparer case. */
struct Descending {
    int value = 0;
    bool operator<(const Descending& other) const { return value > other.value; }
};

SortedSet<int> range(int firstInclusive, int lastInclusive) {
    SortedSet<int> s;
    for (int i = firstInclusive; i <= lastInclusive; ++i) s.Add(i);
    return s;
}

} // namespace

// =====================================================================================
// 1. Functional Count behaviour
// =====================================================================================

TEST(SortedSetCountCacheTests, FullSetCountIsExact) {
    SortedSet<int> empty;
    EXPECT_EQ(empty.getCountProperty(), 0);
    EXPECT_TRUE(empty.getIsEmptyProperty());

    SortedSet<int> s = range(1, 10);
    EXPECT_EQ(s.getCountProperty(), 10);
    EXPECT_FALSE(s.getIsEmptyProperty());
    EXPECT_FALSE(s.getIsViewProperty());

    // A duplicate Add and an absent Remove change nothing.
    EXPECT_FALSE(s.Add(5));
    EXPECT_FALSE(s.Remove(99));
    EXPECT_EQ(s.getCountProperty(), 10);
}

TEST(SortedSetCountCacheTests, EmptyViewCountIsZero) {
    SortedSet<int> s = range(1, 10);

    SortedSet<int> aboveEverything = s.GetViewBetween(50, 60);
    EXPECT_EQ(aboveEverything.getCountProperty(), 0);
    EXPECT_TRUE(aboveEverything.getIsEmptyProperty());
    EXPECT_TRUE(aboveEverything.getIsViewProperty());

    SortedSet<int> belowEverything = s.GetViewBetween(-20, -10);
    EXPECT_EQ(belowEverything.getCountProperty(), 0);

    // An empty view over an empty set.
    SortedSet<int> nothing;
    SortedSet<int> viewOfNothing = nothing.GetViewBetween(1, 100);
    EXPECT_EQ(viewOfNothing.getCountProperty(), 0);
    EXPECT_TRUE(viewOfNothing.getIsEmptyProperty());
}

TEST(SortedSetCountCacheTests, OneElementViewCount) {
    SortedSet<int> s = range(1, 10);

    SortedSet<int> exact = s.GetViewBetween(4, 4);
    EXPECT_EQ(exact.getCountProperty(), 1);
    EXPECT_FALSE(exact.getIsEmptyProperty());

    // A one-element range whose bounds are not themselves members.
    SortedSet<int> gapped = s.GetViewBetween(4, 4);
    EXPECT_EQ(gapped.getCountProperty(), 1);

    SortedSet<int> sparse;
    sparse.Add(10);
    sparse.Add(20);
    SortedSet<int> straddling = sparse.GetViewBetween(15, 25);
    EXPECT_EQ(straddling.getCountProperty(), 1);
}

TEST(SortedSetCountCacheTests, MultiElementViewCountUsesInclusiveBounds) {
    SortedSet<int> s = range(1, 10);

    // Both bounds are members: inclusive on both ends.
    SortedSet<int> inner = s.GetViewBetween(3, 7);
    EXPECT_EQ(inner.getCountProperty(), 5);

    // Bounds outside the set clamp to the whole set.
    SortedSet<int> wide = s.GetViewBetween(-100, 100);
    EXPECT_EQ(wide.getCountProperty(), 10);

    // Bounds between members.
    SortedSet<int> evens;
    for (int i = 0; i <= 20; i += 2) evens.Add(i);
    EXPECT_EQ(evens.GetViewBetween(5, 11).getCountProperty(), 3);  // 6, 8, 10
    EXPECT_EQ(evens.GetViewBetween(6, 10).getCountProperty(), 3);  // 6, 8, 10
}

TEST(SortedSetCountCacheTests, ParentAddUpdatesViewCount) {
    SortedSet<int> s = range(1, 10);
    SortedSet<int> view = s.GetViewBetween(3, 7);
    ASSERT_EQ(view.getCountProperty(), 5);

    s.Add(100);                                   // out of the view's range
    EXPECT_EQ(view.getCountProperty(), 5);
    EXPECT_EQ(s.getCountProperty(), 11);

    s.Remove(4);
    s.Add(4);                                     // back in range, via the parent
    EXPECT_EQ(view.getCountProperty(), 5);

    SortedSet<int> sparse;
    sparse.Add(1);
    sparse.Add(9);
    SortedSet<int> gap = sparse.GetViewBetween(2, 8);
    ASSERT_EQ(gap.getCountProperty(), 0);
    sparse.Add(5);
    EXPECT_EQ(gap.getCountProperty(), 1);         // the cached 0 must not survive
}

TEST(SortedSetCountCacheTests, ParentRemoveUpdatesViewCount) {
    SortedSet<int> s = range(1, 10);
    SortedSet<int> view = s.GetViewBetween(3, 7);
    ASSERT_EQ(view.getCountProperty(), 5);

    s.Remove(5);
    EXPECT_EQ(view.getCountProperty(), 4);

    s.Remove(9);                                  // outside the view
    EXPECT_EQ(view.getCountProperty(), 4);
    EXPECT_EQ(s.getCountProperty(), 8);

    s.Clear();
    EXPECT_EQ(view.getCountProperty(), 0);
}

TEST(SortedSetCountCacheTests, ViewAddUpdatesParentAndViewCount) {
    SortedSet<int> s = range(1, 10);
    s.Remove(5);
    SortedSet<int> view = s.GetViewBetween(3, 7);
    ASSERT_EQ(view.getCountProperty(), 4);
    ASSERT_EQ(s.getCountProperty(), 9);

    EXPECT_TRUE(view.Add(5));
    EXPECT_EQ(view.getCountProperty(), 5);
    EXPECT_EQ(s.getCountProperty(), 10);

    // A rejected duplicate must not disturb either count.
    EXPECT_FALSE(view.Add(5));
    EXPECT_EQ(view.getCountProperty(), 5);
    EXPECT_EQ(s.getCountProperty(), 10);
}

TEST(SortedSetCountCacheTests, ViewRemoveUpdatesCount) {
    SortedSet<int> s = range(1, 10);
    SortedSet<int> view = s.GetViewBetween(3, 7);
    ASSERT_EQ(view.getCountProperty(), 5);

    EXPECT_TRUE(view.Remove(4));
    EXPECT_EQ(view.getCountProperty(), 4);
    EXPECT_EQ(s.getCountProperty(), 9);

    // Out-of-range Remove is a no-op that leaves both counts alone.
    EXPECT_FALSE(view.Remove(9));
    EXPECT_EQ(view.getCountProperty(), 4);
    EXPECT_EQ(s.getCountProperty(), 9);
}

TEST(SortedSetCountCacheTests, ViewClearUpdatesCount) {
    SortedSet<int> s = range(1, 10);
    SortedSet<int> view = s.GetViewBetween(3, 7);
    ASSERT_EQ(view.getCountProperty(), 5);

    view.Clear();
    EXPECT_EQ(view.getCountProperty(), 0);
    EXPECT_TRUE(view.getIsEmptyProperty());
    EXPECT_EQ(s.getCountProperty(), 5);           // 1, 2, 8, 9, 10

    // A second, no-op Clear must leave the (already correct) count alone.
    view.Clear();
    EXPECT_EQ(view.getCountProperty(), 0);
    EXPECT_EQ(s.getCountProperty(), 5);
}

TEST(SortedSetCountCacheTests, OverlappingViewsEachCountTheirOwnRange) {
    SortedSet<int> s = range(1, 20);
    SortedSet<int> left = s.GetViewBetween(1, 12);
    SortedSet<int> right = s.GetViewBetween(8, 20);
    ASSERT_EQ(left.getCountProperty(), 12);
    ASSERT_EQ(right.getCountProperty(), 13);

    // A mutation inside the overlap must be seen by both.
    s.Remove(10);
    EXPECT_EQ(left.getCountProperty(), 11);
    EXPECT_EQ(right.getCountProperty(), 12);

    // A mutation outside the overlap must be seen by exactly one.
    s.Remove(2);
    EXPECT_EQ(left.getCountProperty(), 10);
    EXPECT_EQ(right.getCountProperty(), 12);

    // And a write through one view is visible to the other.
    left.Add(10);
    EXPECT_EQ(left.getCountProperty(), 11);
    EXPECT_EQ(right.getCountProperty(), 13);
}

TEST(SortedSetCountCacheTests, NestedViewsEachCountTheirOwnRange) {
    SortedSet<int> s = range(1, 20);
    SortedSet<int> outer = s.GetViewBetween(5, 15);
    SortedSet<int> inner = outer.GetViewBetween(8, 12);
    ASSERT_EQ(outer.getCountProperty(), 11);
    ASSERT_EQ(inner.getCountProperty(), 5);

    inner.Remove(10);
    EXPECT_EQ(inner.getCountProperty(), 4);
    EXPECT_EQ(outer.getCountProperty(), 10);
    EXPECT_EQ(s.getCountProperty(), 19);

    outer.Add(10);
    EXPECT_EQ(inner.getCountProperty(), 5);
    EXPECT_EQ(outer.getCountProperty(), 11);
    EXPECT_EQ(s.getCountProperty(), 20);
}

TEST(SortedSetCountCacheTests, CopiedViewHandleCountsConsistently) {
    SortedSet<int> s = range(1, 10);
    SortedSet<int> view = s.GetViewBetween(3, 7);
    ASSERT_EQ(view.getCountProperty(), 5);

    SortedSet<int> copy = view;                    // another handle, same state and bounds
    EXPECT_TRUE(copy.getIsViewProperty());
    EXPECT_EQ(copy.getCountProperty(), 5);

    // A mutation through either handle is reflected by both, with no stale cache.
    view.Remove(4);
    EXPECT_EQ(view.getCountProperty(), 4);
    EXPECT_EQ(copy.getCountProperty(), 4);

    copy.Add(4);
    EXPECT_EQ(copy.getCountProperty(), 5);
    EXPECT_EQ(view.getCountProperty(), 5);

    // A copy taken AFTER the source cached its count starts from a cold cache and still agrees.
    SortedSet<int> lateCopy = view;
    EXPECT_EQ(lateCopy.getCountProperty(), 5);
}

TEST(SortedSetCountCacheTests, MovedViewHandleCountsConsistently) {
    SortedSet<int> s = range(1, 10);
    SortedSet<int> view = s.GetViewBetween(3, 7);
    ASSERT_EQ(view.getCountProperty(), 5);        // warm the cache before moving

    SortedSet<int> moved = std::move(view);
    EXPECT_TRUE(moved.getIsViewProperty());
    EXPECT_EQ(moved.getCountProperty(), 5);
    EXPECT_EQ(s.getCountProperty(), 10);

    // The moved-from object is a valid, EMPTY OWNING set -- never a view, never stale.
    EXPECT_FALSE(view.getIsViewProperty());       // NOLINT(bugprone-use-after-move)
    EXPECT_EQ(view.getCountProperty(), 0);
    EXPECT_TRUE(view.getIsEmptyProperty());

    // The moved-to handle stays live.
    s.Remove(4);
    EXPECT_EQ(moved.getCountProperty(), 4);
}

TEST(SortedSetCountCacheTests, ViewSurvivingParentDestructionCountsCorrectly) {
    SortedSet<int> view;
    {
        SortedSet<int> owner = range(1, 10);
        view = owner.GetViewBetween(3, 7);
        ASSERT_EQ(view.getCountProperty(), 5);
    }                                              // owner destroyed; the view keeps the state

    EXPECT_TRUE(view.getIsViewProperty());
    EXPECT_EQ(view.getCountProperty(), 5);

    // The surviving view is still a fully live handle onto its (now solely owned) state.
    EXPECT_TRUE(view.Remove(4));
    EXPECT_EQ(view.getCountProperty(), 4);
    EXPECT_TRUE(view.Add(4));
    EXPECT_EQ(view.getCountProperty(), 5);
}

TEST(SortedSetCountCacheTests, CustomComparerViewCount) {
    // std::less<Descending> sorts DESCENDING, so the "lower" bound is the numerically larger.
    SortedSet<Descending> s;
    for (int i = 1; i <= 10; ++i) s.Add(Descending{i});
    ASSERT_EQ(s.getCountProperty(), 10);

    SortedSet<Descending> view = s.GetViewBetween(Descending{7}, Descending{3});
    EXPECT_EQ(view.getCountProperty(), 5);        // 7, 6, 5, 4, 3

    s.Remove(Descending{5});
    EXPECT_EQ(view.getCountProperty(), 4);

    view.Add(Descending{5});
    EXPECT_EQ(view.getCountProperty(), 5);
    EXPECT_EQ(s.getCountProperty(), 10);

    view.Clear();
    EXPECT_EQ(view.getCountProperty(), 0);
    EXPECT_EQ(s.getCountProperty(), 5);
}

TEST(SortedSetCountCacheTests, LessOnlyElementTypeViewCount) {
    SortedSet<LessOnly> s;
    for (int i = 1; i <= 10; ++i) s.Add(LessOnly{i});

    SortedSet<LessOnly> view = s.GetViewBetween(LessOnly{3}, LessOnly{7});
    EXPECT_EQ(view.getCountProperty(), 5);

    s.Remove(LessOnly{5});
    EXPECT_EQ(view.getCountProperty(), 4);

    view.Add(LessOnly{5});
    EXPECT_EQ(view.getCountProperty(), 5);
    EXPECT_EQ(s.getCountProperty(), 10);
}

TEST(SortedSetCountCacheTests, StringSpecializationViewCount) {
    SortedSet<std::string> s;
    for (const char* w : {"alpha", "bravo", "charlie", "delta", "echo", "foxtrot"}) s.Add(w);
    ASSERT_EQ(s.getCountProperty(), 6);

    SortedSet<std::string> view = s.GetViewBetween("bravo", "echo");
    EXPECT_EQ(view.getCountProperty(), 4);        // bravo, charlie, delta, echo

    s.Remove("charlie");
    EXPECT_EQ(view.getCountProperty(), 3);

    view.Add("charlie");
    EXPECT_EQ(view.getCountProperty(), 4);
    EXPECT_EQ(s.getCountProperty(), 6);
}

TEST(SortedSetCountCacheTests, LargeBoundedRangeCount) {
    constexpr int kSize = 100000;
    SortedSet<int> s;
    for (int i = 0; i < kSize; ++i) s.Add(i);
    ASSERT_EQ(s.getCountProperty(), kSize);

    SortedSet<int> view = s.GetViewBetween(1000, 98999);
    EXPECT_EQ(view.getCountProperty(), 98000);

    // Re-reading a warm cache must give the identical answer.
    for (int k = 0; k < 5; ++k) EXPECT_EQ(view.getCountProperty(), 98000);

    s.Remove(50000);
    EXPECT_EQ(view.getCountProperty(), 97999);

    SortedSet<int> nested = view.GetViewBetween(2000, 2999);
    EXPECT_EQ(nested.getCountProperty(), 1000);
}

// =====================================================================================
// 2. Cache-sensitive behaviour
// =====================================================================================

TEST(SortedSetCountCacheTests, RepeatedCountCallsDoNotChangeObservableState) {
    SortedSet<int> s = range(1, 10);
    SortedSet<int> view = s.GetViewBetween(3, 7);

    const std::vector<int> before = view.ToVector();
    const int min = view.getMinProperty();
    const int max = view.getMaxProperty();

    for (int k = 0; k < 1000; ++k) ASSERT_EQ(view.getCountProperty(), 5);

    EXPECT_EQ(view.ToVector(), before);
    EXPECT_EQ(view.getMinProperty(), min);
    EXPECT_EQ(view.getMaxProperty(), max);
    EXPECT_EQ(s.getCountProperty(), 10);
    EXPECT_TRUE(view.getIsViewProperty());

    // Reading Count must not bump the shared version, so an in-flight enumerator survives it.
    auto it = view.begin();
    for (int k = 0; k < 100; ++k) (void)view.getCountProperty();
    EXPECT_EQ(*it, 3);
    EXPECT_NO_THROW(++it);
}

TEST(SortedSetCountCacheTests, CountIsExactAfterEverySupportedMutation) {
    SortedSet<int> s = range(1, 20);
    SortedSet<int> view = s.GetViewBetween(5, 15);
    ASSERT_EQ(view.getCountProperty(), 11);

    // Interleave every mutation entry point with a Count read, so no cached value can
    // outlive the mutation that should have superseded it.
    s.Add(21);                     EXPECT_EQ(view.getCountProperty(), 11);
    s.Remove(21);                  EXPECT_EQ(view.getCountProperty(), 11);
    s.Remove(5);                   EXPECT_EQ(view.getCountProperty(), 10);
    s.Add(5);                      EXPECT_EQ(view.getCountProperty(), 11);
    view.Remove(15);               EXPECT_EQ(view.getCountProperty(), 10);
    view.Add(15);                  EXPECT_EQ(view.getCountProperty(), 11);
    view.UnionWith(range(6, 9));   EXPECT_EQ(view.getCountProperty(), 11);
    view.ExceptWith(range(6, 7));  EXPECT_EQ(view.getCountProperty(), 9);
    view.UnionWith(range(6, 7));   EXPECT_EQ(view.getCountProperty(), 11);
    view.IntersectWith(range(5, 10));
    EXPECT_EQ(view.getCountProperty(), 6);
    view.SymmetricExceptWith(range(5, 6));
    EXPECT_EQ(view.getCountProperty(), 4);
    view.Clear();                  EXPECT_EQ(view.getCountProperty(), 0);

    EXPECT_EQ(s.getCountProperty(), 9);           // 1..4 and 16..20
}

TEST(SortedSetCountCacheTests, NoStaleCountAfterMutationThroughAnotherHandle) {
    SortedSet<int> s = range(1, 10);
    SortedSet<int> a = s.GetViewBetween(3, 7);
    SortedSet<int> b = a;

    // Warm every handle's cache, then mutate through a third path.
    ASSERT_EQ(a.getCountProperty(), 5);
    ASSERT_EQ(b.getCountProperty(), 5);
    ASSERT_EQ(s.getCountProperty(), 10);

    s.Remove(6);
    EXPECT_EQ(a.getCountProperty(), 4);
    EXPECT_EQ(b.getCountProperty(), 4);
    EXPECT_EQ(s.getCountProperty(), 9);

    // Warm again, mutate through `a`, and read through `b`.
    ASSERT_EQ(b.getCountProperty(), 4);
    a.Add(6);
    EXPECT_EQ(b.getCountProperty(), 5);
}

TEST(SortedSetCountCacheTests, CopyAssignmentDoesNotInheritAnInvalidCachedCount) {
    SortedSet<int> s = range(1, 10);
    SortedSet<int> big = s.GetViewBetween(1, 10);
    SortedSet<int> small = s.GetViewBetween(4, 5);
    ASSERT_EQ(big.getCountProperty(), 10);        // warm both caches to DIFFERENT values
    ASSERT_EQ(small.getCountProperty(), 2);

    big = small;                                  // rebind the warm handle
    EXPECT_EQ(big.getCountProperty(), 2);         // must not answer the stale 10

    // The reverse direction, and an owning set assigned over a warm view.
    SortedSet<int> other = range(100, 120);
    small = other;
    EXPECT_FALSE(small.getIsViewProperty());
    EXPECT_EQ(small.getCountProperty(), 21);

    // Self-assignment leaves a warm cache correct.
    SortedSet<int>& alias = big;
    big = alias;
    EXPECT_EQ(big.getCountProperty(), 2);
}

TEST(SortedSetCountCacheTests, MoveAssignmentDoesNotInheritAnInvalidCachedCount) {
    SortedSet<int> s = range(1, 10);
    SortedSet<int> big = s.GetViewBetween(1, 10);
    SortedSet<int> small = s.GetViewBetween(4, 5);
    ASSERT_EQ(big.getCountProperty(), 10);
    ASSERT_EQ(small.getCountProperty(), 2);

    big = std::move(small);
    EXPECT_EQ(big.getCountProperty(), 2);

    // The moved-from object became a valid, empty OWNING set with no inherited cache.
    EXPECT_FALSE(small.getIsViewProperty());      // NOLINT(bugprone-use-after-move)
    EXPECT_EQ(small.getCountProperty(), 0);

    // A warm view move-assigned over an owning set, and the reverse.
    SortedSet<int> owner = range(1, 4);
    ASSERT_EQ(owner.getCountProperty(), 4);
    owner = s.GetViewBetween(2, 8);
    EXPECT_TRUE(owner.getIsViewProperty());
    EXPECT_EQ(owner.getCountProperty(), 7);
}

TEST(SortedSetCountCacheTests, CopyConstructedHandleStartsFromAColdCache) {
    SortedSet<int> s = range(1, 10);
    SortedSet<int> view = s.GetViewBetween(3, 7);
    ASSERT_EQ(view.getCountProperty(), 5);        // warm

    // Mutate, then copy WITHOUT re-reading the source's Count, so the source's cache is stale
    // at the moment of the copy. The copy must still answer correctly.
    s.Remove(4);
    SortedSet<int> copy = view;
    EXPECT_EQ(copy.getCountProperty(), 4);
    EXPECT_EQ(view.getCountProperty(), 4);

    // An owning set deep-clones, so its copy counts its own independent state.
    SortedSet<int> ownerCopy = s;
    EXPECT_FALSE(ownerCopy.getIsViewProperty());
    EXPECT_EQ(ownerCopy.getCountProperty(), 9);
    s.Remove(1);
    EXPECT_EQ(ownerCopy.getCountProperty(), 9);   // unaffected: independent state
    EXPECT_EQ(s.getCountProperty(), 8);
}

TEST(SortedSetCountCacheTests, AlternatingMutationAndCountNeverReturnsAStaleValue) {
    SortedSet<int> s;
    SortedSet<int> view = s.GetViewBetween(0, 1000);

    intcs expected = 0;
    for (int i = 0; i < 200; ++i) {
        s.Add(i);
        ++expected;
        ASSERT_EQ(view.getCountProperty(), expected) << "after Add(" << i << ")";
    }
    for (int i = 0; i < 200; i += 2) {
        s.Remove(i);
        --expected;
        ASSERT_EQ(view.getCountProperty(), expected) << "after Remove(" << i << ")";
    }
    EXPECT_EQ(view.getCountProperty(), 100);
    EXPECT_EQ(s.getCountProperty(), 100);
}

TEST(SortedSetCountCacheTests, ConcurrentReadOnlyCountIsExactOnOneSharedViewObject) {
    // Deterministic by assertion, not by timing: with no thread mutating, every observation
    // must equal exactly 3001. Before ticket #1784 this same access pattern was a
    // ThreadSanitizer-confirmed data race on the view's plain mutable cache fields; the
    // instrumented reproduction lives in build-probe-sortedset/probe10_tsan_count_race.cpp.
    constexpr int kThreads = 8;
    constexpr int kReads = 500;
    constexpr intcs kExpected = 3001;

    SortedSet<int> s;
    for (int i = 0; i < 5000; ++i) s.Add(i);
    SortedSet<int> view = s.GetViewBetween(1000, 4000);
    ASSERT_EQ(view.getCountProperty(), kExpected);

    std::atomic<int> mismatches{0};
    std::vector<std::thread> workers;
    workers.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t) {
        workers.emplace_back([&view, &mismatches]() {
            for (int k = 0; k < kReads; ++k)
                if (view.getCountProperty() != kExpected)
                    mismatches.fetch_add(1, std::memory_order_relaxed);
        });
    }
    for (std::thread& w : workers) w.join();

    EXPECT_EQ(mismatches.load(), 0);
    EXPECT_EQ(view.getCountProperty(), kExpected);
    EXPECT_EQ(s.getCountProperty(), 5000);
}

TEST(SortedSetCountCacheTests, ConcurrentReadOnlyCountIsExactOnDistinctHandles) {
    constexpr int kThreads = 8;
    constexpr intcs kExpected = 3001;

    SortedSet<int> s;
    for (int i = 0; i < 5000; ++i) s.Add(i);
    SortedSet<int> view = s.GetViewBetween(1000, 4000);

    std::atomic<int> mismatches{0};
    std::vector<std::thread> workers;
    workers.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t) {
        workers.emplace_back([&view, &mismatches]() {
            SortedSet<int> own = view;                    // distinct handle, shared state
            SortedSet<int> nested = own.GetViewBetween(2000, 3000);
            for (int k = 0; k < 200; ++k) {
                if (own.getCountProperty() != kExpected)
                    mismatches.fetch_add(1, std::memory_order_relaxed);
                if (nested.getCountProperty() != 1001)
                    mismatches.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    for (std::thread& w : workers) w.join();

    EXPECT_EQ(mismatches.load(), 0);
    EXPECT_EQ(view.getCountProperty(), kExpected);
}

// =====================================================================================
// 3. Source and layout compatibility with ticket #1783
// =====================================================================================

// The exact public signatures ticket #1784 must not change. These are pointer-to-member type
// identities, so a changed return type, parameter list, or const-qualification is a compile
// error here rather than a silent downstream break.
static_assert(std::is_same_v<decltype(&SortedSet<int>::getCountProperty),
                             intcs (SortedSet<int>::*)() const>,
              "getCountProperty must stay a const member returning intcs by value");
static_assert(std::is_same_v<decltype(&SortedSet<int>::getIsEmptyProperty),
                             bool (SortedSet<int>::*)() const>);
static_assert(std::is_same_v<decltype(&SortedSet<int>::getIsViewProperty),
                             bool (SortedSet<int>::*)() const>);
static_assert(std::is_same_v<decltype(&SortedSet<int>::GetViewBetween),
                             SortedSet<int> (SortedSet<int>::*)(const int&, const int&)>,
              "GetViewBetween must stay non-const and return SortedSet<T> by value (#1783)");
static_assert(std::is_same_v<decltype(&SortedSet<int>::ToSortedSet),
                             SortedSet<int> (SortedSet<int>::*)() const>);
static_assert(std::is_same_v<decltype(&SortedSet<int>::IsWithinRange),
                             bool (SortedSet<int>::*)(const int&) const>);
static_assert(std::is_same_v<decltype(&SortedSet<int>::Contains),
                             bool (SortedSet<int>::*)(const int&) const>);
static_assert(std::is_same_v<decltype(&SortedSet<int>::Add), bool (SortedSet<int>::*)(const int&)>);
static_assert(std::is_same_v<decltype(&SortedSet<int>::Remove),
                             bool (SortedSet<int>::*)(const int&)>);
static_assert(std::is_same_v<decltype(&SortedSet<int>::Clear), void (SortedSet<int>::*)()>);
static_assert(std::is_same_v<decltype(&SortedSet<int>::getMinProperty),
                             int (SortedSet<int>::*)() const>);
static_assert(std::is_same_v<decltype(&SortedSet<int>::getMaxProperty),
                             int (SortedSet<int>::*)() const>);
static_assert(std::is_same_v<decltype(&SortedSet<int>::ToVector),
                             std::vector<int> (SortedSet<int>::*)() const>);

// The value-semantics traits ticket #1783 measured and published. Making the Count cache
// atomic would break all four if the atomics were exposed to the implicitly generated special
// members, so these are the portable half of the layout guarantee.
static_assert(std::is_copy_constructible_v<SortedSet<int>>);
static_assert(std::is_copy_assignable_v<SortedSet<int>>);
static_assert(std::is_nothrow_move_constructible_v<SortedSet<int>>);
static_assert(std::is_nothrow_move_assignable_v<SortedSet<int>>);
static_assert(!std::is_polymorphic_v<SortedSet<int>>);
static_assert(!std::is_trivially_copyable_v<SortedSet<int>>);
static_assert(std::is_copy_constructible_v<SortedSet<std::string>>);
static_assert(std::is_copy_assignable_v<SortedSet<std::string>>);
static_assert(std::is_nothrow_move_constructible_v<SortedSet<std::string>>);
static_assert(!std::is_polymorphic_v<SortedSet<std::string>>);

// The atomic cache must cost nothing in object size, which is what keeps #1783's published
// sizeof values intact. This assertion is portable; the exact byte counts are checked below
// only where they were measured.
static_assert(sizeof(std::atomic<intcs>) == sizeof(intcs),
              "an atomic Count cache wider than intcs would change SortedSet<T>'s layout");
static_assert(alignof(std::atomic<intcs>) == alignof(intcs));

TEST(SortedSetCountCacheTests, PublishedObjectLayoutIsUnchanged) {
    // Ticket #1783 published these exact sizes, measured on the verified Linux/GCC LP64
    // baseline (docs/SortedSetLiveViewDesign.md section 30.6). They are asserted at run time
    // and skipped elsewhere rather than static_asserted, because this repository keeps no
    // permanent architecture-specific sizeof assertions and must still compile for MinGW-w64
    // and Emscripten; the authoritative cross-check is the ABI probe
    // build-probe-sortedset/probe8_postfix_layout_symbols.cpp.
    if constexpr (sizeof(void*) != 8 || sizeof(std::size_t) != 8) {
        GTEST_SKIP() << "layout figures were published for LP64/LLP64 64-bit builds only";
    } else {
        EXPECT_EQ(sizeof(SortedSet<int>), 40u);
        EXPECT_EQ(sizeof(SortedSet<std::string>), 104u);
        EXPECT_EQ(sizeof(SortedSet<int>::Iterator), 40u);
        EXPECT_EQ(alignof(SortedSet<int>), 8u);
        EXPECT_EQ(alignof(SortedSet<std::string>), 8u);
    }
}

TEST(SortedSetCountCacheTests, CountCacheIsNotVisibleThroughThePublicSurface) {
    // The repair must remain an implementation detail: Count still answers an ordinary intcs,
    // comparable and assignable exactly as before, with no atomic leaking into the API.
    SortedSet<int> s = range(1, 10);
    SortedSet<int> view = s.GetViewBetween(3, 7);

    const intcs count = view.getCountProperty();
    static_assert(std::is_same_v<decltype(view.getCountProperty()), intcs>,
                  "Count must be returned by value as a plain intcs, not as an atomic");
    EXPECT_EQ(count, 5);

    intcs copied = count;
    copied += view.getCountProperty();
    EXPECT_EQ(copied, 10);
}
