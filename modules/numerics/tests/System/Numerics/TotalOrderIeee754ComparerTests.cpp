// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <limits>
#include <bit>
#include <cstdint>
#include <type_traits>
#include <utility>

#include "System/Collections/Generic/IComparer.hpp"
#include "System/Collections/Generic/IEqualityComparer.hpp"
#include "System/Double.hpp"
#include "System/Half.hpp"
#include "System/Numerics/DivisionRounding.hpp"
#include "System/Numerics/TotalOrderIeee754Comparer.hpp"

using System::Half;
using System::Numerics::DivisionRounding;
using System::Numerics::TotalOrderIeee754Comparer;

TEST(TotalOrderIeee754ComparerTests, Float_NegativeLessThanPositive) {
    TotalOrderIeee754Comparer<float> cmp;
    EXPECT_LT(cmp.Compare(-1.0f, 1.0f), 0);
    EXPECT_GT(cmp.Compare(1.0f, -1.0f), 0);
    EXPECT_EQ(cmp.Compare(1.0f, 1.0f), 0);
}

TEST(TotalOrderIeee754ComparerTests, Float_NegativeZeroLessThanPositiveZero) {
    TotalOrderIeee754Comparer<float> cmp;
    EXPECT_LT(cmp.Compare(-0.0f, 0.0f), 0);
    EXPECT_GT(cmp.Compare(0.0f, -0.0f), 0);
}

TEST(TotalOrderIeee754ComparerTests, Float_PositiveInfinityLessThanNaN) {
    TotalOrderIeee754Comparer<float> cmp;
    float posInf = std::numeric_limits<float>::infinity();
    float nan = std::numeric_limits<float>::quiet_NaN();
    EXPECT_LT(cmp.Compare(posInf, nan), 0);
}

TEST(TotalOrderIeee754ComparerTests, Float_NegativeInfinityIsSmallest) {
    TotalOrderIeee754Comparer<float> cmp;
    float negInf = -std::numeric_limits<float>::infinity();
    EXPECT_LT(cmp.Compare(negInf, -1.0f), 0);
    EXPECT_LT(cmp.Compare(negInf, 0.0f), 0);
}

TEST(TotalOrderIeee754ComparerTests, Double_OrdersLikeFloat) {
    TotalOrderIeee754Comparer<double> cmp;
    EXPECT_LT(cmp.Compare(-2.0, -1.0), 0);
    EXPECT_LT(cmp.Compare(-0.0, 0.0), 0);
    EXPECT_EQ(cmp.Compare(3.5, 3.5), 0);
}

TEST(TotalOrderIeee754ComparerTests, Half_OrdersNegativeBeforePositive) {
    TotalOrderIeee754Comparer<Half> cmp;
    Half negOne = Half::FromSingle(-1.0f);
    Half posOne = Half::FromSingle(1.0f);
    EXPECT_LT(cmp.Compare(negOne, posOne), 0);
    EXPECT_EQ(cmp.Compare(posOne, posOne), 0);
}

TEST(DivisionRoundingTests, HasFiveValuesMatchingDotNet) {
    EXPECT_EQ(static_cast<int>(DivisionRounding::Truncate), 0);
    EXPECT_EQ(static_cast<int>(DivisionRounding::Floor), 1);
    EXPECT_EQ(static_cast<int>(DivisionRounding::Ceiling), 2);
    EXPECT_EQ(static_cast<int>(DivisionRounding::AwayFromZero), 3);
    EXPECT_EQ(static_cast<int>(DivisionRounding::Euclidean), 4);
}

// ---------------------------------------------------------------------------
// Ticket #2169 (SR-AUD-042, layout-neutral subpart): the equality half of the
// .NET comparer contract. The polymorphic-binding half is #2170 and is gated on
// an approval for an object-layout change; TotalOrder_LayoutIsUnchanged below is
// what makes that gate visible if anyone ever removes it silently.
// ---------------------------------------------------------------------------

#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include "System/Collections/Generic/IEqualityComparer.hpp"

namespace {

// A float with a NaN payload distinct from the default quiet NaN.
float NaNWithPayload(uint32_t payload) {
    uint32_t bits = std::bit_cast<uint32_t>(std::numeric_limits<float>::quiet_NaN()) | payload;
    return std::bit_cast<float>(bits);
}

double NaNWithPayloadD(uint64_t payload) {
    uint64_t bits = std::bit_cast<uint64_t>(std::numeric_limits<double>::quiet_NaN()) | payload;
    return std::bit_cast<double>(bits);
}

} // namespace

TEST(TotalOrderIeee754ComparerTests, Fix2170_TheSecondBaseCostsExactlyOneVptr) {
    // INVERTED BY #2170. The predecessor asserted 8 and said "if this assertion ever fails, an
    // approval-gated change landed without its approval" -- the approval was granted per action on
    // 2026-08-19, so the pin now records what was BOUGHT rather than what was withheld.
    static_assert(sizeof(TotalOrderIeee754Comparer<float>) == 16);
    static_assert(sizeof(TotalOrderIeee754Comparer<double>) == 16);
    static_assert(sizeof(TotalOrderIeee754Comparer<Half>) == 16);
    static_assert(alignof(TotalOrderIeee754Comparer<float>) == 8);

    // Asserted as a RELATIONSHIP, not as a literal: the growth is one pointer, so a future
    // platform with a different pointer width keeps this pin meaningful.
    static_assert(sizeof(TotalOrderIeee754Comparer<float>)
                  == 8 + sizeof(void*));

    EXPECT_TRUE((std::is_base_of_v<System::Collections::Generic::IEqualityComparer<float>,
                                   TotalOrderIeee754Comparer<float>>));
    EXPECT_TRUE((std::is_base_of_v<System::Collections::Generic::IComparer<float>,
                                   TotalOrderIeee754Comparer<float>>));

    // The IEqualityComparer subobject sits at offset 8, after IComparer's. Measured, and pinned
    // because it is the half a bare sizeof cannot express: a single base that merely grew would
    // give the same 16.
    TotalOrderIeee754Comparer<float> cmp;
    const auto* asEquality =
        static_cast<const System::Collections::Generic::IEqualityComparer<float>*>(&cmp);
    const auto* asCompare =
        static_cast<const System::Collections::Generic::IComparer<float>*>(&cmp);
    EXPECT_EQ(reinterpret_cast<const char*>(asCompare) - reinterpret_cast<const char*>(&cmp), 0);
    EXPECT_EQ(reinterpret_cast<const char*>(asEquality) - reinterpret_cast<const char*>(&cmp), 8);
}

TEST(TotalOrderIeee754ComparerTests, Fix2170_BindsPolymorphicallyAsIEqualityComparer) {
    // THIS IS WHAT #2170 BUYS, and it is the one thing #2169 explicitly could not deliver: the
    // comparer can be passed where the INTERFACE is required. Before the second base this function
    // could not be called with it at all -- the failure was a compile error, not a wrong answer.
    const auto throughInterface =
        [](const System::Collections::Generic::IEqualityComparer<double>& eq,
           double a, double b) {
            return std::pair<bool, SharpRuntime::intcs>{eq.Equals(a, b), eq.GetHashCode(a)};
        };

    TotalOrderIeee754Comparer<double> cmp;
    // Total order distinguishes the signed zeros, and the interface call must see THAT semantic
    // rather than ordinary floating equality -- i.e. the override really is dispatched.
    const auto zeros = throughInterface(cmp, -0.0, 0.0);
    EXPECT_FALSE(zeros.first);
    EXPECT_TRUE(cmp.Equals(3.5, 3.5));
    EXPECT_TRUE(throughInterface(cmp, 3.5, 3.5).first);

    // Dispatched, not sliced: the interface call and the direct call agree on every answer.
    EXPECT_EQ(throughInterface(cmp, 1.5, 1.5).second, cmp.GetHashCode(1.5));

    // And the other base still works from the same object.
    const System::Collections::Generic::IComparer<double>& asCompare = cmp;
    EXPECT_LT(asCompare.Compare(-0.0, 0.0), 0);
}

TEST(TotalOrderIeee754ComparerTests, Decl2392_TheBitPatternHashIsADecisionNotAnOmission) {
    // #2392, decided 2026-08-19. .NET's GetHashCode is obj.GetHashCode() -- the VALUE's hash --
    // and Double.GetHashCode normalizes so that "all NaNs and both zeros have the same hash code".
    // This port hashes the BIT PATTERN instead. Adopting .NET's coarser hash was offered and
    // declined.
    //
    // The four pins below are stated as a DIFFERENCE FROM .NET rather than only as a property of
    // this port, so a future reader sees the gap rather than inferring parity.
    TotalOrderIeee754Comparer<double> cmp;
    EXPECT_NE(cmp.GetHashCode(-0.0), cmp.GetHashCode(0.0))
        << "#2392: .NET returns EQUAL here; this port distinguishes the signed zeros";

    const double nanA = std::bit_cast<double>(
        std::bit_cast<uint64_t>(std::numeric_limits<double>::quiet_NaN()) | 0x1ull);
    const double nanB = std::bit_cast<double>(
        std::bit_cast<uint64_t>(std::numeric_limits<double>::quiet_NaN()) | 0x2ull);
    EXPECT_NE(cmp.GetHashCode(nanA), cmp.GetHashCode(nanB))
        << "#2392: .NET returns EQUAL here too; this port distinguishes NaN payloads";

    // THE HALF THAT MAKES THE DECISION AVAILABLE AT ALL: the contract still holds. Equality here
    // IS bit-pattern identity, so equal values can never hash differently -- which is why BOTH
    // hashes are legal and the choice is about distribution, not correctness.
    EXPECT_TRUE(cmp.Equals(nanA, nanA));
    EXPECT_EQ(cmp.GetHashCode(nanA), cmp.GetHashCode(nanA));
    EXPECT_FALSE(cmp.Equals(-0.0, 0.0)) << "unequal, so their hashes are permitted to differ";

    // And .NET's own source of that coarseness is present and correct in this port -- it is the
    // COMPARER that diverges, not Double::GetHashCode. Pinned so a future reader does not
    // "fix" the wrong one.
    EXPECT_EQ(System::Double::GetHashCode(-0.0), System::Double::GetHashCode(0.0))
        << "#2392: Double::GetHashCode already matches .NET, normalization included";
    EXPECT_EQ(System::Double::GetHashCode(nanA), System::Double::GetHashCode(nanB));
}

TEST(TotalOrderIeee754ComparerTests, Decl2170_IEquatableIsNotReproduced) {
    // .NET also implements IEquatable<TotalOrderIeee754Comparer<T>> with `=> true`
    // (TotalOrderIeee754Comparer.cs:204). Deliberately NOT reproduced: a third base would cost a
    // third vptr to express what a stateless empty type already means. Pinned so its absence is a
    // decision rather than an oversight, and so a future addition is a deliberate act.
    static_assert(sizeof(TotalOrderIeee754Comparer<float>) == 16,
                  "a third base would make this 24");
    SUCCEED();
}

TEST(TotalOrderIeee754ComparerTests, EqualsSignatureMatchesIEqualityComparer) {
    // Written so that #2170 stays a two-line change: if these signatures ever drift, adding the
    // base would silently create an overload instead of an override.
    using CF = TotalOrderIeee754Comparer<float>;
    static_assert(std::is_same_v<decltype(&CF::Equals),
                                 bool (CF::*)(const float&, const float&) const>);
    static_assert(std::is_same_v<decltype(&CF::GetHashCode),
                                 SharpRuntime::intcs (CF::*)(const float&) const>);
    SUCCEED();
}

TEST(TotalOrderIeee754ComparerTests, Float_EqualsAgreesWithCompareOnEveryVector) {
    TotalOrderIeee754Comparer<float> cmp;
    const float inf = std::numeric_limits<float>::infinity();
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float vectors[] = {
        -inf, -std::numeric_limits<float>::max(), -1.0f,
        -std::numeric_limits<float>::min(), -std::numeric_limits<float>::denorm_min(),
        -0.0f, 0.0f,
        std::numeric_limits<float>::denorm_min(), std::numeric_limits<float>::min(),
        1.0f, 3.5f, std::numeric_limits<float>::max(), inf, nan, -nan,
    };
    for (float a : vectors)
        for (float b : vectors)
            EXPECT_EQ(cmp.Equals(a, b), cmp.Compare(a, b) == 0)
                << "a bits=" << std::bit_cast<uint32_t>(a) << " b bits=" << std::bit_cast<uint32_t>(b);
}

// The hash inequalities in this suite are retained deliberately. They are not the collision
// assumption SR-AUD-018 outlaws: TotalOrderIeee754Comparer's header states, as behaviour, that
// "-0 and +0 hash differently, and so do distinct NaN payloads" and that the binary64 hash folds
// the wide pattern rather than truncating it. For float and Half the hash IS the bit pattern
// widened, so the property is injective by construction, not lucky
// (docs/HashAssertionContractRule.md R3, class B).
TEST(TotalOrderIeee754ComparerTests, Float_SignedZerosAreDistinct) {
    TotalOrderIeee754Comparer<float> cmp;
    EXPECT_FALSE(cmp.Equals(-0.0f, 0.0f));
    EXPECT_TRUE(cmp.Equals(0.0f, 0.0f));
    EXPECT_TRUE(cmp.Equals(-0.0f, -0.0f));
    // The point of the whole exercise: ordinary floating equality cannot see this.
    EXPECT_TRUE(-0.0f == 0.0f);
    EXPECT_NE(cmp.GetHashCode(-0.0f), cmp.GetHashCode(0.0f));
}

TEST(TotalOrderIeee754ComparerTests, Float_NaNPayloadsAreDistinguished) {
    TotalOrderIeee754Comparer<float> cmp;
    const float nanA = NaNWithPayload(0u);
    const float nanB = NaNWithPayload(0x123u);
    ASSERT_TRUE(std::isnan(nanA));
    ASSERT_TRUE(std::isnan(nanB));
    EXPECT_TRUE(cmp.Equals(nanA, nanA));
    EXPECT_FALSE(cmp.Equals(nanA, nanB));
    // Ordinary equality says neither is equal to anything, including itself.
    EXPECT_FALSE(nanA == nanA);
    EXPECT_EQ(cmp.GetHashCode(nanA), cmp.GetHashCode(nanA));
    EXPECT_NE(cmp.GetHashCode(nanA), cmp.GetHashCode(nanB));
    // A negative NaN is a different value again.
    EXPECT_FALSE(cmp.Equals(nanA, -nanA));
}

TEST(TotalOrderIeee754ComparerTests, Float_HashAgreesWithEquals) {
    TotalOrderIeee754Comparer<float> cmp;
    const float vectors[] = {
        -std::numeric_limits<float>::infinity(), -1.0f, -0.0f, 0.0f,
        std::numeric_limits<float>::denorm_min(), std::numeric_limits<float>::min(),
        1.0f, std::numeric_limits<float>::max(), std::numeric_limits<float>::infinity(),
        NaNWithPayload(0u), NaNWithPayload(0x321u),
    };
    for (float a : vectors) {
        for (float b : vectors) {
            if (cmp.Equals(a, b)) { EXPECT_EQ(cmp.GetHashCode(a), cmp.GetHashCode(b)); }
        }
    }
}

TEST(TotalOrderIeee754ComparerTests, Double_EqualityAndHash) {
    TotalOrderIeee754Comparer<double> cmp;
    const double inf = std::numeric_limits<double>::infinity();
    const double vectors[] = {
        -inf, -std::numeric_limits<double>::max(), -1.0,
        -std::numeric_limits<double>::min(), -std::numeric_limits<double>::denorm_min(),
        -0.0, 0.0, std::numeric_limits<double>::denorm_min(),
        std::numeric_limits<double>::min(), 1.0, 3.5,
        std::numeric_limits<double>::max(), inf,
        NaNWithPayloadD(0u), NaNWithPayloadD(0x4321u),
    };
    for (double a : vectors) {
        for (double b : vectors) {
            EXPECT_EQ(cmp.Equals(a, b), cmp.Compare(a, b) == 0);
            if (cmp.Equals(a, b)) { EXPECT_EQ(cmp.GetHashCode(a), cmp.GetHashCode(b)); }
        }
    }
    EXPECT_FALSE(cmp.Equals(-0.0, 0.0));
    EXPECT_NE(cmp.GetHashCode(-0.0), cmp.GetHashCode(0.0));
}

TEST(TotalOrderIeee754ComparerTests, Double_HashFoldsBothHalvesRatherThanTruncating) {
    // Two binary64 values that differ ONLY above bit 32 must not collide; a plain truncation to
    // intcs would give them the same hash.
    TotalOrderIeee754Comparer<double> cmp;
    const double a = std::bit_cast<double>(uint64_t{0x0000000000000001ull});
    const double b = std::bit_cast<double>(uint64_t{0x0000000100000001ull});
    EXPECT_FALSE(cmp.Equals(a, b));
    EXPECT_NE(cmp.GetHashCode(a), cmp.GetHashCode(b));
}

TEST(TotalOrderIeee754ComparerTests, Half_EqualityAndHash) {
    TotalOrderIeee754Comparer<Half> cmp;
    const Half posZero = Half::FromSingle(0.0f);
    const Half negZero = Half::FromSingle(-0.0f);
    const Half one     = Half::FromSingle(1.0f);
    const Half negOne  = Half::FromSingle(-1.0f);
    const Half inf     = Half::FromSingle(std::numeric_limits<float>::infinity());
    const Half nan     = Half::FromSingle(std::numeric_limits<float>::quiet_NaN());
    const Half vectors[] = {negOne, negZero, posZero, one, inf, nan};
    for (const Half& a : vectors) {
        for (const Half& b : vectors) {
            EXPECT_EQ(cmp.Equals(a, b), cmp.Compare(a, b) == 0);
            if (cmp.Equals(a, b)) { EXPECT_EQ(cmp.GetHashCode(a), cmp.GetHashCode(b)); }
        }
    }
    EXPECT_FALSE(cmp.Equals(negZero, posZero));
    EXPECT_NE(cmp.GetHashCode(negZero), cmp.GetHashCode(posZero));
    EXPECT_TRUE(cmp.Equals(nan, nan));
}

TEST(TotalOrderIeee754ComparerTests, OrderingResultsAreUnchangedByTheEqualitySubpart) {
    // Invariance: #2169 adds members, it does not touch Compare.
    TotalOrderIeee754Comparer<float> cmp;
    EXPECT_LT(cmp.Compare(-1.0f, 1.0f), 0);
    EXPECT_LT(cmp.Compare(-0.0f, 0.0f), 0);
    EXPECT_LT(cmp.Compare(std::numeric_limits<float>::infinity(),
                          std::numeric_limits<float>::quiet_NaN()), 0);
    EXPECT_LT(cmp.Compare(-std::numeric_limits<float>::quiet_NaN(),
                          -std::numeric_limits<float>::infinity()), 0);
    EXPECT_EQ(cmp.Compare(3.5f, 3.5f), 0);
}
