// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Permanent regression suite for ticket #1785 (REMED-COLL-SORTEDSET-NESTED-EXCEPTION-ORDER):
// System::Collections::Generic::SortedSet<T>::GetViewBetween must validate a nested request in
// .NET's order -- lower widening, then upper widening, then the lower-versus-upper
// relationship -- rather than in the order design ticket #1782 selected and implementation
// ticket #1783 shipped (invalid range first).
//
// The two orders are observable exactly when a nested request is simultaneously WIDENING and
// INVERTED. .NET's control flow, verified against the current sources:
//
//   SortedSet.TreeSubSet.cs:342-353   GetViewBetween(lowerValue, upperValue)
//       if (_lBoundActive && Comparer.Compare(_min, lowerValue) > 0)
//           throw new ArgumentOutOfRangeException(nameof(lowerValue));
//       if (_uBoundActive && Comparer.Compare(_max, upperValue) < 0)
//           throw new ArgumentOutOfRangeException(nameof(upperValue));
//       return (TreeSubSet)_underlying.GetViewBetween(lowerValue, upperValue);
//
//   SortedSet.cs:1508-1515            SortedSet<T>.GetViewBetween(lowerValue, upperValue)
//       if (Comparer.Compare(lowerValue, upperValue) > 0)
//           throw new ArgumentException(SR.SortedSet_LowerValueGreaterThanUpperValue,
//                                       nameof(lowerValue));
//       return new TreeSubSet(this, lowerValue, upperValue, true, true);
//
// An owning full set has no active bounds, so only the third check can fire on it and its
// behaviour is unchanged by this ticket. This file therefore pins the nested matrix
// exhaustively and re-pins the top-level cases as an explicit no-regression guard.
//
// The design record is docs/SortedSetLiveViewDesign.md section 33; the measured pre-fix and
// post-fix matrices are build-probe-sortedset/probe18_{prefix,postfix}.log. The live-view
// contract itself is covered by SortedSetLiveViewTests.cpp and is deliberately not duplicated
// here -- only the nested-view behaviour that must survive this reordering is re-asserted.
#include <gtest/gtest.h>

#include <string>
#include <utility>
#include <vector>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Collections/Generic/SortedSet.hpp"
#include "System/InvalidOperationException.hpp"

using System::Collections::Generic::SortedSet;

namespace {

/** Element type providing ONLY operator<, exactly SortedSet<T>'s documented element contract. */
struct LessOnly {
    int value = 0;
    bool operator<(const LessOnly& other) const { return value < other.value; }
};

/**
 * Element type whose std::less orders DESCENDING, so "orders before" disagrees with numeric
 * order. It defines no operator>, ==, <=, or >=, so every bound and ordering decision below
 * must route through the container's comparer.
 */
struct Descending {
    int value = 0;
    bool operator<(const Descending& other) const { return value > other.value; }
};

/** The exception classification of one GetViewBetween call. */
enum class Kind { Ok, OutOfRange, InvalidRange, Other };

/** What one GetViewBetween call did, in full. */
struct Outcome {
    Kind kind = Kind::Other;
    std::string paramName;
    std::string message;
    SharpRuntime::intcs hresult = 0;
    SharpRuntime::intcs count = 0;
};

/**
 * Calls GetViewBetween and records the complete observable outcome. ArgumentOutOfRangeException
 * derives from ArgumentException, so it is caught first; the two are never conflated.
 */
template<typename T>
Outcome attempt(SortedSet<T>& target, const T& lower, const T& upper) {
    try {
        SortedSet<T> nested = target.GetViewBetween(lower, upper);
        Outcome o;
        o.kind = Kind::Ok;
        o.count = nested.getCountProperty();
        return o;
    } catch (const System::ArgumentOutOfRangeException& ex) {
        return Outcome{Kind::OutOfRange, ex.getParamNameProperty(), ex.what(),
                       ex.getHResultProperty(), 0};
    } catch (const System::ArgumentException& ex) {
        return Outcome{Kind::InvalidRange, ex.getParamNameProperty(), ex.what(),
                       ex.getHResultProperty(), 0};
    }
}

// The two exact messages this surface can produce. Both are stable, fully specified by .NET's
// own resources, and therefore pinned in full rather than by prefix:
//   * SR.SortedSet_LowerValueGreaterThanUpperValue = "Must be less than or equal to upperValue."
//   * ArgumentOutOfRangeException(paramName)'s default text.
// The "(Parameter 'x')" suffix is appended exactly once since ticket #1776.
constexpr const char* kInvalidRangeMessage =
    "Must be less than or equal to upperValue. (Parameter 'lowerValue')";
constexpr const char* kOutOfRangeLowerMessage =
    "Specified argument was out of the range of valid values. (Parameter 'lowerValue')";
constexpr const char* kOutOfRangeUpperMessage =
    "Specified argument was out of the range of valid values. (Parameter 'upperValue')";

// COR_E_ARGUMENT / COR_E_ARGUMENTOUTOFRANGE, matching .NET.
constexpr SharpRuntime::intcs kArgumentHResult =
    static_cast<SharpRuntime::intcs>(0x80070057);
constexpr SharpRuntime::intcs kOutOfRangeHResult =
    static_cast<SharpRuntime::intcs>(0x80131502);

void expectOutOfRangeLower(const Outcome& o) {
    EXPECT_EQ(o.kind, Kind::OutOfRange);
    EXPECT_EQ(o.paramName, "lowerValue");
    EXPECT_EQ(o.message, kOutOfRangeLowerMessage);
    EXPECT_EQ(o.hresult, kOutOfRangeHResult);
}

void expectOutOfRangeUpper(const Outcome& o) {
    EXPECT_EQ(o.kind, Kind::OutOfRange);
    EXPECT_EQ(o.paramName, "upperValue");
    EXPECT_EQ(o.message, kOutOfRangeUpperMessage);
    EXPECT_EQ(o.hresult, kOutOfRangeHResult);
}

void expectInvalidRange(const Outcome& o) {
    EXPECT_EQ(o.kind, Kind::InvalidRange);
    EXPECT_EQ(o.paramName, "lowerValue");
    EXPECT_EQ(o.message, kInvalidRangeMessage);
    EXPECT_EQ(o.hresult, kArgumentHResult);
}

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
// 1-2, 10-13. Requests that must still SUCCEED, unchanged by the reordering
// ---------------------------------------------------------------------------

TEST(SortedSetNestedViewOrderTests, ValidNarrowerNestedRangeStillSucceeds) {
    SortedSet<int> parent = oneToTen();
    SortedSet<int> view = parent.GetViewBetween(3, 7);

    SortedSet<int> nested = view.GetViewBetween(4, 6);
    EXPECT_TRUE(nested.getIsViewProperty());
    EXPECT_EQ(nested.getCountProperty(), 3);
    EXPECT_EQ(collect(nested), (std::vector<int>{4, 5, 6}));
}

TEST(SortedSetNestedViewOrderTests, NestedRangeIdenticalToTheParentViewSucceeds) {
    SortedSet<int> parent = oneToTen();
    SortedSet<int> view = parent.GetViewBetween(3, 7);

    // Neither bound widens: `cmp(3, 3)` and `cmp(7, 7)` are both false.
    SortedSet<int> same = view.GetViewBetween(3, 7);
    EXPECT_EQ(same.getCountProperty(), 5);
    EXPECT_EQ(collect(same), (std::vector<int>{3, 4, 5, 6, 7}));
}

TEST(SortedSetNestedViewOrderTests, EqualLowerAndUpperBoundsSucceed) {
    SortedSet<int> parent = oneToTen();
    SortedSet<int> view = parent.GetViewBetween(3, 7);

    SortedSet<int> point = view.GetViewBetween(5, 5);
    EXPECT_EQ(point.getCountProperty(), 1);       // one-element resulting range
    EXPECT_EQ(point.getMinProperty(), 5);
    EXPECT_EQ(point.getMaxProperty(), 5);
    EXPECT_THROW(point.Add(6), System::ArgumentOutOfRangeException);
}

TEST(SortedSetNestedViewOrderTests, EmptyResultingNestedRangeIsStillAValidView) {
    SortedSet<int> parent{1, 2, 3, 4, 6, 7, 8, 9, 10};   // deliberately no 5
    SortedSet<int> view = parent.GetViewBetween(3, 7);

    SortedSet<int> empty = view.GetViewBetween(5, 5);
    EXPECT_TRUE(empty.getIsViewProperty());
    EXPECT_EQ(empty.getCountProperty(), 0);
    EXPECT_TRUE(empty.getIsEmptyProperty());

    // It is a live empty view, not a dead one: writing the one in-range value works.
    EXPECT_TRUE(empty.Add(5));
    EXPECT_TRUE(parent.Contains(5));
    EXPECT_EQ(empty.getCountProperty(), 1);
}

// ---------------------------------------------------------------------------
// 3-5. Widening alone: unchanged by this ticket, re-pinned as a no-regression guard
// ---------------------------------------------------------------------------

TEST(SortedSetNestedViewOrderTests, LowerWideningAloneThrowsOutOfRangeForLowerValue) {
    SortedSet<int> parent = oneToTen();
    SortedSet<int> view = parent.GetViewBetween(3, 7);
    expectOutOfRangeLower(attempt(view, 2, 6));
}

TEST(SortedSetNestedViewOrderTests, UpperWideningAloneThrowsOutOfRangeForUpperValue) {
    SortedSet<int> parent = oneToTen();
    SortedSet<int> view = parent.GetViewBetween(3, 7);
    expectOutOfRangeUpper(attempt(view, 4, 9));
}

TEST(SortedSetNestedViewOrderTests, BothBoundsWideningSelectsTheLowerBound) {
    SortedSet<int> parent = oneToTen();
    SortedSet<int> view = parent.GetViewBetween(3, 7);
    // TreeSubSet.cs:344 precedes TreeSubSet.cs:348, so lowerValue wins.
    expectOutOfRangeLower(attempt(view, 2, 9));
}

// ---------------------------------------------------------------------------
// 6-9. The precedence this ticket changes
// ---------------------------------------------------------------------------

TEST(SortedSetNestedViewOrderTests, PurelyInvertedRangeInsideTheViewStillThrowsArgumentException) {
    SortedSet<int> parent = oneToTen();
    SortedSet<int> view = parent.GetViewBetween(3, 7);
    // Neither bound widens [3,7], so the delegated-to SortedSet.cs:1510 check is reached.
    expectInvalidRange(attempt(view, 6, 4));
}

TEST(SortedSetNestedViewOrderTests, LowerWideningWinsOverAnInvertedRange) {
    SortedSet<int> parent = oneToTen();
    SortedSet<int> view = parent.GetViewBetween(3, 7);

    // view[3,7].GetViewBetween(2, 1): 2 widens below 3 AND 1 orders before 2.
    // Ticket #1783 threw ArgumentException("Must be less than or equal to upperValue.").
    // .NET -- and this port since #1785 -- throws ArgumentOutOfRangeException("lowerValue").
    expectOutOfRangeLower(attempt(view, 2, 1));
}

TEST(SortedSetNestedViewOrderTests, UpperWideningWinsOverAnInvertedRange) {
    SortedSet<int> parent = oneToTen();
    SortedSet<int> view = parent.GetViewBetween(3, 7);

    // view[3,7].GetViewBetween(12, 9): 12 does not widen below 3, 9 widens above 7, and
    // 9 orders before 12. The upper-widening check at TreeSubSet.cs:348 is reached before
    // the delegation, so upperValue wins over the inverted range.
    expectOutOfRangeUpper(attempt(view, 12, 9));
}

TEST(SortedSetNestedViewOrderTests, InvertedRangeOutsideTheViewButNotWideningStaysArgumentException) {
    SortedSet<int> parent = oneToTen();
    SortedSet<int> view = parent.GetViewBetween(3, 7);

    // Both bounds are outside [3,7], but on the NON-widening side: 9 is above the upper
    // bound (so it does not widen the lower one) and 2 is below the lower bound (so it does
    // not widen the upper one). Neither .NET check fires and the inverted-range check wins.
    expectInvalidRange(attempt(view, 9, 2));
}

TEST(SortedSetNestedViewOrderTests, WideningBothBoundsWhileInvertedIsArithmeticallyImpossible) {
    // A view's bounds always satisfy !cmp(upper_, lower_) -- construction rejects anything
    // else -- so lower widening (cmp(lower, lower_)) and upper widening (cmp(upper_, upper))
    // together give lower < lower_ <= upper_ < upper, which cannot also be inverted. This is
    // why the matrix has no "both widen AND inverted" row: exhaustive proof over a grid that
    // covers every relative position of both arguments against both bounds.
    SortedSet<int> parent = oneToTen();
    SortedSet<int> view = parent.GetViewBetween(3, 7);

    for (int lower = -2; lower <= 12; ++lower) {
        for (int upper = -2; upper <= 12; ++upper) {
            const bool widensLower = lower < 3;
            const bool widensUpper = upper > 7;
            const bool inverted = upper < lower;
            ASSERT_FALSE(widensLower && widensUpper && inverted)
                << "unreachable combination at (" << lower << ", " << upper << ")";
        }
    }
}

// ---------------------------------------------------------------------------
// The complete matrix as one exhaustive oracle
// ---------------------------------------------------------------------------

TEST(SortedSetNestedViewOrderTests, ExhaustiveGridMatchesTheDotNetDecisionProcedure) {
    // The oracle is .NET's control flow transcribed directly, independently of the header
    // under test: TreeSubSet.cs's two bound checks, then SortedSet.cs:1510's range check.
    // Every (lower, upper) pair around and across the view's bounds is compared against it.
    for (int lower = -2; lower <= 12; ++lower) {
        for (int upper = -2; upper <= 12; ++upper) {
            SortedSet<int> parent = oneToTen();
            SortedSet<int> view = parent.GetViewBetween(3, 7);
            const Outcome actual = attempt(view, lower, upper);

            SCOPED_TRACE("lower=" + std::to_string(lower) + " upper=" + std::to_string(upper));
            if (lower < 3) {
                expectOutOfRangeLower(actual);
            } else if (upper > 7) {
                expectOutOfRangeUpper(actual);
            } else if (upper < lower) {
                expectInvalidRange(actual);
            } else {
                EXPECT_EQ(actual.kind, Kind::Ok);
                // A successful nested view holds exactly the parent's elements in [lower, upper].
                SharpRuntime::intcs expected = 0;
                for (int v = lower; v <= upper; ++v)
                    if (v >= 1 && v <= 10) ++expected;
                EXPECT_EQ(actual.count, expected);
            }
        }
    }
}

TEST(SortedSetNestedViewOrderTests, TopLevelGetViewBetweenIsUnchangedByTheReordering) {
    // An owning set activates neither bound, so the two widening checks are skipped and only
    // SortedSet.cs:1510 can fire -- exactly the pre-#1785 behaviour.
    SortedSet<int> parent = oneToTen();

    expectInvalidRange(attempt(parent, 7, 3));
    EXPECT_EQ(attempt(parent, 3, 7).count, 5);
    EXPECT_EQ(attempt(parent, 100, 200).count, 0);      // disjoint but valid
    EXPECT_EQ(attempt(parent, 5, 5).count, 1);
    EXPECT_EQ(attempt(parent, -5, 100).count, 10);      // wider than the contents: legal
}

// ---------------------------------------------------------------------------
// 16. A nested view of a nested view
// ---------------------------------------------------------------------------

TEST(SortedSetNestedViewOrderTests, NestedViewOfANestedViewValidatesAgainstTheInnerBounds) {
    SortedSet<int> parent = oneToTen();
    SortedSet<int> outer = parent.GetViewBetween(3, 7);
    SortedSet<int> inner = outer.GetViewBetween(4, 6);

    // Narrowing further is fine, and the result is still flattened onto the root state.
    SortedSet<int> deepest = inner.GetViewBetween(5, 6);
    EXPECT_EQ(collect(deepest), (std::vector<int>{5, 6}));
    EXPECT_TRUE(deepest.Remove(5));
    EXPECT_FALSE(parent.Contains(5));
    EXPECT_TRUE(deepest.Add(5));

    // The effective bounds are the INNER view's [4,6], not the outer view's [3,7]: widening
    // back out to the outer bounds is rejected on both ends.
    expectOutOfRangeLower(attempt(inner, 3, 6));
    expectOutOfRangeUpper(attempt(inner, 5, 7));

    // And the new precedence applies at every depth.
    expectOutOfRangeLower(attempt(inner, 3, 2));    // widens below 4 and is inverted
    expectOutOfRangeUpper(attempt(inner, 9, 7));    // widens above 6 and is inverted
    expectInvalidRange(attempt(inner, 6, 5));       // inverted strictly inside [4,6]
}

// ---------------------------------------------------------------------------
// 14-15. Custom comparer, and an operator<-only element type
// ---------------------------------------------------------------------------

TEST(SortedSetNestedViewOrderTests, CustomComparerDrivesPrecedenceInComparerOrderNotIntOrder) {
    // std::less<Descending> sorts DESCENDING, so the view [7, 3] runs from 7 down to 3 and
    // "widening the lower bound" means asking for a numerically LARGER value.
    SortedSet<Descending> s{Descending{1}, Descending{3}, Descending{5},
                            Descending{7}, Descending{9}};
    SortedSet<Descending> view = s.GetViewBetween(Descending{7}, Descending{3});
    ASSERT_EQ(view.getCountProperty(), 3);

    // Widening alone, in comparer order.
    expectOutOfRangeLower(attempt(view, Descending{9}, Descending{5}));
    expectOutOfRangeUpper(attempt(view, Descending{5}, Descending{1}));

    // Inverted strictly inside the view: 3 orders AFTER 7 under this comparer.
    expectInvalidRange(attempt(view, Descending{3}, Descending{7}));

    // Simultaneously widening and inverted -- the cases this ticket changes. Under the
    // pre-#1785 order both were ArgumentException("...upperValue...", "lowerValue").
    expectOutOfRangeLower(attempt(view, Descending{9}, Descending{11}));
    expectOutOfRangeUpper(attempt(view, Descending{0}, Descending{1}));

    // Valid narrowing still works and is still live.
    SortedSet<Descending> narrow = view.GetViewBetween(Descending{7}, Descending{5});
    EXPECT_EQ(narrow.getCountProperty(), 2);
    EXPECT_TRUE(narrow.Remove(Descending{5}));
    EXPECT_FALSE(s.Contains(Descending{5}));
}

TEST(SortedSetNestedViewOrderTests, OperatorLessOnlyElementTypeGetsTheSamePrecedence) {
    SortedSet<LessOnly> s{LessOnly{1}, LessOnly{3}, LessOnly{5}, LessOnly{7}, LessOnly{9}};
    SortedSet<LessOnly> view = s.GetViewBetween(LessOnly{3}, LessOnly{7});

    expectOutOfRangeLower(attempt(view, LessOnly{1}, LessOnly{5}));
    expectOutOfRangeUpper(attempt(view, LessOnly{4}, LessOnly{9}));
    expectInvalidRange(attempt(view, LessOnly{6}, LessOnly{4}));
    expectOutOfRangeLower(attempt(view, LessOnly{2}, LessOnly{1}));
    expectOutOfRangeUpper(attempt(view, LessOnly{12}, LessOnly{9}));

    SortedSet<LessOnly> narrow = view.GetViewBetween(LessOnly{5}, LessOnly{7});
    EXPECT_EQ(narrow.getCountProperty(), 2);
}

TEST(SortedSetNestedViewOrderTests, NonTrivialElementTypeGetsTheSamePrecedence) {
    SortedSet<std::string> s{std::string("apple"), std::string("banana"),
                             std::string("cherry"), std::string("date")};
    SortedSet<std::string> view =
        s.GetViewBetween(std::string("banana"), std::string("cherry"));

    expectOutOfRangeLower(attempt(view, std::string("apple"), std::string("apricot")));
    expectOutOfRangeUpper(attempt(view, std::string("date"), std::string("cranberry")));
    expectInvalidRange(attempt(view, std::string("cherry"), std::string("banana")));
    EXPECT_EQ(attempt(view, std::string("banana"), std::string("cherry")).count, 2);
}

// ---------------------------------------------------------------------------
// A failed nested construction must be a complete no-op
// ---------------------------------------------------------------------------

TEST(SortedSetNestedViewOrderTests, FailedNestedConstructionMutatesNothingAndBumpsNoVersion) {
    SortedSet<int> parent = oneToTen();
    SortedSet<int> view = parent.GetViewBetween(3, 7);
    SortedSet<int> nested = view.GetViewBetween(4, 6);

    const std::vector<int> before = parent.ToVector();
    auto parentIterator = parent.begin();       // snapshots the single shared version
    auto viewIterator = view.begin();

    const std::vector<std::pair<int, int>> failing = {
        {2, 6}, {4, 9}, {2, 9}, {6, 4}, {2, 1}, {12, 9}, {9, 2},
    };
    for (const auto& [lower, upper] : failing) {
        EXPECT_NE(attempt(view, lower, upper).kind, Kind::Ok);
        EXPECT_EQ(parent.ToVector(), before);
        EXPECT_EQ(parent.getCountProperty(), 10);
        EXPECT_EQ(view.getCountProperty(), 5);
        EXPECT_EQ(nested.getCountProperty(), 3);
    }

    // No version was bumped: an iterator taken before the failures is still usable, which is
    // exactly what a bumped counter would have made throw.
    EXPECT_NO_THROW((void)*parentIterator);
    EXPECT_NO_THROW((void)*viewIterator);
    EXPECT_EQ(*parentIterator, 1);
    EXPECT_EQ(*viewIterator, 3);
}

TEST(SortedSetNestedViewOrderTests, RepeatedFailedConstructionLeavesEveryViewFullyUsable) {
    SortedSet<int> parent = oneToTen();
    SortedSet<int> view = parent.GetViewBetween(3, 7);
    SortedSet<int> sibling = parent.GetViewBetween(5, 9);

    for (int i = 0; i < 500; ++i) {
        EXPECT_EQ(attempt(view, 2, 1).kind, Kind::OutOfRange);
        EXPECT_EQ(attempt(view, 12, 9).kind, Kind::OutOfRange);
        EXPECT_EQ(attempt(view, 6, 4).kind, Kind::InvalidRange);
    }

    EXPECT_EQ(view.getCountProperty(), 5);
    EXPECT_EQ(sibling.getCountProperty(), 5);
    EXPECT_TRUE(view.Remove(4));
    EXPECT_FALSE(parent.Contains(4));
    EXPECT_TRUE(view.Add(4));
    EXPECT_EQ(parent.getCountProperty(), 10);
}

// ---------------------------------------------------------------------------
// Nested-view behaviour that must survive the reordering unchanged
// ---------------------------------------------------------------------------

TEST(SortedSetNestedViewOrderTests, NestedViewsStayLiveInBothDirections) {
    SortedSet<int> parent = oneToTen();
    SortedSet<int> outer = parent.GetViewBetween(3, 7);
    SortedSet<int> inner = outer.GetViewBetween(4, 6);

    // parent -> nested view
    EXPECT_TRUE(parent.Remove(5));
    EXPECT_FALSE(inner.Contains(5));
    EXPECT_EQ(inner.getCountProperty(), 2);

    // nested view -> parent
    EXPECT_TRUE(inner.Add(5));
    EXPECT_TRUE(parent.Contains(5));
    EXPECT_TRUE(outer.Contains(5));

    // Bounds stay enforced for the whole life of the nested view.
    EXPECT_THROW(inner.Add(7), System::ArgumentOutOfRangeException);
    EXPECT_FALSE(inner.Remove(7));          // out of range: false, no throw, parent untouched
    EXPECT_TRUE(parent.Contains(7));

    // Clear removes exactly the nested range.
    inner.Clear();
    EXPECT_EQ(inner.getCountProperty(), 0);
    EXPECT_EQ(collect(outer), (std::vector<int>{3, 7}));
    EXPECT_EQ(parent.ToVector(), (std::vector<int>{1, 2, 3, 7, 8, 9, 10}));
}

TEST(SortedSetNestedViewOrderTests, OverlappingViewsStillShareOneStateAndOneVersion) {
    SortedSet<int> parent = oneToTen();
    SortedSet<int> a = parent.GetViewBetween(2, 6);
    SortedSet<int> b = parent.GetViewBetween(4, 8);
    SortedSet<int> nested = a.GetViewBetween(4, 6);

    EXPECT_TRUE(nested.Remove(5));
    EXPECT_FALSE(a.Contains(5));
    EXPECT_FALSE(b.Contains(5));
    EXPECT_FALSE(parent.Contains(5));

    // One shared counter: a mutation through any handle fail-fasts every enumeration.
    auto it = nested.begin();
    EXPECT_TRUE(parent.Add(5));
    EXPECT_THROW((void)*it, System::InvalidOperationException);

    auto parentIterator = parent.begin();
    EXPECT_TRUE(nested.Remove(4));
    EXPECT_THROW((void)*parentIterator, System::InvalidOperationException);
}

TEST(SortedSetNestedViewOrderTests, CustomComparerNestedBoundsStayCorrectAfterFailedAttempts) {
    SortedSet<Descending> s{Descending{1}, Descending{3}, Descending{5},
                            Descending{7}, Descending{9}};
    SortedSet<Descending> view = s.GetViewBetween(Descending{9}, Descending{3});
    SortedSet<Descending> nested = view.GetViewBetween(Descending{7}, Descending{3});

    expectOutOfRangeLower(attempt(nested, Descending{9}, Descending{11}));
    expectOutOfRangeUpper(attempt(nested, Descending{5}, Descending{1}));

    // Bounds and contents are exactly what they were before the two failures.
    EXPECT_EQ(nested.getCountProperty(), 3);
    EXPECT_EQ(nested.getMinProperty().value, 7);
    EXPECT_EQ(nested.getMaxProperty().value, 3);
    EXPECT_TRUE(nested.Contains(Descending{5}));
    EXPECT_FALSE(nested.Contains(Descending{9}));
    EXPECT_THROW(nested.Add(Descending{9}), System::ArgumentOutOfRangeException);
    EXPECT_TRUE(nested.Add(Descending{4}));
    EXPECT_TRUE(s.Contains(Descending{4}));
}
