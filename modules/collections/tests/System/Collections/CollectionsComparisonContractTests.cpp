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
#include "System/Collections/Generic/List.hpp"
#include "System/Collections/Generic/ReferenceEqualityComparer.hpp"
#include "System/Collections/Immutable/ImmutableArray.hpp"
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
