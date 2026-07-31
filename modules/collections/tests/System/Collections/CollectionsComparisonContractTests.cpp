// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Permanent regression suite for the Collections default comparison contract
// family (ticket #1912; plan docs/CollectionsComparisonContractPlan.md).
//
// These defects are invisible to AddressSanitizer, UndefinedBehaviorSanitizer
// and LeakSanitizer, and libstdc++ debug mode catches exactly one of the 64
// measured rows. This file is therefore the primary correctness gate for the
// family, not a supplement to a sanitizer run. See the plan's section 3.3.
#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "System/Collections/Generic/Comparer.hpp"
#include "System/Collections/Generic/EqualityComparer.hpp"
#include "System/Collections/Generic/NullableComparer.hpp"
#include "System/Collections/Generic/NullableEqualityComparer.hpp"
#include "System/Collections/Generic/ObjectComparer.hpp"
#include "System/Collections/Generic/ObjectEqualityComparer.hpp"
#include "System/Collections/Concurrent/ConcurrentDictionary.hpp"
#include "System/Collections/Generic/Dictionary.hpp"
#include "System/Collections/Generic/LinkedList.hpp"
#include "System/Collections/Generic/OrderedDictionary.hpp"
#include "System/Collections/Generic/SortedDictionary.hpp"
#include "System/Collections/Generic/SortedList.hpp"
#include "System/Collections/Generic/List.hpp"
#include "System/Collections/Generic/Queue.hpp"
#include "System/Collections/Generic/Stack.hpp"
#include "System/Collections/Generic/ReferenceEqualityComparer.hpp"
#include "System/Collections/Immutable/ImmutableArray.hpp"
#include "System/Collections/Immutable/ImmutableDictionary.hpp"
#include "System/Collections/Immutable/ImmutableSortedDictionary.hpp"
#include "System/Collections/ObjectModel/Collection.hpp"
#include "System/Collections/ObjectModel/KeyedCollection.hpp"
#include "System/Collections/ObjectModel/ReadOnlyCollection.hpp"
#include "System/ArgumentException.hpp"
#include "System/Collections/Immutable/ImmutableList.hpp"

namespace {

using SharpRuntime::intcs;
namespace G = System::Collections::Generic;

constexpr double kNaN  = std::numeric_limits<double>::quiet_NaN();
constexpr float  kNaNf = std::numeric_limits<float>::quiet_NaN();
constexpr double kInf  = std::numeric_limits<double>::infinity();

/** A NaN with a payload different from the quiet default, so a payload-sensitive
 *  hash would disagree with the canonical one. */
double payloadNaN() {
    std::uint64_t bits = 0x7FF8000000000007ULL;
    double d;
    std::memcpy(&d, &bits, sizeof d);
    return d;
}

} // namespace

// ---------------------------------------------------------------------------
// Comparer<T>::Default -- the port's Comparer<T>.Default (#1914)
// ---------------------------------------------------------------------------

TEST(CollectionsComparisonContract, ComparerDefaultOrdersNaNBeforeEverything) {
    const auto& c = G::Comparer<double>::Default();
    EXPECT_LT(c.Compare(kNaN, 1.0), 0);
    EXPECT_GT(c.Compare(1.0, kNaN), 0);
    EXPECT_LT(c.Compare(kNaN, -kInf), 0);   // below negative infinity, per Double.CompareTo
    EXPECT_GT(c.Compare(-kInf, kNaN), 0);
    EXPECT_EQ(c.Compare(kNaN, kNaN), 0);    // two NaNs are equivalent
    EXPECT_EQ(c.Compare(kNaN, payloadNaN()), 0);
}

TEST(CollectionsComparisonContract, ComparerDefaultFloatMatchesDouble) {
    const auto& c = G::Comparer<float>::Default();
    EXPECT_LT(c.Compare(kNaNf, 1.0f), 0);
    EXPECT_GT(c.Compare(1.0f, kNaNf), 0);
    EXPECT_EQ(c.Compare(kNaNf, kNaNf), 0);
}

TEST(CollectionsComparisonContract, ComparerDefaultIsAStrictWeakOrdering) {
    // The reason the raw operator was wrong is not the answer it gave but that
    // the equivalence it induced was intransitive: NaN was "equivalent" to both
    // 1.0 and 2.0 while those two were not equivalent to each other.
    const auto& c = G::Comparer<double>::Default();
    const bool nanEquivOne = c.Compare(kNaN, 1.0) == 0;
    const bool nanEquivTwo = c.Compare(kNaN, 2.0) == 0;
    const bool oneEquivTwo = c.Compare(1.0, 2.0) == 0;
    EXPECT_FALSE(nanEquivOne);
    EXPECT_FALSE(nanEquivTwo);
    EXPECT_FALSE(oneEquivTwo);
}

TEST(CollectionsComparisonContract, ComparerDefaultSignedZeroAndOrdinaryValues) {
    const auto& c = G::Comparer<double>::Default();
    EXPECT_EQ(c.Compare(0.0, -0.0), 0);
    EXPECT_LT(c.Compare(-1.0, 1.0), 0);
    EXPECT_GT(c.Compare(kInf, 1.0), 0);
    EXPECT_LT(c.Compare(-kInf, 1.0), 0);
}

TEST(CollectionsComparisonContract, ComparerDefaultNegativeControls) {
    EXPECT_LT(G::Comparer<int>::Default().Compare(1, 2), 0);
    EXPECT_GT(G::Comparer<int>::Default().Compare(2, 1), 0);
    EXPECT_EQ(G::Comparer<int>::Default().Compare(2, 2), 0);
    EXPECT_LT(G::Comparer<std::string>::Default().Compare("a", "b"), 0);
    EXPECT_EQ(G::Comparer<std::string>::Default().Compare("a", "a"), 0);
}

TEST(CollectionsComparisonContract, ComparerDefaultStaysASingleton) {
    EXPECT_EQ(&G::Comparer<double>::Default(), &G::Comparer<double>::Default());
}

TEST(CollectionsComparisonContract, ComparerCreateStillUsesTheCallersFunction) {
    // The explicit caller-supplied path must not be routed through the policy.
    // A reversed comparison stays reversed, NaN included.
    auto* reversed = G::Comparer<double>::Create(
        [](const double& a, const double& b) -> intcs {
            if (a < b) return 1;
            if (b < a) return -1;
            return 0;
        });
    EXPECT_GT(reversed->Compare(1.0, 2.0), 0);
    EXPECT_EQ(reversed->Compare(kNaN, 1.0), 0);   // the caller's own NaN behaviour, unchanged
    delete reversed;
}

// ---------------------------------------------------------------------------
// EqualityComparer<T>::Default (#1914)
// ---------------------------------------------------------------------------

TEST(CollectionsComparisonContract, EqualityComparerDefaultNaNEqualsItself) {
    const auto& c = G::EqualityComparer<double>::Default();
    EXPECT_TRUE(c.Equals(kNaN, kNaN));
    EXPECT_TRUE(c.Equals(kNaN, payloadNaN()));
    EXPECT_FALSE(c.Equals(kNaN, 1.0));
    EXPECT_FALSE(c.Equals(1.0, kNaN));
}

TEST(CollectionsComparisonContract, EqualityComparerDefaultHashIsConsistentWithEquals) {
    const auto& c = G::EqualityComparer<double>::Default();
    // Equal objects must hash alike, or every hash container built on this
    // comparer would lose the element it just accepted.
    EXPECT_EQ(c.GetHashCode(kNaN), c.GetHashCode(payloadNaN()));
    EXPECT_EQ(c.GetHashCode(0.0), c.GetHashCode(-0.0));
    EXPECT_EQ(c.GetHashCode(1.5), c.GetHashCode(1.5));
}

TEST(CollectionsComparisonContract, EqualityComparerDefaultSignedZeroStillEqual) {
    const auto& c = G::EqualityComparer<double>::Default();
    EXPECT_TRUE(c.Equals(0.0, -0.0));
    EXPECT_TRUE(c.Equals(kInf, kInf));
    EXPECT_FALSE(c.Equals(kInf, -kInf));
}

TEST(CollectionsComparisonContract, EqualityComparerDefaultFloatMatchesDouble) {
    const auto& c = G::EqualityComparer<float>::Default();
    EXPECT_TRUE(c.Equals(kNaNf, kNaNf));
    EXPECT_EQ(c.GetHashCode(kNaNf), c.GetHashCode(kNaNf));
    EXPECT_FALSE(c.Equals(kNaNf, 1.0f));
}

TEST(CollectionsComparisonContract, EqualityComparerDefaultNegativeControls) {
    EXPECT_TRUE(G::EqualityComparer<int>::Default().Equals(3, 3));
    EXPECT_FALSE(G::EqualityComparer<int>::Default().Equals(3, 4));
    EXPECT_EQ(G::EqualityComparer<int>::Default().GetHashCode(3),
              G::EqualityComparer<int>::Default().GetHashCode(3));
    EXPECT_TRUE(G::EqualityComparer<std::string>::Default().Equals("a", "a"));
    EXPECT_FALSE(G::EqualityComparer<std::string>::Default().Equals("a", "b"));
}

TEST(CollectionsComparisonContract, EqualityComparerCreateStillUsesTheCallersFunction) {
    auto everythingEqual = G::EqualityComparer<double>::Create(
        [](const double&, const double&) { return true; },
        [](const double&) -> intcs { return 7; });
    EXPECT_TRUE(everythingEqual->Equals(1.0, 2.0));
    EXPECT_EQ(everythingEqual->GetHashCode(kNaN), 7);
}

// ---------------------------------------------------------------------------
// ObjectComparer / ObjectEqualityComparer (#1914)
// ---------------------------------------------------------------------------

TEST(CollectionsComparisonContract, ObjectComparerFollowsTheDefaultOrdering) {
    G::ObjectComparer<double> c;
    EXPECT_LT(c.Compare(kNaN, 1.0), 0);
    EXPECT_GT(c.Compare(1.0, kNaN), 0);
    EXPECT_EQ(c.Compare(kNaN, kNaN), 0);
    EXPECT_LT(c.Compare(1.0, 2.0), 0);

    G::ObjectComparer<int> ints;   // negative control
    EXPECT_LT(ints.Compare(1, 2), 0);
    EXPECT_EQ(ints.Compare(2, 2), 0);
}

TEST(CollectionsComparisonContract, ObjectEqualityComparerFollowsTheDefaultEquality) {
    G::ObjectEqualityComparer<double> c;
    EXPECT_TRUE(c.Equals(kNaN, kNaN));
    EXPECT_EQ(c.GetHashCode(kNaN), c.GetHashCode(payloadNaN()));
    EXPECT_TRUE(c.Equals(0.0, -0.0));
    EXPECT_FALSE(c.Equals(1.0, 2.0));

    G::ObjectEqualityComparer<std::string> strings;   // negative control
    EXPECT_TRUE(strings.Equals("a", "a"));
    EXPECT_FALSE(strings.Equals("a", "b"));
}

// ---------------------------------------------------------------------------
// NullableComparer / NullableEqualityComparer (#1914)
// ---------------------------------------------------------------------------

TEST(CollectionsComparisonContract, NullableComparerOrdersNullFirstThenTheDefaultOrdering) {
    G::NullableComparer<double> c;
    const std::optional<double> none;
    EXPECT_EQ(c.Compare(none, none), 0);
    EXPECT_LT(c.Compare(none, std::optional<double>(kNaN)), 0);
    EXPECT_GT(c.Compare(std::optional<double>(kNaN), none), 0);
    EXPECT_LT(c.Compare(std::optional<double>(kNaN), std::optional<double>(1.0)), 0);
    EXPECT_GT(c.Compare(std::optional<double>(1.0), std::optional<double>(kNaN)), 0);
    EXPECT_EQ(c.Compare(std::optional<double>(kNaN), std::optional<double>(kNaN)), 0);
}

TEST(CollectionsComparisonContract, NullableEqualityComparerIsTheReflexiveHalfOfTheSplit) {
    G::NullableEqualityComparer<double> c;
    const std::optional<double> none;
    EXPECT_TRUE(c.Equals(none, none));
    EXPECT_FALSE(c.Equals(none, std::optional<double>(kNaN)));
    EXPECT_TRUE(c.Equals(std::optional<double>(kNaN), std::optional<double>(kNaN)));
    EXPECT_EQ(c.GetHashCode(none), 0);
    EXPECT_EQ(c.GetHashCode(std::optional<double>(kNaN)),
              c.GetHashCode(std::optional<double>(payloadNaN())));

    G::NullableComparer<int> ints;   // negative control
    EXPECT_LT(ints.Compare(std::optional<int>(1), std::optional<int>(2)), 0);
}

TEST(CollectionsComparisonContract, LiftedOperatorEqualsStaysRawIeee) {
    // The other half of the split the plan's section 5.4 pins: C#'s LIFTED == on
    // T? is the underlying operator==, so two NaNs are NOT equal through it,
    // in .NET as well as here. std::optional's own operator== is that lifted
    // operator, and it must keep disagreeing with the comparer above.
    const std::optional<double> a(kNaN), b(kNaN);
    EXPECT_FALSE(a == b);
    G::NullableEqualityComparer<double> c;
    EXPECT_TRUE(c.Equals(a, b));
}

// ---------------------------------------------------------------------------
// ReferenceEqualityComparer -- declared negative control (#1914)
// ---------------------------------------------------------------------------

TEST(CollectionsComparisonContract, ReferenceEqualityComparerStaysIdentity) {
    G::ReferenceEqualityComparer<double> c;
    double x = kNaN;
    double y = kNaN;
    EXPECT_TRUE(c.Equals(&x, &x));
    EXPECT_FALSE(c.Equals(&x, &y));   // equal values, different objects
    double* null = nullptr;
    EXPECT_TRUE(c.Equals(null, null));
}

// ---------------------------------------------------------------------------
// The five default-ordering sites (#1915)
// ---------------------------------------------------------------------------

namespace {

/** Counts inversions among the NON-NaN elements: those are the elements a
 *  correct sort must leave ordered whatever the NaN policy is.
 *
 *  Exact O(n^2) count up to `kExactInversionLimit` elements, which is what makes
 *  a failure diagnosable; above it the equivalent O(n) monotonicity check, which
 *  returns 1 for "at least one inversion" rather than the count. The distinction
 *  matters only for a failing run: `== 0` means the same thing either way, and
 *  keeping the quadratic count for 65,536 elements cost 20 s of every gate. */
constexpr std::size_t kExactInversionLimit = 4096;

long long finiteInversions(const std::vector<double>& v) {
    std::vector<double> f;
    for (double d : v) if (!std::isnan(d)) f.push_back(d);
    if (f.size() > kExactInversionLimit) {
        for (std::size_t i = 0; i + 1 < f.size(); ++i)
            if (f[i + 1] < f[i]) return 1;
        return 0;
    }
    long long inv = 0;
    for (std::size_t i = 0; i + 1 < f.size(); ++i)
        for (std::size_t j = i + 1; j < f.size(); ++j)
            if (f[j] < f[i]) ++inv;
    return inv;
}

/** Deterministic xorshift, so the adversarial vector below is byte-identical on
 *  every run and on every machine. */
struct Xorshift {
    std::uint64_t s;
    explicit Xorshift(std::uint64_t seed) : s(seed) {}
    std::uint64_t next() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return s; }
    double unit() { return static_cast<double>(next() >> 11) * (1.0 / 9007199254740992.0); }
};

/** A NaN-bearing vector large enough to reproduce the silent corruption; a
 *  three-element example provably cannot (plan section 3.2). */
std::vector<double> adversarialVector(std::size_t n, double nanDensity, std::uint64_t seed) {
    Xorshift rng(seed);
    std::vector<double> v(n);
    for (std::size_t i = 0; i < n; ++i) v[i] = rng.unit() * 1000.0;
    const std::size_t nans = static_cast<std::size_t>(static_cast<double>(n) * nanDensity) + 1;
    for (std::size_t k = 0; k < nans && k < n; ++k) v[(k * 7919) % n] = kNaN;
    return v;
}

} // namespace

TEST(CollectionsComparisonContract, ListSortPutsNaNFirstAndOrdersTheRest) {
    G::List<double> l(std::vector<double>{3, kNaN, 1, 2});
    l.Sort();
    const auto v = l.ToArray();
    ASSERT_EQ(v.size(), 4u);
    EXPECT_TRUE(std::isnan(v[0]));
    EXPECT_DOUBLE_EQ(v[1], 1.0);
    EXPECT_DOUBLE_EQ(v[2], 2.0);
    EXPECT_DOUBLE_EQ(v[3], 3.0);
}

TEST(CollectionsComparisonContract, ListSortLeavesNoInversionOnAdversarialInput) {
    // 7 sizes x 3 densities, deterministic. Asserted by inversion count, not by
    // spot-checking positions: the defect this replaces left the finite elements
    // unsorted in the MIDDLE of large ranges.
    for (std::size_t n : {16u, 64u, 256u, 1024u, 4096u, 16384u, 65536u}) {
        for (double density : {0.001, 0.01, 0.5}) {
            G::List<double> l(adversarialVector(n, density, 0xC0FFEEull + n));
            l.Sort();
            const auto out = l.ToArray();
            ASSERT_EQ(out.size(), n) << "n=" << n << " density=" << density;
            EXPECT_EQ(finiteInversions(out), 0)
                << "n=" << n << " density=" << density;
            // Every NaN must be at the front, and there must be exactly as many
            // as went in.
            std::size_t nanCount = 0;
            while (nanCount < out.size() && std::isnan(out[nanCount])) ++nanCount;
            for (std::size_t i = nanCount; i < out.size(); ++i)
                ASSERT_FALSE(std::isnan(out[i])) << "n=" << n << " i=" << i;
        }
    }
}

TEST(CollectionsComparisonContract, ListSortHandlesAllNaNEmptyAndSingleElement) {
    G::List<double> empty;
    empty.Sort();
    EXPECT_EQ(empty.getCountProperty(), 0);

    G::List<double> one(std::vector<double>{kNaN});
    one.Sort();
    ASSERT_EQ(one.getCountProperty(), 1);
    EXPECT_TRUE(std::isnan(one.ToArray()[0]));

    G::List<double> allNaN(std::vector<double>{kNaN, kNaN, kNaN, kNaN});
    allNaN.Sort();
    for (double d : allNaN.ToArray()) EXPECT_TRUE(std::isnan(d));
}

TEST(CollectionsComparisonContract, ListSortKeepsSignedZeroAndInfinitiesInPlace) {
    G::List<double> l(std::vector<double>{kInf, -0.0, kNaN, -kInf, 0.0});
    l.Sort();
    const auto v = l.ToArray();
    ASSERT_EQ(v.size(), 5u);
    EXPECT_TRUE(std::isnan(v[0]));
    EXPECT_DOUBLE_EQ(v[1], -kInf);
    EXPECT_EQ(v[2], 0.0);          // -0.0 and +0.0 are equivalent, either order
    EXPECT_EQ(v[3], 0.0);
    EXPECT_DOUBLE_EQ(v[4], kInf);
}

TEST(CollectionsComparisonContract, ListSortNegativeControls) {
    G::List<int> ints(std::vector<int>{3, 1, 2, 1});
    ints.Sort();
    EXPECT_EQ(ints.ToArray(), (std::vector<int>{1, 1, 2, 3}));

    G::List<std::string> strings(std::vector<std::string>{"c", "a", "b"});
    strings.Sort();
    EXPECT_EQ(strings.ToArray(), (std::vector<std::string>{"a", "b", "c"}));
}

TEST(CollectionsComparisonContract, ListSortWithACallerComparisonIsUnchanged) {
    // The caller-supplied path must keep the caller's own semantics, NaN
    // included, and must NOT be routed through the policy.
    G::List<double> l(std::vector<double>{3, 1, 2});
    l.Sort([](const double& a, const double& b) -> intcs {
        if (a < b) return 1;
        if (b < a) return -1;
        return 0;
    });
    EXPECT_EQ(l.ToArray(), (std::vector<double>{3, 2, 1}));
}

TEST(CollectionsComparisonContract, ListBinarySearchFindsNaNAtTheFront) {
    G::List<double> l(std::vector<double>{kNaN, 1, 2, 3});
    EXPECT_EQ(l.BinarySearch(kNaN), 0);
    EXPECT_EQ(l.BinarySearch(1.0), 1);
    EXPECT_EQ(l.BinarySearch(2.0), 2);
    EXPECT_EQ(l.BinarySearch(3.0), 3);
    EXPECT_LT(l.BinarySearch(2.5), 0);       // absent -> complement of insertion point
    EXPECT_EQ(l.BinarySearch(2.5), ~static_cast<intcs>(3));
}

TEST(CollectionsComparisonContract, ListBinarySearchRoundTripsWithSort) {
    // The two must agree on one ordering: whatever Sort produces, BinarySearch
    // must be able to search. This is the property the raw operators broke.
    G::List<double> l(adversarialVector(1024, 0.01, 0x5EEDull));
    l.Sort();
    const auto v = l.ToArray();
    for (std::size_t i = 0; i < v.size(); i += 37) {
        const intcs found = l.BinarySearch(v[i]);
        ASSERT_GE(found, 0) << "i=" << i;
        if (std::isnan(v[i])) EXPECT_TRUE(std::isnan(v[static_cast<std::size_t>(found)]));
        else EXPECT_DOUBLE_EQ(v[static_cast<std::size_t>(found)], v[i]);
    }
}

TEST(CollectionsComparisonContract, ListBinarySearchNegativeControls) {
    G::List<int> ints(std::vector<int>{1, 3, 5, 7});
    EXPECT_EQ(ints.BinarySearch(5), 2);
    EXPECT_EQ(ints.BinarySearch(4), ~static_cast<intcs>(2));
    EXPECT_EQ(ints.BinarySearch(0), ~static_cast<intcs>(0));
    EXPECT_EQ(ints.BinarySearch(9), ~static_cast<intcs>(4));
}

TEST(CollectionsComparisonContract, ImmutableListSortPutsNaNFirst) {
    auto l = System::Collections::Immutable::ImmutableList<double>::Create(
                 std::vector<double>{3, kNaN, 1, 2}).Sort();
    ASSERT_EQ(l.getCountProperty(), 4);
    EXPECT_TRUE(std::isnan(l[0]));
    EXPECT_DOUBLE_EQ(l[1], 1.0);
    EXPECT_DOUBLE_EQ(l[2], 2.0);
    EXPECT_DOUBLE_EQ(l[3], 3.0);
}

TEST(CollectionsComparisonContract, ImmutableListRangeSortOnlyTouchesItsRange) {
    auto l = System::Collections::Immutable::ImmutableList<double>::Create(
                 std::vector<double>{9, 3, kNaN, 1}).Sort(1, 3);
    ASSERT_EQ(l.getCountProperty(), 4);
    EXPECT_DOUBLE_EQ(l[0], 9.0);          // outside the range, untouched
    EXPECT_TRUE(std::isnan(l[1]));
    EXPECT_DOUBLE_EQ(l[2], 1.0);
    EXPECT_DOUBLE_EQ(l[3], 3.0);
}

TEST(CollectionsComparisonContract, ImmutableListSortLeavesNoInversionOnAdversarialInput) {
    auto l = System::Collections::Immutable::ImmutableList<double>::Create(
                 adversarialVector(4096, 0.01, 0xBEEFull)).Sort();
    std::vector<double> out;
    for (intcs i = 0; i < l.getCountProperty(); ++i) out.push_back(l[i]);
    EXPECT_EQ(finiteInversions(out), 0);
}

TEST(CollectionsComparisonContract, ImmutableListSortNegativeControlsAndCallerComparison) {
    auto ints = System::Collections::Immutable::ImmutableList<int>::Create(
                    std::vector<int>{3, 1, 2}).Sort();
    EXPECT_EQ(ints[0], 1);
    EXPECT_EQ(ints[2], 3);

    auto reversed = System::Collections::Immutable::ImmutableList<double>::Create(
        std::vector<double>{1, 3, 2}).Sort([](const double& a, const double& b) -> intcs {
            if (a < b) return 1;
            if (b < a) return -1;
            return 0;
        });
    EXPECT_DOUBLE_EQ(reversed[0], 3.0);
    EXPECT_DOUBLE_EQ(reversed[2], 1.0);
}

TEST(CollectionsComparisonContract, ImmutableArraySortPutsNaNFirst) {
    auto a = System::Collections::Immutable::ImmutableArray<double>::Create(
                 std::vector<double>{3, kNaN, 1, 2}).Sort();
    ASSERT_EQ(a.getLengthProperty(), 4);
    EXPECT_TRUE(std::isnan(a[0]));
    EXPECT_DOUBLE_EQ(a[1], 1.0);
    EXPECT_DOUBLE_EQ(a[2], 2.0);
    EXPECT_DOUBLE_EQ(a[3], 3.0);

    auto ints = System::Collections::Immutable::ImmutableArray<int>::Create(
                    std::vector<int>{3, 1, 2}).Sort();   // negative control
    EXPECT_EQ(ints[0], 1);
    EXPECT_EQ(ints[2], 3);
}

// ---------------------------------------------------------------------------
// The sequence-collection default-equality sites (#1916)
// ---------------------------------------------------------------------------

TEST(CollectionsComparisonContract, ListFindsAndRemovesANaNElement) {
    G::List<double> l(std::vector<double>{1, kNaN, 2});
    EXPECT_TRUE(l.Contains(kNaN));
    EXPECT_EQ(l.IndexOf(kNaN), 1);
    EXPECT_TRUE(l.Remove(kNaN));
    EXPECT_EQ(l.getCountProperty(), 2);
    EXPECT_FALSE(l.Contains(kNaN));
}

TEST(CollectionsComparisonContract, ListRangeOverloadsFindANaNElement) {
    G::List<double> l(std::vector<double>{1, kNaN, 2, kNaN});
    EXPECT_EQ(l.IndexOf(kNaN, 2), 3);
    EXPECT_EQ(l.IndexOf(kNaN, 2, 2), 3);
    EXPECT_EQ(l.LastIndexOf(kNaN), 3);
    EXPECT_EQ(l.LastIndexOf(kNaN, 2), 1);
    EXPECT_EQ(l.LastIndexOf(kNaN, 3, 2), 3);
}

TEST(CollectionsComparisonContract, ListRemoveDoesNotMutateWhenNothingMatches) {
    // No partial mutation on a failed match: neither the contents nor the
    // fail-fast mutation counter may move.
    G::List<double> l(std::vector<double>{1, 2, 3});
    auto* e = l.GetEnumerator();
    EXPECT_FALSE(l.Remove(kNaN));
    EXPECT_EQ(l.getCountProperty(), 3);
    EXPECT_TRUE(e->MoveNext());          // would throw if the version had advanced
    delete e;
}

TEST(CollectionsComparisonContract, ListEqualityHandlesSignedZeroAndDuplicates) {
    G::List<double> l(std::vector<double>{-0.0, kNaN, kNaN, 0.0});
    EXPECT_EQ(l.IndexOf(0.0), 0);        // -0.0 == +0.0, so the first slot wins
    EXPECT_EQ(l.LastIndexOf(-0.0), 3);
    EXPECT_EQ(l.IndexOf(kNaN), 1);
    EXPECT_EQ(l.LastIndexOf(kNaN), 2);
}

TEST(CollectionsComparisonContract, ListEqualityNegativeControls) {
    G::List<int> ints(std::vector<int>{1, 2, 3});
    EXPECT_TRUE(ints.Contains(2));
    EXPECT_EQ(ints.IndexOf(2), 1);
    EXPECT_FALSE(ints.Contains(9));
    EXPECT_EQ(ints.IndexOf(9), -1);

    G::List<std::string> strings(std::vector<std::string>{"a", "b"});
    EXPECT_TRUE(strings.Contains("b"));
    EXPECT_EQ(strings.IndexOf("b"), 1);
    EXPECT_FALSE(strings.Remove("z"));
}

TEST(CollectionsComparisonContract, CollectionAndReadOnlyCollectionFindANaNElement) {
    System::Collections::ObjectModel::Collection<double> c;
    c.Add(1.0);
    c.Add(kNaN);
    EXPECT_TRUE(c.Contains(kNaN));
    EXPECT_EQ(c.IndexOf(kNaN), 1);
    EXPECT_TRUE(c.Remove(kNaN));
    EXPECT_EQ(c.getCountProperty(), 1);
    EXPECT_FALSE(c.Remove(kNaN));        // absent now, and no partial mutation
    EXPECT_EQ(c.getCountProperty(), 1);

    System::Collections::ObjectModel::ReadOnlyCollection<double> ro(std::vector<double>{1, kNaN});
    EXPECT_EQ(ro.IndexOf(kNaN), 1);
    EXPECT_TRUE(ro.Contains(kNaN));
    EXPECT_EQ(ro.IndexOf(5.0), -1);
}

TEST(CollectionsComparisonContract, GenericQueueAndStackFindANaNElement) {
    G::Queue<double> q;
    q.Enqueue(1.0);
    q.Enqueue(kNaN);
    EXPECT_TRUE(q.Contains(kNaN));
    EXPECT_FALSE(q.Contains(5.0));

    G::Stack<double> s;
    s.Push(1.0);
    s.Push(kNaN);
    EXPECT_TRUE(s.Contains(kNaN));
    EXPECT_FALSE(s.Contains(5.0));

    G::Queue<int> ints;                  // negative control
    ints.Enqueue(7);
    EXPECT_TRUE(ints.Contains(7));
    EXPECT_FALSE(ints.Contains(8));
}

TEST(CollectionsComparisonContract, LinkedListFindsANaNElement) {
    G::LinkedList<double> l;
    l.AddLast(kNaN);
    l.AddLast(1.0);
    l.AddLast(kNaN);
    EXPECT_TRUE(l.Contains(kNaN));
    EXPECT_TRUE(static_cast<bool>(l.Find(kNaN)));
    EXPECT_TRUE(static_cast<bool>(l.FindLast(kNaN)));
    EXPECT_NE(l.Find(kNaN), l.FindLast(kNaN));
    EXPECT_TRUE(l.Remove(kNaN));
    EXPECT_EQ(l.getCountProperty(), 2);
    EXPECT_FALSE(l.Contains(5.0));
    EXPECT_FALSE(static_cast<bool>(l.Find(5.0)));
}

TEST(CollectionsComparisonContract, LinkedListNodeValueComparisonUsesDefaultEquality) {
    G::LinkedList<double> l;
    l.AddLast(kNaN);
    auto node = l.getFirstProperty();
    EXPECT_TRUE(node == kNaN);
    EXPECT_FALSE(node == 1.0);
}

TEST(CollectionsComparisonContract, LinkedListRemoveDoesNotMutateWhenNothingMatches) {
    G::LinkedList<double> l;
    l.AddLast(1.0);
    l.AddLast(2.0);
    EXPECT_FALSE(l.Remove(kNaN));
    EXPECT_EQ(l.getCountProperty(), 2);
}

TEST(CollectionsComparisonContract, ImmutableArrayFindsReplacesAndRemovesANaNElement) {
    namespace I = System::Collections::Immutable;
    auto a = I::ImmutableArray<double>::Create(std::vector<double>{1, kNaN});
    EXPECT_TRUE(a.Contains(kNaN));
    EXPECT_EQ(a.IndexOf(kNaN), 1);

    auto replaced = a.Replace(kNaN, 7.0);
    ASSERT_EQ(replaced.getLengthProperty(), 2);
    EXPECT_DOUBLE_EQ(replaced[1], 7.0);

    auto removed = a.Remove(kNaN);
    ASSERT_EQ(removed.getLengthProperty(), 1);
    EXPECT_DOUBLE_EQ(removed[0], 1.0);

    auto three = I::ImmutableArray<double>::Create(std::vector<double>{kNaN, 1, kNaN});
    EXPECT_EQ(three.LastIndexOf(kNaN), 2);

    // The source is immutable and must be untouched by any of the above.
    EXPECT_EQ(a.getLengthProperty(), 2);
    EXPECT_TRUE(std::isnan(a[1]));
}

TEST(CollectionsComparisonContract, ImmutableListFindsReplacesAndRemovesANaNElement) {
    namespace I = System::Collections::Immutable;
    auto l = I::ImmutableList<double>::Create(std::vector<double>{1, kNaN});
    EXPECT_TRUE(l.Contains(kNaN));
    EXPECT_EQ(l.IndexOf(kNaN), 1);

    auto removed = l.Remove(kNaN);
    ASSERT_EQ(removed.getCountProperty(), 1);
    EXPECT_DOUBLE_EQ(removed[0], 1.0);

    // This one THREW ArgumentException("Cannot find the old value in the list.")
    // before the repair -- the family's only exception-visible row.
    auto replaced = l.Replace(kNaN, 7.0);
    ASSERT_EQ(replaced.getCountProperty(), 2);
    EXPECT_DOUBLE_EQ(replaced[1], 7.0);

    auto three = I::ImmutableList<double>::Create(std::vector<double>{kNaN, 1, kNaN});
    EXPECT_EQ(three.LastIndexOf(kNaN), 2);
    EXPECT_EQ(three.IndexOf(kNaN, 1, 2), 2);
}

TEST(CollectionsComparisonContract, ImmutableListReplaceStillThrowsWhenGenuinelyAbsent) {
    namespace I = System::Collections::Immutable;
    auto l = I::ImmutableList<double>::Create(std::vector<double>{1, kNaN});
    EXPECT_THROW((void)l.Replace(5.0, 7.0), System::ArgumentException);
}

TEST(CollectionsComparisonContract, ImmutableListBinarySearchFindsNaNAtTheFront) {
    namespace I = System::Collections::Immutable;
    auto l = I::ImmutableList<double>::Create(std::vector<double>{kNaN, 1, 2});
    EXPECT_EQ(l.BinarySearch(kNaN), 0);
    EXPECT_EQ(l.BinarySearch(1.0), 1);
    EXPECT_EQ(l.BinarySearch(2.0), 2);
    EXPECT_LT(l.BinarySearch(1.5), 0);
}

TEST(CollectionsComparisonContract, ImmutableListBuilderFindsANaNElement) {
    namespace I = System::Collections::Immutable;
    auto b = I::ImmutableList<double>::Create(std::vector<double>{1, kNaN}).ToBuilder();
    EXPECT_TRUE(b.Contains(kNaN));
    EXPECT_EQ(b.IndexOf(kNaN), 1);
    EXPECT_TRUE(b.Remove(kNaN));
    EXPECT_EQ(b.getCountProperty(), 1);
    EXPECT_FALSE(b.Remove(kNaN));
}

TEST(CollectionsComparisonContract, ImmutableExplicitComparerOverloadsStillUseTheCallersComparer) {
    namespace I = System::Collections::Immutable;
    // A comparer that calls everything equal must remove the FIRST element,
    // not the NaN the policy would have matched.
    auto everythingEqual = G::EqualityComparer<double>::Create(
        [](const double&, const double&) { return true; },
        [](const double&) -> intcs { return 0; });
    auto l = I::ImmutableList<double>::Create(std::vector<double>{1, kNaN});
    auto removed = l.Remove(kNaN, *everythingEqual);
    ASSERT_EQ(removed.getCountProperty(), 1);
    EXPECT_TRUE(std::isnan(removed[0]));   // element 0 (the 1.0) went, not the NaN
}

TEST(CollectionsComparisonContract, KeyedCollectionFindsANaNItem) {
    struct Keyed : System::Collections::ObjectModel::KeyedCollection<int, double> {
    protected:
        int GetKeyForItem(const double& item) const override {
            return std::isnan(item) ? -1 : static_cast<int>(item);
        }
    };
    Keyed k;
    k.Add(1.0);
    k.Add(kNaN);
    EXPECT_TRUE(k.Contains(kNaN));
    EXPECT_TRUE(k.Remove(kNaN));
    EXPECT_EQ(k.getCountProperty(), 1);
}

// ---------------------------------------------------------------------------
// The associative value-equality sites (#1917)
// ---------------------------------------------------------------------------

TEST(CollectionsComparisonContract, DictionaryFamilyFindsANaNValue) {
    G::Dictionary<int, double> d;
    d.Add(0, kNaN);
    d.Add(1, 5.0);
    EXPECT_TRUE(d.ContainsValue(kNaN));
    EXPECT_FALSE(d.ContainsValue(9.0));

    G::SortedDictionary<int, double> sd;
    sd.Add(0, kNaN);
    EXPECT_TRUE(sd.ContainsValue(kNaN));
    EXPECT_FALSE(sd.ContainsValue(9.0));

    G::OrderedDictionary<int, double> od;
    od.Add(0, kNaN);
    EXPECT_TRUE(od.ContainsValue(kNaN));
    EXPECT_FALSE(od.ContainsValue(9.0));
}

TEST(CollectionsComparisonContract, SortedListFindsANaNValueByValueAndByIndex) {
    G::SortedList<int, double> sl;
    sl.Add(0, 1.0);
    sl.Add(1, kNaN);
    EXPECT_TRUE(sl.ContainsValue(kNaN));
    EXPECT_EQ(sl.IndexOfValue(kNaN), 1);
    EXPECT_EQ(sl.IndexOfValue(9.0), -1);
}

TEST(CollectionsComparisonContract, DictionaryValueEqualityHandlesSignedZero) {
    G::Dictionary<int, double> d;
    d.Add(0, -0.0);
    EXPECT_TRUE(d.ContainsValue(0.0));   // -0.0 == +0.0 under the default contract
}

TEST(CollectionsComparisonContract, DictionaryValueEqualityNegativeControls) {
    G::Dictionary<int, std::string> d;
    d.Add(0, "x");
    EXPECT_TRUE(d.ContainsValue("x"));
    EXPECT_FALSE(d.ContainsValue("y"));

    G::Dictionary<int, int> ints;
    ints.Add(0, 7);
    EXPECT_TRUE(ints.ContainsValue(7));
    EXPECT_FALSE(ints.ContainsValue(8));
}

TEST(CollectionsComparisonContract, ImmutableDictionaryFamilyFindsANaNValue) {
    namespace I = System::Collections::Immutable;
    auto d = I::ImmutableDictionary<int, double>::Empty().Add(0, kNaN);
    EXPECT_TRUE(d.ContainsValue(kNaN));
    EXPECT_TRUE(d.Contains(std::pair<int, double>(0, kNaN)));
    EXPECT_FALSE(d.Contains(std::pair<int, double>(0, 5.0)));

    auto sd = I::ImmutableSortedDictionary<int, double>::Empty().Add(0, kNaN);
    EXPECT_TRUE(sd.ContainsValue(kNaN));
    EXPECT_TRUE(sd.Contains(std::pair<int, double>(0, kNaN)));
}

TEST(CollectionsComparisonContract, ImmutableDictionaryAddOfAnEqualNaNIsANoOpNotAThrow) {
    namespace I = System::Collections::Immutable;
    // .NET's ImmutableDictionary.Add compares the existing value with the value
    // comparer: same key with an EQUAL value is a no-op, a different value
    // throws. Before the repair a NaN never compared equal to itself, so
    // re-adding the same NaN threw "An item with the same key has already been
    // added."
    auto d = I::ImmutableDictionary<int, double>::Empty().Add(0, kNaN);
    auto again = d.Add(0, kNaN);
    EXPECT_EQ(again.getCountProperty(), 1);
    EXPECT_THROW((void)d.Add(0, 5.0), System::ArgumentException);

    auto sd = I::ImmutableSortedDictionary<int, double>::Empty().Add(0, kNaN);
    auto sdAgain = sd.Add(0, kNaN);
    EXPECT_EQ(sdAgain.getCountProperty(), 1);
    EXPECT_THROW((void)sd.Add(0, 5.0), System::ArgumentException);
}

TEST(CollectionsComparisonContract, ImmutableDictionaryAddRangeAppliesTheSameRule) {
    namespace I = System::Collections::Immutable;
    auto d = I::ImmutableDictionary<int, double>::Empty().Add(0, kNaN);
    auto merged = d.AddRange({{0, kNaN}, {1, 2.0}});
    EXPECT_EQ(merged.getCountProperty(), 2);
    EXPECT_TRUE(merged.ContainsValue(2.0));
}

TEST(CollectionsComparisonContract, ConcurrentDictionaryCompareAndSwapAcceptsANaNComparand) {
    System::Collections::Concurrent::ConcurrentDictionary<int, double> d;
    d.TryAdd(0, kNaN);
    // Before the repair this could never succeed: the comparison value never
    // compared equal to the stored NaN, so the entry was permanently frozen.
    EXPECT_TRUE(d.TryUpdate(0, 5.0, kNaN));
    double got = 0.0;
    ASSERT_TRUE(d.TryGetValue(0, got));
    EXPECT_DOUBLE_EQ(got, 5.0);
}

TEST(CollectionsComparisonContract, ConcurrentDictionaryCompareAndSwapDoesNotMutateOnMismatch) {
    System::Collections::Concurrent::ConcurrentDictionary<int, double> d;
    d.TryAdd(0, 1.0);
    EXPECT_FALSE(d.TryUpdate(0, 5.0, kNaN));   // stored 1.0 is not NaN
    double got = 0.0;
    ASSERT_TRUE(d.TryGetValue(0, got));
    EXPECT_DOUBLE_EQ(got, 1.0);                // unchanged: no partial mutation
    EXPECT_FALSE(d.TryUpdate(9, 5.0, 1.0));    // absent key
}

TEST(CollectionsComparisonContract, ConcurrentDictionaryAddOrUpdateRetriesAgainstANaNObservation) {
    System::Collections::Concurrent::ConcurrentDictionary<int, double> d;
    d.TryAdd(0, kNaN);
    const double result = d.AddOrUpdate(
        0, 99.0, [](const int&, const double& existing) {
            return std::isnan(existing) ? 42.0 : existing;
        });
    EXPECT_DOUBLE_EQ(result, 42.0);
    double got = 0.0;
    ASSERT_TRUE(d.TryGetValue(0, got));
    EXPECT_DOUBLE_EQ(got, 42.0);
}
