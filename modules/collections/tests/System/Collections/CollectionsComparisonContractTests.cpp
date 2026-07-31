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
#include "System/Collections/Generic/ReferenceEqualityComparer.hpp"

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
