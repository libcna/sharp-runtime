// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// CCF-010 / SR-AUD-046 — permanent regressions for the default comparison and
// equality contract. See docs/ComparisonContractPlan.md.
//
// These tests are the ONLY gate on this family. Measured on 2026-07-31 against
// the pre-repair headers, AddressSanitizer, UndefinedBehaviorSanitizer and
// _GLIBCXX_DEBUG were all silent on every defect, including the data corruption
// of the finite elements in a NaN-bearing sort. Nothing but a value assertion
// can catch a regression here.

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "System/Array.hpp"
#include "System/MemoryExtensions.hpp"
#include "System/Span.hpp"
#include "System/Tuple.hpp"
#include "System/ValueTuple.hpp"
#include "System/detail/ComparisonPolicy.hpp"

using SharpRuntime::intcs;
using System::Array;

namespace {

constexpr float kNaNf = std::numeric_limits<float>::quiet_NaN();
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr float kInff = std::numeric_limits<float>::infinity();
constexpr double kInf = std::numeric_limits<double>::infinity();

/** A type with only the two built-in-shaped operators, to prove the policy adds
 *  no requirement beyond what the pre-repair code already needed. */
struct Widget {
    int v;
    bool operator<(const Widget& o) const { return v < o.v; }
    bool operator==(const Widget& o) const { return v == o.v; }
};

/** Counts inversions among the non-NaN elements: 0 iff the finite part is sorted. */
template<typename T>
int finiteInversions(const std::vector<T>& v) {
    int inversions = 0;
    T prev = -std::numeric_limits<T>::infinity();
    for (T x : v) {
        if (std::isnan(x)) continue;
        if (x < prev) ++inversions;
        prev = x;
    }
    return inversions;
}

} // namespace

// ===========================================================================
// The policy itself — System::detail::compareValues / equalValues / hashValue
// ===========================================================================

TEST(ComparisonPolicyTests, CompareValues_NaN_Orders_Before_Every_Value) {
    // Single.CompareTo: NaN is smaller than everything, including -Infinity.
    EXPECT_LT(System::detail::compareValues(kNaNf, 1.0f), 0);
    EXPECT_GT(System::detail::compareValues(1.0f, kNaNf), 0);
    EXPECT_LT(System::detail::compareValues(kNaNf, -kInff), 0);
    EXPECT_GT(System::detail::compareValues(-kInff, kNaNf), 0);
    EXPECT_LT(System::detail::compareValues(kNaN, -kInf), 0);
    EXPECT_GT(System::detail::compareValues(-kInf, kNaN), 0);
}

TEST(ComparisonPolicyTests, CompareValues_TwoNaNs_Are_Equal) {
    EXPECT_EQ(System::detail::compareValues(kNaNf, kNaNf), 0);
    EXPECT_EQ(System::detail::compareValues(kNaN, kNaN), 0);
    // A NaN with a different payload still compares equal.
    const double other = std::nan("7");
    ASSERT_TRUE(std::isnan(other));
    EXPECT_EQ(System::detail::compareValues(kNaN, other), 0);
}

TEST(ComparisonPolicyTests, CompareValues_Ordinary_Values_Unchanged) {
    EXPECT_LT(System::detail::compareValues(1.0f, 2.0f), 0);
    EXPECT_GT(System::detail::compareValues(2.0f, 1.0f), 0);
    EXPECT_EQ(System::detail::compareValues(2.0f, 2.0f), 0);
    EXPECT_LT(System::detail::compareValues(-kInff, kInff), 0);
}

TEST(ComparisonPolicyTests, CompareValues_SignedZero_Compares_Equal) {
    EXPECT_EQ(System::detail::compareValues(0.0, -0.0), 0);
    EXPECT_EQ(System::detail::compareValues(-0.0f, 0.0f), 0);
}

TEST(ComparisonPolicyTests, CompareValues_NonFloating_Is_The_Builtin_Operator) {
    EXPECT_LT(System::detail::compareValues(1, 2), 0);
    EXPECT_GT(System::detail::compareValues(2, 1), 0);
    EXPECT_EQ(System::detail::compareValues(2, 2), 0);
    EXPECT_LT(System::detail::compareValues(std::string("a"), std::string("b")), 0);
    EXPECT_EQ(System::detail::compareValues(Widget{5}, Widget{5}), 0);
    EXPECT_LT(System::detail::compareValues(Widget{4}, Widget{5}), 0);
}

TEST(ComparisonPolicyTests, EqualValues_NaN_Equals_Itself) {
    EXPECT_TRUE(System::detail::equalValues(kNaNf, kNaNf));
    EXPECT_TRUE(System::detail::equalValues(kNaN, kNaN));
    EXPECT_TRUE(System::detail::equalValues(kNaN, std::nan("7")));
    EXPECT_FALSE(System::detail::equalValues(kNaN, 1.0));
    EXPECT_FALSE(System::detail::equalValues(1.0, kNaN));
}

TEST(ComparisonPolicyTests, EqualValues_SignedZero_And_Ordinary_Values) {
    EXPECT_TRUE(System::detail::equalValues(0.0, -0.0));
    EXPECT_TRUE(System::detail::equalValues(2.5f, 2.5f));
    EXPECT_FALSE(System::detail::equalValues(2.5f, 2.6f));
    EXPECT_TRUE(System::detail::equalValues(kInf, kInf));
    EXPECT_FALSE(System::detail::equalValues(kInf, -kInf));
}

TEST(ComparisonPolicyTests, EqualValues_NonFloating_Is_The_Builtin_Operator) {
    EXPECT_TRUE(System::detail::equalValues(7, 7));
    EXPECT_FALSE(System::detail::equalValues(7, 8));
    EXPECT_TRUE(System::detail::equalValues(std::string("x"), std::string("x")));
    EXPECT_TRUE(System::detail::equalValues(Widget{3}, Widget{3}));
}

TEST(ComparisonPolicyTests, HashValue_Every_NaN_Hashes_Alike) {
    // Required by equalValues' NaN reflexivity: equal objects, equal hashes.
    EXPECT_EQ(System::detail::hashValue(kNaN), System::detail::hashValue(std::nan("7")));
    EXPECT_EQ(System::detail::hashValue(kNaN), System::detail::hashValue(-kNaN));
    EXPECT_EQ(System::detail::hashValue(kNaNf), System::detail::hashValue(-kNaNf));
}

TEST(ComparisonPolicyTests, HashValue_SignedZero_And_Ordinary_Values) {
    EXPECT_EQ(System::detail::hashValue(0.0), System::detail::hashValue(-0.0));
    EXPECT_EQ(System::detail::hashValue(2.5), System::detail::hashValue(2.5));
    EXPECT_EQ(System::detail::hashValue(42), std::hash<int>{}(42));
    EXPECT_EQ(System::detail::hashValue(std::string("q")), std::hash<std::string>{}("q"));
}

TEST(ComparisonPolicyTests, DefaultLess_Is_A_Total_Order_With_NaN_Smallest) {
    System::detail::DefaultLess<double> less;
    EXPECT_TRUE(less(kNaN, -kInf));
    EXPECT_FALSE(less(-kInf, kNaN));
    EXPECT_FALSE(less(kNaN, kNaN));    // irreflexive
    EXPECT_TRUE(less(1.0, 2.0));
    EXPECT_FALSE(less(2.0, 1.0));
    // Non-floating: exactly `a < b`.
    System::detail::DefaultLess<Widget> wless;
    EXPECT_TRUE(wless(Widget{1}, Widget{2}));
    EXPECT_FALSE(wless(Widget{2}, Widget{1}));
    System::detail::DefaultLess<std::string> sless;
    EXPECT_TRUE(sless("a", "b"));
}

TEST(ComparisonPolicyTests, DefaultGreater_Is_The_Reverse) {
    System::detail::DefaultGreater<double> greater;
    EXPECT_TRUE(greater(-kInf, kNaN));
    EXPECT_FALSE(greater(kNaN, -kInf));
    EXPECT_FALSE(greater(kNaN, kNaN));
    EXPECT_TRUE(greater(2.0, 1.0));
}

TEST(ComparisonPolicyTests, MoveNaNsToFront_Counts_And_Partitions) {
    std::vector<double> v{1.0, kNaN, 2.0, kNaN, 3.0};
    const std::size_t moved = System::detail::moveNaNsToFront(v.begin(), v.end());
    EXPECT_EQ(moved, 2u);
    EXPECT_TRUE(std::isnan(v[0]));
    EXPECT_TRUE(std::isnan(v[1]));
    for (std::size_t i = moved; i < v.size(); ++i) EXPECT_FALSE(std::isnan(v[i]));
}

TEST(ComparisonPolicyTests, MoveNaNsToFront_Degenerate_Ranges) {
    std::vector<double> empty;
    EXPECT_EQ(System::detail::moveNaNsToFront(empty.begin(), empty.end()), 0u);

    std::vector<double> one{kNaN};
    EXPECT_EQ(System::detail::moveNaNsToFront(one.begin(), one.end()), 1u);

    std::vector<double> allNaN{kNaN, kNaN, kNaN};
    EXPECT_EQ(System::detail::moveNaNsToFront(allNaN.begin(), allNaN.end()), 3u);

    std::vector<double> noNaN{3.0, 1.0, 2.0};
    EXPECT_EQ(System::detail::moveNaNsToFront(noNaN.begin(), noNaN.end()), 0u);
    EXPECT_EQ(noNaN, (std::vector<double>{3.0, 1.0, 2.0}));  // untouched

    // Non-floating value type: no-op returning 0.
    std::vector<int> ints{3, 1, 2};
    EXPECT_EQ(System::detail::moveNaNsToFront(ints.begin(), ints.end()), 0u);
    EXPECT_EQ(ints, (std::vector<int>{3, 1, 2}));
}

// ===========================================================================
// Array::Sort — .NET's NaN ordering and the strict-weak-ordering repair
// ===========================================================================

TEST(ArrayComparisonContractTests, Sort_Float_Puts_NaN_First) {
    std::vector<float> v{3.0f, kNaNf, 1.0f};
    Array::Sort(v);
    EXPECT_TRUE(std::isnan(v[0]));
    EXPECT_FLOAT_EQ(v[1], 1.0f);
    EXPECT_FLOAT_EQ(v[2], 3.0f);
}

TEST(ArrayComparisonContractTests, Sort_Double_Puts_NaN_First) {
    std::vector<double> v{3.0, kNaN, 1.0};
    Array::Sort(v);
    EXPECT_TRUE(std::isnan(v[0]));
    EXPECT_DOUBLE_EQ(v[1], 1.0);
    EXPECT_DOUBLE_EQ(v[2], 3.0);
}

TEST(ArrayComparisonContractTests, Sort_Puts_NaN_Below_NegativeInfinity) {
    std::vector<double> v{kInf, 0.0, kNaN, -kInf};
    Array::Sort(v);
    EXPECT_TRUE(std::isnan(v[0]));
    EXPECT_DOUBLE_EQ(v[1], -kInf);
    EXPECT_DOUBLE_EQ(v[2], 0.0);
    EXPECT_DOUBLE_EQ(v[3], kInf);
}

TEST(ArrayComparisonContractTests, Sort_Degenerate_Ranges) {
    std::vector<double> empty;
    Array::Sort(empty);
    EXPECT_TRUE(empty.empty());

    std::vector<double> one{kNaN};
    Array::Sort(one);
    EXPECT_TRUE(std::isnan(one[0]));

    std::vector<double> allNaN{kNaN, kNaN, kNaN};
    Array::Sort(allNaN);
    for (double x : allNaN) EXPECT_TRUE(std::isnan(x));
    EXPECT_EQ(allNaN.size(), 3u);
}

// The measured defect: a NaN-bearing range gives std::sort a comparator that is
// not a strict weak ordering, and the FINITE elements come out unsorted. 191
// inversions before the repair at this exact shape.
TEST(ArrayComparisonContractTests, Sort_LargeRange_Leaves_Finite_Elements_Sorted) {
    const int n = 4096;
    std::vector<float> v;
    v.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i)
        v.push_back((i % 3 == 1) ? kNaNf : static_cast<float>(n - i));
    Array::Sort(v);

    EXPECT_EQ(finiteInversions(v), 0);
    // Every NaN precedes every number.
    std::size_t nanCount = 0;
    while (nanCount < v.size() && std::isnan(v[nanCount])) ++nanCount;
    EXPECT_EQ(nanCount, 1365u);
    for (std::size_t i = nanCount; i < v.size(); ++i) EXPECT_FALSE(std::isnan(v[i]));
}

// >16 elements so introsort's insertion-sort phase runs; the smallest shape that
// can exercise __unguarded_linear_insert.
TEST(ArrayComparisonContractTests, Sort_JustOverInsertionSortThreshold) {
    std::vector<double> v;
    for (int i = 0; i < 24; ++i) v.push_back((i % 4 == 0) ? kNaN : static_cast<double>(24 - i));
    Array::Sort(v);
    EXPECT_EQ(finiteInversions(v), 0);
    for (int i = 0; i < 6; ++i) EXPECT_TRUE(std::isnan(v[static_cast<std::size_t>(i)]));
}

TEST(ArrayComparisonContractTests, Sort_Range_Overload_Honours_The_Same_Contract) {
    std::vector<double> v{9.0, 3.0, kNaN, 1.0, 8.0};
    Array::Sort(v, 1, 3);
    EXPECT_DOUBLE_EQ(v[0], 9.0);
    EXPECT_TRUE(std::isnan(v[1]));
    EXPECT_DOUBLE_EQ(v[2], 1.0);
    EXPECT_DOUBLE_EQ(v[3], 3.0);
    EXPECT_DOUBLE_EQ(v[4], 8.0);
}

TEST(ArrayComparisonContractTests, Sort_NonFloating_Types_Are_Unchanged) {
    std::vector<int> ints{3, 1, 4, 1, 5, 9, 2, 6};
    Array::Sort(ints);
    EXPECT_EQ(ints, (std::vector<int>{1, 1, 2, 3, 4, 5, 6, 9}));

    std::vector<std::string> strings{"pear", "apple", "fig"};
    Array::Sort(strings);
    EXPECT_EQ(strings, (std::vector<std::string>{"apple", "fig", "pear"}));

    std::vector<Widget> widgets{Widget{3}, Widget{1}, Widget{2}};
    Array::Sort(widgets);
    EXPECT_EQ(widgets[0].v, 1);
    EXPECT_EQ(widgets[1].v, 2);
    EXPECT_EQ(widgets[2].v, 3);
}

TEST(ArrayComparisonContractTests, Sort_With_Caller_Comparison_Is_Deliberately_Untouched) {
    // The caller-supplied-comparer overload passes the predicate straight
    // through, exactly as .NET's IntrospectiveSort does with a non-default
    // IComparer<T>. Pinned so a future reader does not "fix" it too.
    std::vector<double> v{3.0, 1.0, 2.0};
    Array::Sort(v, std::function<int(const double&, const double&)>(
                       [](const double& a, const double& b) { return a < b ? -1 : (b < a ? 1 : 0); }));
    EXPECT_DOUBLE_EQ(v[0], 1.0);
    EXPECT_DOUBLE_EQ(v[2], 3.0);
}

// ===========================================================================
// Array::BinarySearch
// ===========================================================================

TEST(ArrayComparisonContractTests, BinarySearch_Finds_NaN_In_DotNet_Order) {
    std::vector<float> v{kNaNf, 1.0f, 3.0f};
    EXPECT_EQ(Array::BinarySearch(v, kNaNf), 0);
    EXPECT_EQ(Array::BinarySearch(v, 1.0f), 1);
    EXPECT_EQ(Array::BinarySearch(v, 3.0f), 2);
}

TEST(ArrayComparisonContractTests, BinarySearch_Insertion_Point_For_Absent_Values) {
    std::vector<double> v{kNaN, 1.0, 3.0};
    EXPECT_EQ(Array::BinarySearch(v, 2.0), ~static_cast<intcs>(2));
    EXPECT_EQ(Array::BinarySearch(v, 9.0), ~static_cast<intcs>(3));
    // NaN sorts below everything, so an absent NaN would insert at 0.
    std::vector<double> noNaN{1.0, 3.0};
    EXPECT_EQ(Array::BinarySearch(noNaN, kNaN), ~static_cast<intcs>(0));
}

TEST(ArrayComparisonContractTests, BinarySearch_Range_Overload_Finds_NaN) {
    std::vector<double> v{99.0, kNaN, 1.0, 3.0, 99.0};
    EXPECT_EQ(Array::BinarySearch(v, 1, 3, kNaN), 1);
    EXPECT_EQ(Array::BinarySearch(v, 1, 3, 3.0), 3);
}

TEST(ArrayComparisonContractTests, BinarySearch_NonFloating_Unchanged) {
    std::vector<int> v{1, 3, 5, 7};
    EXPECT_EQ(Array::BinarySearch(v, 5), 2);
    EXPECT_EQ(Array::BinarySearch(v, 4), ~static_cast<intcs>(2));
}

// ===========================================================================
// Array::IndexOf / LastIndexOf
// ===========================================================================

TEST(ArrayComparisonContractTests, IndexOf_Finds_NaN) {
    std::vector<float> v{1.0f, kNaNf, 3.0f, kNaNf};
    EXPECT_EQ(Array::IndexOf(v, kNaNf), 1);
    EXPECT_EQ(Array::IndexOf(v, kNaNf, 2), 3);
    EXPECT_EQ(Array::IndexOf(v, kNaNf, 0, 1), -1);
    EXPECT_EQ(Array::LastIndexOf(v, kNaNf), 3);
    EXPECT_EQ(Array::LastIndexOf(v, kNaNf, 2), 1);
    EXPECT_EQ(Array::LastIndexOf(v, kNaNf, 2, 2), 1);
}

TEST(ArrayComparisonContractTests, IndexOf_SignedZero_Still_Matches) {
    std::vector<double> v{-0.0};
    EXPECT_EQ(Array::IndexOf(v, 0.0), 0);
    EXPECT_EQ(Array::LastIndexOf(v, 0.0), 0);
}

TEST(ArrayComparisonContractTests, IndexOf_NonFloating_Unchanged) {
    std::vector<int> v{4, 5, 6, 5};
    EXPECT_EQ(Array::IndexOf(v, 5), 1);
    EXPECT_EQ(Array::LastIndexOf(v, 5), 3);
    EXPECT_EQ(Array::IndexOf(v, 99), -1);

    std::vector<std::string> s{"a", "b"};
    EXPECT_EQ(Array::IndexOf(s, std::string("b")), 1);
}

// ===========================================================================
// MemoryExtensions — Sort, BinarySearch, SequenceCompareTo (ticket #1906)
// ===========================================================================

TEST(MemoryExtensionsComparisonContractTests, Sort_Float_Puts_NaN_First) {
    std::vector<float> v{3.0f, kNaNf, 1.0f};
    System::MemoryExtensions::Sort(System::Span<float>(v));
    EXPECT_TRUE(std::isnan(v[0]));
    EXPECT_FLOAT_EQ(v[1], 1.0f);
    EXPECT_FLOAT_EQ(v[2], 3.0f);
}

TEST(MemoryExtensionsComparisonContractTests, Sort_Puts_NaN_Below_NegativeInfinity) {
    std::vector<double> v{kInf, 0.0, kNaN, -kInf};
    System::MemoryExtensions::Sort(System::Span<double>(v));
    EXPECT_TRUE(std::isnan(v[0]));
    EXPECT_DOUBLE_EQ(v[1], -kInf);
    EXPECT_DOUBLE_EQ(v[3], kInf);
}

TEST(MemoryExtensionsComparisonContractTests, Sort_LargeRange_Leaves_Finite_Elements_Sorted) {
    const int n = 4096;
    std::vector<float> v;
    v.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i)
        v.push_back((i % 3 == 1) ? kNaNf : static_cast<float>(n - i));
    System::MemoryExtensions::Sort(System::Span<float>(v));
    EXPECT_EQ(finiteInversions(v), 0);
}

TEST(MemoryExtensionsComparisonContractTests, Sort_Degenerate_And_NonFloating) {
    std::vector<double> empty;
    System::MemoryExtensions::Sort(System::Span<double>(empty));
    EXPECT_TRUE(empty.empty());

    std::vector<double> allNaN{kNaN, kNaN};
    System::MemoryExtensions::Sort(System::Span<double>(allNaN));
    EXPECT_TRUE(std::isnan(allNaN[0]));
    EXPECT_TRUE(std::isnan(allNaN[1]));

    std::vector<int> ints{3, 1, 2};
    System::MemoryExtensions::Sort(System::Span<int>(ints));
    EXPECT_EQ(ints, (std::vector<int>{1, 2, 3}));
}

TEST(MemoryExtensionsComparisonContractTests, BinarySearch_Finds_NaN) {
    std::vector<float> v{kNaNf, 1.0f, 3.0f};
    System::ReadOnlySpan<float> ro(v.data(), static_cast<intcs>(v.size()));
    EXPECT_EQ(System::MemoryExtensions::BinarySearch(ro, kNaNf), 0);
    EXPECT_EQ(System::MemoryExtensions::BinarySearch(ro, 3.0f), 2);
    EXPECT_EQ(System::MemoryExtensions::BinarySearch(System::Span<float>(v), kNaNf), 0);
    // Absent NaN inserts at the front, because NaN sorts below everything.
    std::vector<float> noNaN{1.0f, 3.0f};
    System::ReadOnlySpan<float> ro2(noNaN.data(), 2);
    EXPECT_EQ(System::MemoryExtensions::BinarySearch(ro2, kNaNf), ~static_cast<intcs>(0));
}

TEST(MemoryExtensionsComparisonContractTests, SequenceCompareTo_NaN_Sorts_First) {
    std::vector<float> nan{kNaNf};
    std::vector<float> one{1.0f};
    System::ReadOnlySpan<float> a(nan.data(), 1);
    System::ReadOnlySpan<float> b(one.data(), 1);
    EXPECT_LT(System::MemoryExtensions::SequenceCompareTo(a, b), 0);
    EXPECT_GT(System::MemoryExtensions::SequenceCompareTo(b, a), 0);
}

TEST(MemoryExtensionsComparisonContractTests, SequenceCompareTo_TwoNaNs_Are_Equal) {
    // Already correct before ticket #1906 -- pinned so it cannot regress.
    std::vector<double> l{kNaN};
    std::vector<double> r{kNaN};
    System::ReadOnlySpan<double> a(l.data(), 1);
    System::ReadOnlySpan<double> b(r.data(), 1);
    EXPECT_EQ(System::MemoryExtensions::SequenceCompareTo(a, b), 0);
}

TEST(MemoryExtensionsComparisonContractTests, SequenceCompareTo_Length_Tail_And_NonFloating) {
    std::vector<int> shortv{1, 2};
    std::vector<int> longv{1, 2, 3};
    System::ReadOnlySpan<int> a(shortv.data(), 2);
    System::ReadOnlySpan<int> b(longv.data(), 3);
    EXPECT_LT(System::MemoryExtensions::SequenceCompareTo(a, b), 0);
    EXPECT_GT(System::MemoryExtensions::SequenceCompareTo(b, a), 0);
    EXPECT_EQ(System::MemoryExtensions::SequenceCompareTo(a, a), 0);
}

TEST(MemoryExtensionsComparisonContractTests, SequenceEqual_And_Contains_Still_Find_NaN) {
    // The equality half of the contract was already correct in this file; ticket
    // #1906 only re-pointed it at the shared policy. Pinned unchanged.
    std::vector<float> l{kNaNf};
    std::vector<float> r{kNaNf};
    System::ReadOnlySpan<float> a(l.data(), 1);
    System::ReadOnlySpan<float> b(r.data(), 1);
    EXPECT_TRUE(System::MemoryExtensions::SequenceEqual(a, b));
    EXPECT_TRUE(System::MemoryExtensions::Contains(a, kNaNf));
    EXPECT_EQ(System::MemoryExtensions::IndexOf(a, kNaNf), 0);
    EXPECT_EQ(System::MemoryExtensions::Count(a, kNaNf), 1);
}

// ===========================================================================
// Tuple / ValueTuple — CompareTo, equality and the hash invariant (#1908)
// ===========================================================================

TEST(TupleComparisonContractTests, Tuple1_CompareTo_Orders_NaN_First) {
    System::Tuple1<float> nan(kNaNf);
    System::Tuple1<float> one(1.0f);
    EXPECT_EQ(nan.CompareTo(one), -1);
    EXPECT_EQ(one.CompareTo(nan), 1);
    EXPECT_EQ(nan.CompareTo(System::Tuple1<float>(kNaNf)), 0);
    // NaN sorts below negative infinity, as Single.CompareTo requires.
    System::Tuple1<double> negInf(-kInf);
    EXPECT_EQ(System::Tuple1<double>(kNaN).CompareTo(negInf), -1);
}

TEST(TupleComparisonContractTests, Tuple1_Equality_Is_NaN_Reflexive) {
    System::Tuple1<float> a(kNaNf);
    System::Tuple1<float> b(kNaNf);
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);
    EXPECT_FALSE(a == System::Tuple1<float>(1.0f));
}

TEST(TupleComparisonContractTests, Tuple_NaN_In_Each_Component_Position) {
    EXPECT_EQ((System::Tuple2<int, float>(1, kNaNf).CompareTo(System::Tuple2<int, float>(1, 1.0f))), -1);
    EXPECT_EQ((System::Tuple3<int, int, double>(1, 1, kNaN)
                   .CompareTo(System::Tuple3<int, int, double>(1, 1, 1.0))), -1);
    EXPECT_EQ((System::Tuple2<float, int>(kNaNf, 9).CompareTo(System::Tuple2<float, int>(1.0f, 0))), -1);
    EXPECT_TRUE((System::Tuple2<int, float>(1, kNaNf) == System::Tuple2<int, float>(1, kNaNf)));
}

TEST(TupleComparisonContractTests, Tuple_Equal_Objects_Have_Equal_Hashes) {
    System::Tuple2<int, double> a(1, kNaN);
    System::Tuple2<int, double> b(1, std::nan("7"));
    ASSERT_TRUE(a == b);
    EXPECT_EQ(a.GetHashCode(), b.GetHashCode());

    System::Tuple1<double> pz(0.0);
    System::Tuple1<double> nz(-0.0);
    ASSERT_TRUE(pz == nz);
    EXPECT_EQ(pz.GetHashCode(), nz.GetHashCode());
}

TEST(TupleComparisonContractTests, Tuple_NonFloating_Unchanged) {
    EXPECT_EQ((System::Tuple2<int, int>(1, 2).CompareTo(System::Tuple2<int, int>(1, 3))), -1);
    EXPECT_EQ((System::Tuple2<int, int>(1, 3).CompareTo(System::Tuple2<int, int>(1, 3))), 0);
    EXPECT_TRUE((System::Tuple2<int, std::string>(1, "a") == System::Tuple2<int, std::string>(1, "a")));
    EXPECT_FALSE((System::Tuple2<int, std::string>(1, "a") == System::Tuple2<int, std::string>(1, "b")));
}

TEST(ValueTupleComparisonContractTests, CompareTo_Orders_NaN_First) {
    System::ValueTuple1<float> nan{kNaNf};
    System::ValueTuple1<float> one{1.0f};
    EXPECT_EQ(nan.CompareTo(one), -1);
    EXPECT_EQ(one.CompareTo(nan), 1);
    EXPECT_EQ(nan.CompareTo(System::ValueTuple1<float>{kNaNf}), 0);
    EXPECT_EQ((System::ValueTuple1<double>{kNaN}.CompareTo(System::ValueTuple1<double>{-kInf})), -1);
}

TEST(ValueTupleComparisonContractTests, Relational_Operators_Follow_CompareTo) {
    System::ValueTuple1<float> nan{kNaNf};
    System::ValueTuple1<float> one{1.0f};
    EXPECT_TRUE(nan < one);
    EXPECT_TRUE(nan <= one);
    EXPECT_FALSE(nan > one);
    EXPECT_FALSE(nan >= one);
    EXPECT_TRUE(nan <= System::ValueTuple1<float>{kNaNf});
    EXPECT_TRUE(nan >= System::ValueTuple1<float>{kNaNf});
}

TEST(ValueTupleComparisonContractTests, Equality_Is_NaN_Reflexive) {
    System::ValueTuple1<float> a{kNaNf};
    System::ValueTuple1<float> b{kNaNf};
    EXPECT_TRUE(a == b);
    EXPECT_TRUE(a.Equals(b));
    EXPECT_FALSE(a != b);
    EXPECT_FALSE((a == System::ValueTuple1<float>{1.0f}));
}

TEST(ValueTupleComparisonContractTests, NaN_In_First_Middle_And_Last_Component) {
    using VT4 = System::ValueTuple4<int, double, int, double>;
    EXPECT_EQ((VT4{1, kNaN, 1, 1.0}.CompareTo(VT4{1, 1.0, 1, 1.0})), -1);      // middle
    EXPECT_EQ((VT4{1, 1.0, 1, kNaN}.CompareTo(VT4{1, 1.0, 1, 1.0})), -1);      // last
    using VT2 = System::ValueTuple2<double, int>;
    EXPECT_EQ((VT2{kNaN, 9}.CompareTo(VT2{1.0, 0})), -1);                      // first
    EXPECT_TRUE((VT4{1, kNaN, 1, kNaN} == VT4{1, kNaN, 1, kNaN}));
}

TEST(ValueTupleComparisonContractTests, NaN_Inside_ValueTuple8_Rest) {
    using Rest = System::ValueTuple1<double>;
    using VT8 = System::ValueTuple8<int, int, int, int, int, int, int, Rest>;
    VT8 a{1, 2, 3, 4, 5, 6, 7, Rest{kNaN}};
    VT8 b{1, 2, 3, 4, 5, 6, 7, Rest{kNaN}};
    VT8 c{1, 2, 3, 4, 5, 6, 7, Rest{1.0}};
    EXPECT_TRUE(a == b);
    EXPECT_EQ(a.CompareTo(b), 0);
    EXPECT_EQ(a.CompareTo(c), -1);
    EXPECT_EQ(a.GetHashCode(), b.GetHashCode());
}

TEST(ValueTupleComparisonContractTests, Equal_Objects_Have_Equal_Hashes) {
    System::ValueTuple1<double> a{kNaN};
    System::ValueTuple1<double> b{std::nan("7")};
    ASSERT_TRUE(a == b);
    EXPECT_EQ(a.GetHashCode(), b.GetHashCode());

    System::ValueTuple2<int, double> pz{1, 0.0};
    System::ValueTuple2<int, double> nz{1, -0.0};
    ASSERT_TRUE(pz == nz);
    EXPECT_EQ(pz.GetHashCode(), nz.GetHashCode());
}

TEST(ValueTupleComparisonContractTests, NonFloating_Unchanged) {
    using VT2 = System::ValueTuple2<int, std::string>;
    EXPECT_EQ((VT2{1, "a"}.CompareTo(VT2{1, "b"})), -1);
    EXPECT_EQ((VT2{1, "b"}.CompareTo(VT2{1, "b"})), 0);
    EXPECT_EQ((VT2{2, "a"}.CompareTo(VT2{1, "z"})), 1);
    EXPECT_TRUE((VT2{1, "a"} == VT2{1, "a"}));
    EXPECT_FALSE((VT2{1, "a"} == VT2{1, "b"}));
}

TEST(ValueTupleComparisonContractTests, EmptyValueTuple_Is_Still_Constant_True) {
    System::ValueTuple a;
    System::ValueTuple b;
    EXPECT_TRUE(a == b);
    EXPECT_TRUE(a.Equals(b));
}
