// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Permanent regression suite for ticket #1783 (REMED-COLL-SORTEDSET-LIVE-VIEW), audit finding
// SR-AUD-361: System::Collections::Generic::SortedSet<T>::GetViewBetween must return a live,
// bounded, bidirectionally write-through view over the same underlying tree state -- .NET's
// TreeSubSet contract -- instead of the detached snapshot copy earlier revisions returned.
//
// The design record is docs/SortedSetLiveViewDesign.md; the cases below follow its section 21
// test plan and additionally pin the four adjacent defects folded into this ticket's scope by
// design section 4.6:
//   1. GetViewBetween required operator> although the class documents operator< only;
//   2. bounds were not enforced after view construction;
//   3. nested views could silently widen their parent view;
//   4. whole-object assignment defeated the fail-fast version guard (a silently wrong
//      dereference on copy-assign, an ASan-confirmed heap-use-after-free on move-assign).
//
// The three pre-existing GetViewBetween tests (LinkedListSortedSetTests.cpp:465,
// SortedStackTests.cpp:43, Ticket1713VersionTrackingTests.cpp:108) are deliberately left in
// place unchanged as the "still works" baseline; none of them asserted a snapshot property.
#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "System/ArgumentException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Collections/Generic/SortedSet.hpp"
#include "System/InvalidOperationException.hpp"

using System::Collections::Generic::SortedSet;

namespace {

/**
 * Element type providing ONLY operator< -- exactly the ordering contract SortedSet<T>'s class
 * doc-comment states. Before ticket #1783, instantiating GetViewBetween for such a type was a
 * hard compile error at SortedSet.hpp:297 and :300 ("no match for 'operator>'"). This type
 * existing in a compiled test file is itself the compile-level regression for that defect.
 */
struct LessOnly {
    int value = 0;
    bool operator<(const LessOnly& other) const { return value < other.value; }
};

/**
 * Element type whose ordering deliberately disagrees with natural int ordering: std::less on
 * it sorts DESCENDING. SortedSet<T> takes its ordering from std::set<T>::key_comp(), so this
 * is the strongest "custom comparer" this port's surface admits -- an IComparer<T> constructor
 * is an explicit non-goal of design section 26. It also defines no operator>, so every bound,
 * nested-view, lookup, Min/Max, and enumeration decision must route through the comparer.
 */
struct Descending {
    int value = 0;
    bool operator<(const Descending& other) const { return value > other.value; }
};

/**
 * Element type that counts how many times it is COPY-constructed. It exists so the scale
 * regression can prove GetViewBetween is O(1) in element copies rather than the O(k) copy the
 * snapshot implementation performed -- a wall-clock assertion would be flaky, this one is exact.
 */
struct Counted {
    static inline int copies = 0;
    int value = 0;

    Counted() = default;
    explicit Counted(int v) : value(v) {}
    Counted(const Counted& other) : value(other.value) { ++copies; }
    Counted(Counted&& other) noexcept : value(other.value) {}
    Counted& operator=(const Counted& other) { value = other.value; ++copies; return *this; }
    Counted& operator=(Counted&& other) noexcept { value = other.value; return *this; }
    ~Counted() = default;

    bool operator<(const Counted& other) const { return value < other.value; }
};

SortedSet<int> oneToTen() {
    SortedSet<int> s;
    for (int i = 1; i <= 10; ++i) s.Add(i);
    return s;
}

std::vector<int> collect(const SortedSet<int>& s) {
    std::vector<int> out;
    for (int x : s) out.push_back(x);
    return out;
}

} // namespace

// ---------------------------------------------------------------------------
// 1-3. Live propagation in both directions, and out-of-range invisibility
// ---------------------------------------------------------------------------

TEST(SortedSetLiveViewTests, ParentMutationIsVisibleInView) {
    SortedSet<int> parent{1, 2, 3, 4, 6, 7, 8, 9, 10};
    SortedSet<int> view = parent.GetViewBetween(3, 7);
    EXPECT_EQ(view.getCountProperty(), 4);

    EXPECT_TRUE(parent.Add(5));
    EXPECT_EQ(view.getCountProperty(), 5);
    EXPECT_TRUE(view.Contains(5));
    EXPECT_EQ(view.getMinProperty(), 3);
    EXPECT_EQ(view.getMaxProperty(), 7);
    EXPECT_EQ(collect(view), (std::vector<int>{3, 4, 5, 6, 7}));

    EXPECT_TRUE(parent.Remove(4));
    EXPECT_EQ(view.getCountProperty(), 4);
    EXPECT_FALSE(view.Contains(4));
    EXPECT_EQ(collect(view), (std::vector<int>{3, 5, 6, 7}));
}

TEST(SortedSetLiveViewTests, ViewMutationIsVisibleInParent) {
    SortedSet<int> parent{1, 2, 3, 4, 6, 7, 8, 9, 10};
    SortedSet<int> view = parent.GetViewBetween(3, 7);

    EXPECT_TRUE(view.Add(5));
    EXPECT_TRUE(parent.Contains(5));
    EXPECT_EQ(parent.getCountProperty(), 10);

    EXPECT_TRUE(view.Remove(4));
    EXPECT_FALSE(parent.Contains(4));
    EXPECT_EQ(parent.getCountProperty(), 9);
}

TEST(SortedSetLiveViewTests, OutOfRangeParentMutationIsInvisibleInView) {
    SortedSet<int> parent = oneToTen();
    SortedSet<int> view = parent.GetViewBetween(3, 7);
    const auto before = view.getCountProperty();

    EXPECT_TRUE(parent.Add(99));
    EXPECT_TRUE(parent.Remove(1));
    EXPECT_EQ(view.getCountProperty(), before);
    EXPECT_FALSE(view.Contains(99));
    EXPECT_EQ(view.getMaxProperty(), 7);
    EXPECT_EQ(collect(view), (std::vector<int>{3, 4, 5, 6, 7}));
}

// ---------------------------------------------------------------------------
// 4-6. Bounds enforcement after construction (adjacent defect 2)
// ---------------------------------------------------------------------------

TEST(SortedSetLiveViewTests, OutOfRangeAddThrowsAndWritesNothing) {
    SortedSet<int> parent = oneToTen();
    SortedSet<int> view = parent.GetViewBetween(3, 7);

    try {
        view.Add(99);
        FAIL() << "an out-of-range Add on a view must throw";
    } catch (const System::ArgumentOutOfRangeException& ex) {
        EXPECT_EQ(ex.getParamNameProperty(), "item");
    }
    EXPECT_THROW(view.Add(2), System::ArgumentOutOfRangeException);

    EXPECT_FALSE(parent.Contains(99));
    EXPECT_EQ(parent.getCountProperty(), 10);
    EXPECT_EQ(view.getCountProperty(), 5);
    EXPECT_EQ(view.getMaxProperty(), 7);
}

TEST(SortedSetLiveViewTests, OutOfRangeRemoveReturnsFalseWithoutThrowing) {
    SortedSet<int> parent = oneToTen();
    SortedSet<int> view = parent.GetViewBetween(3, 7);

    EXPECT_NO_THROW({
        EXPECT_FALSE(view.Remove(10));
        EXPECT_FALSE(view.Remove(1));
        EXPECT_FALSE(view.Remove(42));
    });
    EXPECT_TRUE(parent.Contains(10));
    EXPECT_TRUE(parent.Contains(1));
    EXPECT_EQ(parent.getCountProperty(), 10);
}

TEST(SortedSetLiveViewTests, OutOfRangeContainsIsFalseEvenWhenTheParentHasIt) {
    SortedSet<int> parent = oneToTen();
    SortedSet<int> view = parent.GetViewBetween(3, 7);

    EXPECT_TRUE(parent.Contains(2));
    EXPECT_TRUE(parent.Contains(8));
    EXPECT_FALSE(view.Contains(2));
    EXPECT_FALSE(view.Contains(8));
    EXPECT_TRUE(view.Contains(3));
    EXPECT_TRUE(view.Contains(7));
}

// ---------------------------------------------------------------------------
// 7. Range-scoped Clear
// ---------------------------------------------------------------------------

TEST(SortedSetLiveViewTests, ViewClearRemovesExactlyItsRange) {
    SortedSet<int> parent = oneToTen();
    SortedSet<int> view = parent.GetViewBetween(3, 7);

    view.Clear();
    EXPECT_EQ(view.getCountProperty(), 0);
    EXPECT_TRUE(view.getIsEmptyProperty());
    EXPECT_EQ(parent.getCountProperty(), 5);
    EXPECT_EQ(parent.ToVector(), (std::vector<int>{1, 2, 8, 9, 10}));

    // The emptied view still enforces its bounds and still writes through.
    EXPECT_THROW(view.Add(99), System::ArgumentOutOfRangeException);
    EXPECT_TRUE(view.Add(4));
    EXPECT_TRUE(parent.Contains(4));
}

TEST(SortedSetLiveViewTests, FullSetClearClearsEverything) {
    SortedSet<int> parent = oneToTen();
    SortedSet<int> view = parent.GetViewBetween(3, 7);
    parent.Clear();
    EXPECT_EQ(parent.getCountProperty(), 0);
    EXPECT_EQ(view.getCountProperty(), 0);
}

// ---------------------------------------------------------------------------
// 8-10. Bound inclusivity, invalid range, disjoint range
// ---------------------------------------------------------------------------

TEST(SortedSetLiveViewTests, BoundsAreInclusiveOnBothEnds) {
    SortedSet<int> parent = oneToTen();
    SortedSet<int> view = parent.GetViewBetween(3, 7);

    EXPECT_TRUE(view.Contains(3));
    EXPECT_TRUE(view.Contains(7));
    EXPECT_TRUE(view.IsWithinRange(3));
    EXPECT_TRUE(view.IsWithinRange(7));
    // The values immediately outside each bound.
    EXPECT_FALSE(view.IsWithinRange(2));
    EXPECT_FALSE(view.IsWithinRange(8));
    EXPECT_FALSE(view.Contains(2));
    EXPECT_FALSE(view.Contains(8));
    EXPECT_EQ(view.getMinProperty(), 3);
    EXPECT_EQ(view.getMaxProperty(), 7);
}

TEST(SortedSetLiveViewTests, SingleElementRangeIsValid) {
    SortedSet<int> parent = oneToTen();
    SortedSet<int> view = parent.GetViewBetween(3, 3);
    EXPECT_EQ(view.getCountProperty(), 1);
    EXPECT_EQ(view.getMinProperty(), 3);
    EXPECT_EQ(view.getMaxProperty(), 3);
    EXPECT_THROW(view.Add(4), System::ArgumentOutOfRangeException);
}

TEST(SortedSetLiveViewTests, InvalidRangeThrowsWithTheDotNetMessage) {
    SortedSet<int> parent = oneToTen();
    try {
        (void)parent.GetViewBetween(7, 3);
        FAIL() << "an inverted range must throw";
    } catch (const System::ArgumentException& ex) {
        // .NET's SR.SortedSet_LowerValueGreaterThanUpperValue, with the "(Parameter 'x')"
        // suffix appended exactly once since ticket #1776.
        EXPECT_STREQ(ex.what(), "Must be less than or equal to upperValue. (Parameter 'lowerValue')");
        EXPECT_EQ(ex.getParamNameProperty(), "lowerValue");
    }
}

TEST(SortedSetLiveViewTests, DisjointRangeIsAValidEmptyLiveView) {
    SortedSet<int> parent = oneToTen();
    SortedSet<int> view = parent.GetViewBetween(100, 200);
    EXPECT_TRUE(view.getIsViewProperty());
    EXPECT_EQ(view.getCountProperty(), 0);
    EXPECT_EQ(view.getMinProperty(), 0);
    EXPECT_EQ(view.getMaxProperty(), 0);
    EXPECT_THROW(view.Add(5), System::ArgumentOutOfRangeException);

    EXPECT_TRUE(view.Add(150));
    EXPECT_TRUE(parent.Contains(150));
    EXPECT_EQ(view.getCountProperty(), 1);
}

// ---------------------------------------------------------------------------
// 11-13. Nested and overlapping views (adjacent defect 3)
// ---------------------------------------------------------------------------

TEST(SortedSetLiveViewTests, NestedViewNarrowsAndWritesThrough) {
    SortedSet<int> parent = oneToTen();
    SortedSet<int> outer = parent.GetViewBetween(3, 7);
    SortedSet<int> inner = outer.GetViewBetween(4, 6);

    EXPECT_TRUE(inner.getIsViewProperty());
    EXPECT_EQ(inner.getCountProperty(), 3);
    EXPECT_EQ(collect(inner), (std::vector<int>{4, 5, 6}));

    EXPECT_TRUE(inner.Remove(5));
    EXPECT_FALSE(outer.Contains(5));
    EXPECT_FALSE(parent.Contains(5));

    EXPECT_TRUE(inner.Add(5));
    EXPECT_TRUE(outer.Contains(5));
    EXPECT_TRUE(parent.Contains(5));
    EXPECT_THROW(inner.Add(7), System::ArgumentOutOfRangeException);
}

TEST(SortedSetLiveViewTests, NestedViewMayNotWidenEitherBound) {
    SortedSet<int> parent = oneToTen();
    SortedSet<int> view = parent.GetViewBetween(3, 7);

    try {
        (void)view.GetViewBetween(1, 6);
        FAIL() << "widening the lower bound must throw";
    } catch (const System::ArgumentOutOfRangeException& ex) {
        EXPECT_EQ(ex.getParamNameProperty(), "lowerValue");
    }
    try {
        (void)view.GetViewBetween(4, 9);
        FAIL() << "widening the upper bound must throw";
    } catch (const System::ArgumentOutOfRangeException& ex) {
        EXPECT_EQ(ex.getParamNameProperty(), "upperValue");
    }
    // Re-requesting the same bounds is narrowing-or-equal, hence legal.
    EXPECT_NO_THROW((void)view.GetViewBetween(3, 7));
}

TEST(SortedSetLiveViewTests, NestedViewIsFlattenedNotChained) {
    SortedSet<int> parent = oneToTen();
    SortedSet<int> outer = parent.GetViewBetween(3, 7);
    SortedSet<int> inner = outer.GetViewBetween(4, 6);

    // Flattened to depth 1: destroying the intermediate handle changes nothing.
    { SortedSet<int> discard = std::move(outer); (void)discard.getCountProperty(); }
    EXPECT_EQ(inner.getCountProperty(), 3);
    EXPECT_TRUE(inner.Remove(5));
    EXPECT_FALSE(parent.Contains(5));
}

TEST(SortedSetLiveViewTests, NestedClearRemovesOnlyTheInnerRange) {
    SortedSet<int> parent = oneToTen();
    SortedSet<int> outer = parent.GetViewBetween(3, 7);
    SortedSet<int> inner = outer.GetViewBetween(4, 6);

    inner.Clear();
    EXPECT_EQ(inner.getCountProperty(), 0);
    EXPECT_EQ(collect(outer), (std::vector<int>{3, 7}));
    EXPECT_EQ(parent.ToVector(), (std::vector<int>{1, 2, 3, 7, 8, 9, 10}));
}

TEST(SortedSetLiveViewTests, OverlappingViewsObserveEachOther) {
    SortedSet<int> parent = oneToTen();
    SortedSet<int> a = parent.GetViewBetween(2, 6);
    SortedSet<int> b = parent.GetViewBetween(4, 8);

    EXPECT_TRUE(a.Remove(5));
    EXPECT_FALSE(b.Contains(5));
    EXPECT_TRUE(b.Add(5));
    EXPECT_TRUE(a.Contains(5));

    // Disjoint views share the state but not the visible values.
    SortedSet<int> c = parent.GetViewBetween(1, 2);
    SortedSet<int> d = parent.GetViewBetween(9, 10);
    EXPECT_TRUE(c.Remove(1));
    EXPECT_EQ(d.getCountProperty(), 2);
    EXPECT_FALSE(d.Contains(1));
}

TEST(SortedSetLiveViewTests, RepeatedGetViewBetweenYieldsDistinctLiveWrappers) {
    SortedSet<int> parent = oneToTen();
    SortedSet<int> first = parent.GetViewBetween(3, 7);
    SortedSet<int> second = parent.GetViewBetween(3, 7);

    EXPECT_NE(&first, &second);
    EXPECT_TRUE(first.Remove(4));
    EXPECT_FALSE(second.Contains(4));
    EXPECT_TRUE(second.Add(4));
    EXPECT_TRUE(first.Contains(4));
}

// ---------------------------------------------------------------------------
// 14-15. Lifetime: a view and an iterator each keep the state alive
// ---------------------------------------------------------------------------

TEST(SortedSetLiveViewTests, ViewOutlivesTheObjectItCameFrom) {
    SortedSet<int> view;
    {
        SortedSet<int> parent = oneToTen();
        view = parent.GetViewBetween(3, 7);
    }
    EXPECT_EQ(view.getCountProperty(), 5);
    EXPECT_EQ(collect(view), (std::vector<int>{3, 4, 5, 6, 7}));
    EXPECT_TRUE(view.Remove(5));
    EXPECT_EQ(view.getCountProperty(), 4);
    EXPECT_THROW(view.Add(99), System::ArgumentOutOfRangeException);
}

TEST(SortedSetLiveViewTests, IteratorOutlivesTheObjectItCameFrom) {
    SortedSet<int>::Iterator it = SortedSet<int>().begin();
    SortedSet<int>::Iterator last = it;
    {
        SortedSet<int> s{1, 2, 3};
        it = s.begin();
        last = s.end();
    }
    std::vector<int> seen;
    for (; it != last; ++it) seen.push_back(*it);
    EXPECT_EQ(seen, (std::vector<int>{1, 2, 3}));
}

// ---------------------------------------------------------------------------
// 16-20. Copy, move, and assignment (adjacent defect 4)
// ---------------------------------------------------------------------------

TEST(SortedSetLiveViewTests, FullSetCopyIsAnIndependentDeepClone) {
    SortedSet<int> parent = oneToTen();
    SortedSet<int> view = parent.GetViewBetween(3, 7);
    SortedSet<int> clone = parent;

    EXPECT_FALSE(clone.getIsViewProperty());
    EXPECT_EQ(clone.getCountProperty(), 10);

    EXPECT_TRUE(clone.Add(99));
    EXPECT_TRUE(clone.Remove(5));
    EXPECT_FALSE(parent.Contains(99));
    EXPECT_TRUE(parent.Contains(5));
    EXPECT_TRUE(view.Contains(5));

    EXPECT_TRUE(parent.Remove(6));
    EXPECT_TRUE(clone.Contains(6));
}

TEST(SortedSetLiveViewTests, ViewCopyIsAnotherLiveHandle) {
    SortedSet<int> parent = oneToTen();
    SortedSet<int> view = parent.GetViewBetween(3, 7);
    SortedSet<int> viewCopy = view;

    EXPECT_TRUE(viewCopy.getIsViewProperty());
    EXPECT_EQ(viewCopy.getCountProperty(), 5);

    EXPECT_TRUE(viewCopy.Remove(4));
    EXPECT_FALSE(view.Contains(4));
    EXPECT_FALSE(parent.Contains(4));
    EXPECT_THROW(viewCopy.Add(99), System::ArgumentOutOfRangeException);
}

TEST(SortedSetLiveViewTests, FullSetMoveTransfersStateAndLeavesAValidEmptyOwningSet) {
    SortedSet<int> parent = oneToTen();
    SortedSet<int> view = parent.GetViewBetween(3, 7);
    SortedSet<int> moved = std::move(parent);

    EXPECT_EQ(moved.getCountProperty(), 10);
    EXPECT_FALSE(moved.getIsViewProperty());
    // NOLINTNEXTLINE(bugprone-use-after-move) -- the moved-from contract is under test.
    EXPECT_EQ(parent.getCountProperty(), 0);
    EXPECT_FALSE(parent.getIsViewProperty());
    EXPECT_TRUE(parent.Add(1234));
    EXPECT_FALSE(moved.Contains(1234));

    // The view followed the state and now observes mutations made through the destination.
    EXPECT_EQ(view.getCountProperty(), 5);
    EXPECT_TRUE(moved.Remove(4));
    EXPECT_FALSE(view.Contains(4));
}

TEST(SortedSetLiveViewTests, ViewMoveKeepsViewnessAndLeavesAValidEmptyOwningSet) {
    SortedSet<int> parent = oneToTen();
    SortedSet<int> view = parent.GetViewBetween(3, 7);
    SortedSet<int> movedView = std::move(view);

    EXPECT_TRUE(movedView.getIsViewProperty());
    EXPECT_EQ(movedView.getCountProperty(), 5);
    EXPECT_THROW(movedView.Add(99), System::ArgumentOutOfRangeException);
    EXPECT_TRUE(movedView.Remove(4));
    EXPECT_FALSE(parent.Contains(4));

    // NOLINTNEXTLINE(bugprone-use-after-move) -- the moved-from contract is under test.
    EXPECT_FALSE(view.getIsViewProperty());
    EXPECT_EQ(view.getCountProperty(), 0);
    EXPECT_TRUE(view.Add(99));
    EXPECT_FALSE(parent.Contains(99));
}

TEST(SortedSetLiveViewTests, CopyAssignRebindsWithoutDisturbingViewsOrIterators) {
    SortedSet<int> parent = oneToTen();
    SortedSet<int> view = parent.GetViewBetween(3, 7);
    SortedSet<int> other{100, 200};

    SortedSet<int>::Iterator it = parent.begin();
    parent = other;

    EXPECT_EQ(parent.getCountProperty(), 2);
    // The view keeps the abandoned state alive and keeps observing it.
    EXPECT_EQ(view.getCountProperty(), 5);
    EXPECT_TRUE(view.Contains(4));
    // The iterator still walks the pre-assignment contents; before #1783 this silently
    // yielded an element of the NEW tree with no diagnostic at all.
    EXPECT_EQ(*it, 1);
    ++it;
    EXPECT_EQ(*it, 2);

    EXPECT_TRUE(parent.Add(300));
    EXPECT_FALSE(view.Contains(300));
    EXPECT_EQ(other.getCountProperty(), 2);
}

TEST(SortedSetLiveViewTests, MoveAssignRebindsWithoutUseAfterFree) {
    // Direct regression for the ASan-confirmed heap-use-after-free design section 3.2
    // measured: move assignment freed the tree the outstanding iterator pointed into.
    SortedSet<int> parent = oneToTen();
    SortedSet<int> view = parent.GetViewBetween(3, 7);
    SortedSet<int>::Iterator it = parent.begin();

    parent = SortedSet<int>{100, 200};

    EXPECT_EQ(parent.getCountProperty(), 2);
    EXPECT_EQ(view.getCountProperty(), 5);
    EXPECT_EQ(*it, 1);
    ++it;
    EXPECT_EQ(*it, 2);
    EXPECT_TRUE(view.Remove(3));
    EXPECT_EQ(view.getCountProperty(), 4);
}

TEST(SortedSetLiveViewTests, ViewAssignmentRebindsTheHandle) {
    SortedSet<int> parent = oneToTen();
    SortedSet<int> view = parent.GetViewBetween(3, 7);
    SortedSet<int> narrow = parent.GetViewBetween(4, 5);

    view = narrow;                       // view copy-assign: another handle, narrower bounds
    EXPECT_TRUE(view.getIsViewProperty());
    EXPECT_EQ(view.getCountProperty(), 2);
    EXPECT_THROW(view.Add(6), System::ArgumentOutOfRangeException);
    EXPECT_TRUE(view.Remove(4));
    EXPECT_FALSE(parent.Contains(4));

    view = SortedSet<int>{7, 8, 9};      // view move-assign from an owning set: role changes
    EXPECT_FALSE(view.getIsViewProperty());
    EXPECT_EQ(view.getCountProperty(), 3);
    EXPECT_TRUE(view.Add(1000));
    EXPECT_FALSE(parent.Contains(1000));
}

TEST(SortedSetLiveViewTests, SelfAssignmentIsSafe) {
    SortedSet<int> s = oneToTen();
    SortedSet<int>& alias = s;
    s = alias;                                       // self copy-assign
    EXPECT_EQ(s.getCountProperty(), 10);
    s = std::move(alias);                            // self move-assign
    EXPECT_EQ(s.getCountProperty(), 10);
    EXPECT_TRUE(s.Contains(1));
    EXPECT_TRUE(s.Contains(10));

    SortedSet<int> view = s.GetViewBetween(3, 7);
    SortedSet<int>& viewAlias = view;
    view = viewAlias;
    EXPECT_TRUE(view.getIsViewProperty());
    EXPECT_EQ(view.getCountProperty(), 5);
    view = std::move(viewAlias);
    EXPECT_TRUE(view.getIsViewProperty());
    EXPECT_EQ(view.getCountProperty(), 5);
}

TEST(SortedSetLiveViewTests, AssignmentBetweenWrappersSharingOneStateIsSafe) {
    SortedSet<int> parent = oneToTen();
    SortedSet<int> a = parent.GetViewBetween(2, 6);
    SortedSet<int> b = parent.GetViewBetween(4, 8);

    a = b;                                            // distinct objects, one shared State
    EXPECT_TRUE(a.getIsViewProperty());
    EXPECT_EQ(a.getCountProperty(), 5);
    EXPECT_TRUE(a.Remove(8));
    EXPECT_FALSE(b.Contains(8));
    EXPECT_FALSE(parent.Contains(8));

    parent = a;                                       // owning set := view over its own state
    EXPECT_TRUE(parent.getIsViewProperty());
    EXPECT_EQ(parent.getCountProperty(), 4);
    EXPECT_EQ(collect(parent), (std::vector<int>{4, 5, 6, 7}));
}

TEST(SortedSetLiveViewTests, CopyAssignCannotMakeAStaleIteratorSilentlyValid) {
    SortedSet<int> a{1, 2, 3};
    SortedSet<int> b{10, 20, 30};
    SortedSet<int>::Iterator it = a.begin();

    a.Add(4);                       // bumps a's version -> `it` is stale for a's state
    EXPECT_THROW((void)*it, System::InvalidOperationException);

    a = b;                          // rebinding a must not reset the guard for `it`
    EXPECT_THROW((void)*it, System::InvalidOperationException);
    EXPECT_THROW(++it, System::InvalidOperationException);
}

// ---------------------------------------------------------------------------
// 21-22. Enumeration and the single shared version counter
// ---------------------------------------------------------------------------

TEST(SortedSetLiveViewTests, EnumerationYieldsOnlyInRangeValues) {
    SortedSet<int> parent = oneToTen();
    SortedSet<int> view = parent.GetViewBetween(3, 7);
    EXPECT_EQ(collect(view), (std::vector<int>{3, 4, 5, 6, 7}));
    EXPECT_EQ(view.ToVector(), (std::vector<int>{3, 4, 5, 6, 7}));

    const SortedSet<int>& constView = view;           // const iteration
    std::vector<int> seen;
    for (int x : constView) seen.push_back(x);
    EXPECT_EQ(seen, (std::vector<int>{3, 4, 5, 6, 7}));

    SortedSet<int> empty = parent.GetViewBetween(100, 200);
    EXPECT_TRUE(empty.begin() == empty.end());
}

TEST(SortedSetLiveViewTests, ParentMutationInvalidatesAViewsIterator) {
    SortedSet<int> parent = oneToTen();
    SortedSet<int> view = parent.GetViewBetween(3, 7);
    EXPECT_THROW({
        for (int x : view) { (void)x; parent.Add(99); }
    }, System::InvalidOperationException);
}

TEST(SortedSetLiveViewTests, ViewMutationInvalidatesTheParentsIterator) {
    SortedSet<int> parent = oneToTen();
    SortedSet<int> view = parent.GetViewBetween(3, 7);
    EXPECT_THROW({
        for (int x : parent) { (void)x; view.Remove(5); }
    }, System::InvalidOperationException);
}

TEST(SortedSetLiveViewTests, OneViewInvalidatesAnotherViewsIterator) {
    SortedSet<int> parent = oneToTen();
    SortedSet<int> a = parent.GetViewBetween(1, 4);
    SortedSet<int> b = parent.GetViewBetween(7, 10);   // deliberately disjoint from a
    EXPECT_THROW({
        for (int x : a) { (void)x; b.Remove(9); }
    }, System::InvalidOperationException);
}

TEST(SortedSetLiveViewTests, SelfMutationDuringEnumerationStillThrows) {
    SortedSet<int> parent = oneToTen();
    SortedSet<int> view = parent.GetViewBetween(3, 7);
    EXPECT_THROW({
        for (int x : view) { (void)x; view.Remove(4); }
    }, System::InvalidOperationException);
}

TEST(SortedSetLiveViewTests, IndependentCopiedStateDoesNotInvalidateTheSourceIterator) {
    SortedSet<int> a = oneToTen();
    SortedSet<int> clone = a;
    EXPECT_NO_THROW({
        int sum = 0;
        for (int x : a) { sum += x; clone.Add(99); clone.Remove(1); }
        EXPECT_EQ(sum, 55);
    });
}

TEST(SortedSetLiveViewTests, RejectedDuplicateAddAndAbsentRemoveDoNotInvalidate) {
    SortedSet<int> parent = oneToTen();
    SortedSet<int> view = parent.GetViewBetween(3, 7);
    EXPECT_NO_THROW({
        for (int x : view) {
            (void)x;
            EXPECT_FALSE(parent.Add(4));      // already present
            EXPECT_FALSE(parent.Remove(4242));// absent
            EXPECT_FALSE(view.Remove(10));    // out of range
        }
    });
    // A no-op Clear on an out-of-range view likewise must not bump the version.
    SortedSet<int> emptyView = parent.GetViewBetween(100, 200);
    EXPECT_NO_THROW({
        for (int x : view) { (void)x; emptyView.Clear(); }
    });
}

// ---------------------------------------------------------------------------
// 23-25. Set algebra
// ---------------------------------------------------------------------------

TEST(SortedSetLiveViewTests, SetAlgebraThroughAViewWritesThroughAndEnforcesBounds) {
    {
        SortedSet<int> parent = oneToTen();
        SortedSet<int> view = parent.GetViewBetween(3, 7);
        SortedSet<int> other{5, 6, 99};
        EXPECT_THROW(view.UnionWith(other), System::ArgumentOutOfRangeException);
        EXPECT_FALSE(parent.Contains(99));
    }
    {
        SortedSet<int> parent{1, 2, 3, 6, 7, 8};
        SortedSet<int> view = parent.GetViewBetween(3, 7);
        SortedSet<int> other{4, 5};
        view.UnionWith(other);
        EXPECT_EQ(parent.ToVector(), (std::vector<int>{1, 2, 3, 4, 5, 6, 7, 8}));
    }
    {
        SortedSet<int> parent = oneToTen();
        SortedSet<int> view = parent.GetViewBetween(3, 7);
        SortedSet<int> other{4, 6, 99};
        view.IntersectWith(other);
        EXPECT_EQ(collect(view), (std::vector<int>{4, 6}));
        EXPECT_EQ(parent.ToVector(), (std::vector<int>{1, 2, 4, 6, 8, 9, 10}));
    }
    {
        SortedSet<int> parent = oneToTen();
        SortedSet<int> view = parent.GetViewBetween(3, 7);
        SortedSet<int> other{1, 4, 5, 10};   // out-of-range members are simply not removed
        view.ExceptWith(other);
        EXPECT_EQ(collect(view), (std::vector<int>{3, 6, 7}));
        EXPECT_TRUE(parent.Contains(1));
        EXPECT_TRUE(parent.Contains(10));
    }
    {
        SortedSet<int> parent{1, 2, 3, 4, 8, 9};
        SortedSet<int> view = parent.GetViewBetween(3, 7);
        SortedSet<int> other{4, 5};
        view.SymmetricExceptWith(other);
        EXPECT_EQ(collect(view), (std::vector<int>{3, 5}));
        EXPECT_EQ(parent.ToVector(), (std::vector<int>{1, 2, 3, 5, 8, 9}));
    }
}

TEST(SortedSetLiveViewTests, SetAlgebraWithSharedStateAliases) {
    {   // The parent minus one of its own views.
        SortedSet<int> parent{1, 2, 3, 4, 5};
        SortedSet<int> view = parent.GetViewBetween(2, 4);
        parent.ExceptWith(view);
        EXPECT_EQ(parent.ToVector(), (std::vector<int>{1, 5}));
        EXPECT_EQ(view.getCountProperty(), 0);
    }
    {   // A view against a handle onto the same state with identical bounds.
        SortedSet<int> parent{1, 2, 3, 4, 5};
        SortedSet<int> view = parent.GetViewBetween(2, 4);
        SortedSet<int> sameView = view;
        view.SymmetricExceptWith(sameView);
        EXPECT_EQ(view.getCountProperty(), 0);
        EXPECT_EQ(parent.ToVector(), (std::vector<int>{1, 5}));
    }
    {   // Distinct objects, one state, different bounds.
        SortedSet<int> parent = oneToTen();
        SortedSet<int> a = parent.GetViewBetween(2, 6);
        SortedSet<int> b = parent.GetViewBetween(4, 8);
        a.ExceptWith(b);
        EXPECT_EQ(collect(a), (std::vector<int>{2, 3}));
        EXPECT_EQ(parent.ToVector(), (std::vector<int>{1, 2, 3, 7, 8, 9, 10}));
    }
    {   // Intersecting a set with one of its own views keeps only the view's range.
        SortedSet<int> parent = oneToTen();
        SortedSet<int> view = parent.GetViewBetween(4, 6);
        parent.IntersectWith(view);
        EXPECT_EQ(parent.ToVector(), (std::vector<int>{4, 5, 6}));
    }
    {   // Union of a set with its own view is a no-op, not an invalidated walk.
        SortedSet<int> parent = oneToTen();
        SortedSet<int> view = parent.GetViewBetween(4, 6);
        parent.UnionWith(view);
        EXPECT_EQ(parent.getCountProperty(), 10);
    }
    {   // The pre-#1783 object-identity regressions still hold.
        SortedSet<int> a{1, 2, 3, 4, 5};
        a.ExceptWith(a);
        EXPECT_EQ(a.getCountProperty(), 0);
        SortedSet<int> b{1, 2, 3, 4, 5};
        b.SymmetricExceptWith(b);
        EXPECT_EQ(b.getCountProperty(), 0);
        SortedSet<int> c{1, 2, 3};
        c.IntersectWith(c);
        EXPECT_EQ(c.getCountProperty(), 3);
    }
}

TEST(SortedSetLiveViewTests, ReadOnlyPredicatesAreRangeAware) {
    SortedSet<int> parent = oneToTen();
    SortedSet<int> view = parent.GetViewBetween(3, 5);
    SortedSet<int> equal{3, 4, 5};
    SortedSet<int> superset{2, 3, 4, 5, 6};

    EXPECT_TRUE(view.SetEquals(equal));
    EXPECT_TRUE(equal.SetEquals(view));
    EXPECT_FALSE(view.SetEquals(parent));
    EXPECT_TRUE(view.IsSubsetOf(parent));
    EXPECT_TRUE(view.IsSubsetOf(superset));
    EXPECT_FALSE(parent.IsSubsetOf(view));
    EXPECT_TRUE(parent.IsSupersetOf(view));
    EXPECT_TRUE(view.IsProperSubsetOf(superset));
    EXPECT_FALSE(view.IsProperSubsetOf(equal));
    EXPECT_TRUE(superset.IsProperSupersetOf(view));
    EXPECT_TRUE(view.Overlaps(superset));

    SortedSet<int> disjoint = parent.GetViewBetween(8, 10);
    EXPECT_FALSE(view.Overlaps(disjoint));
    EXPECT_FALSE(disjoint.Overlaps(view));
    EXPECT_FALSE(view.SetEquals(disjoint));

    // A view compared against an out-of-range source considers only in-range elements.
    SortedSet<int> withOutOfRange{3, 4, 5, 99};
    EXPECT_FALSE(view.SetEquals(withOutOfRange));
    EXPECT_TRUE(view.IsSubsetOf(withOutOfRange));
    EXPECT_FALSE(view.IsSupersetOf(withOutOfRange));
}

// ---------------------------------------------------------------------------
// 26-27. ToSortedSet and getIsViewProperty
// ---------------------------------------------------------------------------

TEST(SortedSetLiveViewTests, ToSortedSetProducesADetachedOwningSet) {
    SortedSet<int> parent = oneToTen();
    SortedSet<int> snapshot = parent.GetViewBetween(3, 7).ToSortedSet();

    EXPECT_FALSE(snapshot.getIsViewProperty());
    EXPECT_EQ(snapshot.ToVector(), (std::vector<int>{3, 4, 5, 6, 7}));

    EXPECT_TRUE(snapshot.Add(99));           // no bounds: a detached set accepts anything
    EXPECT_FALSE(parent.Contains(99));
    EXPECT_TRUE(parent.Remove(4));
    EXPECT_TRUE(snapshot.Contains(4));

    SortedSet<int> wholeCopy = parent.ToSortedSet();
    EXPECT_FALSE(wholeCopy.getIsViewProperty());
    EXPECT_EQ(wholeCopy.getCountProperty(), parent.getCountProperty());
}

TEST(SortedSetLiveViewTests, IsViewPropertyDistinguishesTheTwoRoles) {
    SortedSet<int> defaulted;
    SortedSet<int> listed{1, 2, 3};
    EXPECT_FALSE(defaulted.getIsViewProperty());
    EXPECT_FALSE(listed.getIsViewProperty());
    EXPECT_FALSE(SortedSet<int>(listed).getIsViewProperty());

    SortedSet<int> view = listed.GetViewBetween(1, 2);
    SortedSet<int> nested = view.GetViewBetween(1, 2);
    EXPECT_TRUE(view.getIsViewProperty());
    EXPECT_TRUE(nested.getIsViewProperty());
    EXPECT_TRUE(SortedSet<int>(view).getIsViewProperty());
    EXPECT_FALSE(view.ToSortedSet().getIsViewProperty());

    // An owning full set has no bounds, so every value is "within range".
    EXPECT_TRUE(listed.IsWithinRange(-1000));
    EXPECT_TRUE(listed.IsWithinRange(1000));
}

// ---------------------------------------------------------------------------
// 28. Element type with operator< only (adjacent defect 1)
// ---------------------------------------------------------------------------

TEST(SortedSetLiveViewTests, ElementTypeWithOnlyOperatorLessInstantiatesTheWholeSurface) {
    SortedSet<LessOnly> s{LessOnly{1}, LessOnly{3}, LessOnly{5}, LessOnly{7}, LessOnly{9}};
    SortedSet<LessOnly> view = s.GetViewBetween(LessOnly{3}, LessOnly{7});

    EXPECT_EQ(view.getCountProperty(), 3);
    EXPECT_EQ(view.getMinProperty().value, 3);
    EXPECT_EQ(view.getMaxProperty().value, 7);
    EXPECT_TRUE(view.Contains(LessOnly{5}));
    EXPECT_FALSE(view.Contains(LessOnly{9}));
    EXPECT_TRUE(view.IsWithinRange(LessOnly{4}));
    EXPECT_FALSE(view.IsWithinRange(LessOnly{2}));

    EXPECT_TRUE(view.Add(LessOnly{4}));
    EXPECT_TRUE(s.Contains(LessOnly{4}));
    EXPECT_THROW(view.Add(LessOnly{99}), System::ArgumentOutOfRangeException);
    EXPECT_FALSE(view.Remove(LessOnly{9}));
    EXPECT_THROW((void)view.GetViewBetween(LessOnly{1}, LessOnly{5}),
                 System::ArgumentOutOfRangeException);
    EXPECT_THROW((void)s.GetViewBetween(LessOnly{7}, LessOnly{3}), System::ArgumentException);

    std::vector<LessOnly> seen;
    for (const LessOnly& x : view) seen.push_back(x);
    ASSERT_EQ(seen.size(), 4u);
    EXPECT_EQ(seen.front().value, 3);
    EXPECT_EQ(seen.back().value, 7);

    SortedSet<LessOnly> other{LessOnly{3}, LessOnly{4}, LessOnly{5}, LessOnly{7}};
    EXPECT_TRUE(view.SetEquals(other));
    EXPECT_TRUE(view.Overlaps(other));
    EXPECT_TRUE(view.IsSubsetOf(s));
    view.ExceptWith(other);
    EXPECT_EQ(view.getCountProperty(), 0);
    EXPECT_EQ(s.getCountProperty(), 2);
    EXPECT_EQ(view.ToSortedSet().getCountProperty(), 0);
}

// ---------------------------------------------------------------------------
// Custom ordering: everything must route through the container's comparer
// ---------------------------------------------------------------------------

TEST(SortedSetLiveViewTests, CustomOrderingDrivesBoundsLookupAndEnumeration) {
    // Sorted descending, so "lower" means numerically larger.
    SortedSet<Descending> s{Descending{1}, Descending{3}, Descending{5},
                            Descending{7}, Descending{9}};
    std::vector<int> order;
    for (const Descending& d : s) order.push_back(d.value);
    EXPECT_EQ(order, (std::vector<int>{9, 7, 5, 3, 1}));

    SortedSet<Descending> view = s.GetViewBetween(Descending{7}, Descending{3});
    EXPECT_EQ(view.getCountProperty(), 3);
    EXPECT_EQ(view.getMinProperty().value, 7);
    EXPECT_EQ(view.getMaxProperty().value, 3);
    EXPECT_TRUE(view.Contains(Descending{5}));
    EXPECT_FALSE(view.Contains(Descending{9}));
    EXPECT_FALSE(view.Contains(Descending{1}));

    // Arguments in natural-int order are the INVERTED range under this comparer.
    EXPECT_THROW((void)s.GetViewBetween(Descending{3}, Descending{7}), System::ArgumentException);
    // Widening in comparer order, not in int order.
    EXPECT_THROW((void)view.GetViewBetween(Descending{9}, Descending{5}),
                 System::ArgumentOutOfRangeException);
    EXPECT_THROW((void)view.GetViewBetween(Descending{5}, Descending{1}),
                 System::ArgumentOutOfRangeException);

    EXPECT_TRUE(view.Add(Descending{4}));
    EXPECT_TRUE(s.Contains(Descending{4}));
    EXPECT_THROW(view.Add(Descending{2}), System::ArgumentOutOfRangeException);

    view.Clear();
    std::vector<int> remaining;
    for (const Descending& d : s) remaining.push_back(d.value);
    EXPECT_EQ(remaining, (std::vector<int>{9, 1}));
}

// ---------------------------------------------------------------------------
// 29. Non-trivial element type across the matrix
// ---------------------------------------------------------------------------

TEST(SortedSetLiveViewTests, NonTrivialElementTypeAcrossTheMatrix) {
    SortedSet<std::string> s{std::string("apple"), std::string("banana"),
                             std::string("cherry"), std::string("date"),
                             std::string("elderberry")};
    SortedSet<std::string> view = s.GetViewBetween(std::string("banana"), std::string("date"));

    EXPECT_EQ(view.getCountProperty(), 3);
    EXPECT_EQ(view.getMinProperty(), "banana");
    EXPECT_EQ(view.getMaxProperty(), "date");
    EXPECT_EQ(view.ToVector(), (std::vector<std::string>{"banana", "cherry", "date"}));

    EXPECT_TRUE(view.Add(std::string("blueberry")));
    EXPECT_TRUE(s.Contains(std::string("blueberry")));
    EXPECT_THROW(view.Add(std::string("zucchini")), System::ArgumentOutOfRangeException);
    EXPECT_FALSE(view.Remove(std::string("apple")));

    SortedSet<std::string> viewCopy = view;
    EXPECT_TRUE(viewCopy.Remove(std::string("cherry")));
    EXPECT_FALSE(s.Contains(std::string("cherry")));

    SortedSet<std::string> snapshot = view.ToSortedSet();
    view.Clear();
    EXPECT_EQ(snapshot.getCountProperty(), 3);
    EXPECT_EQ(s.ToVector(), (std::vector<std::string>{"apple", "elderberry"}));

    SortedSet<std::string> moved = std::move(snapshot);
    EXPECT_EQ(moved.getCountProperty(), 3);
    // NOLINTNEXTLINE(bugprone-use-after-move) -- the moved-from contract is under test.
    EXPECT_EQ(snapshot.getCountProperty(), 0);
}

// ---------------------------------------------------------------------------
// 30. Scale: GetViewBetween must stay O(1) in element copies, and the view's
//     cached Count must track the shared version.
// ---------------------------------------------------------------------------

TEST(SortedSetLiveViewTests, GetViewBetweenCopiesNoElementsRegardlessOfRangeSize) {
    SortedSet<Counted> s;
    for (int i = 0; i < 10000; ++i) s.Add(Counted(i));

    Counted::copies = 0;
    SortedSet<Counted> view = s.GetViewBetween(Counted(1000), Counted(9000));
    const int copiesForView = Counted::copies;

    // The snapshot implementation copied every in-range element (8,001 of them). The live
    // view copies only the two bound values, so this stays a small constant.
    EXPECT_LE(copiesForView, 8);
    EXPECT_EQ(view.getCountProperty(), 8001);
}

TEST(SortedSetLiveViewTests, ScaleRegressionWithOneHundredThousandElements) {
    SortedSet<int> s;
    for (int i = 0; i < 100000; ++i) s.Add(i);
    EXPECT_EQ(s.getCountProperty(), 100000);

    SortedSet<int> view = s.GetViewBetween(40000, 60000);
    EXPECT_EQ(view.getCountProperty(), 20001);
    EXPECT_EQ(view.getCountProperty(), 20001);   // second call takes the version-keyed cache
    EXPECT_EQ(view.getMinProperty(), 40000);
    EXPECT_EQ(view.getMaxProperty(), 60000);

    long long sum = 0;
    for (int x : view) sum += x;
    EXPECT_EQ(sum, 1000050000LL);

    EXPECT_TRUE(s.Remove(50000));                // invalidates the cached count
    EXPECT_EQ(view.getCountProperty(), 20000);

    view.Clear();
    EXPECT_EQ(view.getCountProperty(), 0);
    EXPECT_EQ(s.getCountProperty(), 79999);
    EXPECT_TRUE(s.Contains(39999));
    EXPECT_TRUE(s.Contains(60001));
    EXPECT_FALSE(s.Contains(40000));
}
