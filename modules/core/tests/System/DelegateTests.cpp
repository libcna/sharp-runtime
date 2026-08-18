// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/ArgumentException.hpp"
#include "System/Delegate.hpp"
#include "System/NotImplementedException.hpp"

using System::Delegate;

// ---------------------------------------------------------------------------
// Single-target invoke
// ---------------------------------------------------------------------------

TEST(DelegateTests, SingleTarget_Invoke_CallsLambda) {
    int count = 0;
    auto d = std::make_shared<Delegate>([&]{ ++count; });
    d->Invoke();
    EXPECT_EQ(count, 1);
}

TEST(DelegateTests, SingleTarget_OperatorCall_CallsLambda) {
    int count = 0;
    auto d = std::make_shared<Delegate>([&]{ ++count; });
    (*d)();
    EXPECT_EQ(count, 1);
}

TEST(DelegateTests, Default_Invoke_IsNoOp) {
    auto d = std::make_shared<Delegate>();
    EXPECT_NO_THROW(d->Invoke());
}

// ---------------------------------------------------------------------------
// HasSingleTarget
// ---------------------------------------------------------------------------

TEST(DelegateTests, SingleTarget_HasSingleTarget_True) {
    auto d = std::make_shared<Delegate>([]{});
    EXPECT_TRUE(d->getHasSingleTargetProperty());
}

// ---------------------------------------------------------------------------
// Combine two delegates
// ---------------------------------------------------------------------------

TEST(DelegateTests, Combine_TwoDelegates_BothInvoked) {
    int a = 0, b = 0;
    auto d1 = std::make_shared<Delegate>([&]{ ++a; });
    auto d2 = std::make_shared<Delegate>([&]{ ++b; });
    auto combined = Delegate::Combine(d1, d2);
    ASSERT_TRUE(combined);
    combined->Invoke();
    EXPECT_EQ(a, 1);
    EXPECT_EQ(b, 1);
}

TEST(DelegateTests, Combine_MulticastHasSingleTarget_False) {
    auto d1 = std::make_shared<Delegate>([]{});
    auto d2 = std::make_shared<Delegate>([]{});
    auto combined = Delegate::Combine(d1, d2);
    EXPECT_FALSE(combined->getHasSingleTargetProperty());
}

TEST(DelegateTests, Combine_NullFirst_ReturnsSecond) {
    auto d = std::make_shared<Delegate>([]{});
    auto result = Delegate::Combine(nullptr, d);
    EXPECT_EQ(result.get(), d.get());
}

TEST(DelegateTests, Combine_NullSecond_ReturnsFirst) {
    auto d = std::make_shared<Delegate>([]{});
    auto result = Delegate::Combine(d, nullptr);
    EXPECT_EQ(result.get(), d.get());
}

TEST(DelegateTests, Combine_BothNull_ReturnsNull) {
    auto result = Delegate::Combine(nullptr, nullptr);
    EXPECT_FALSE(result);
}

// ---------------------------------------------------------------------------
// Combine(vector)
// ---------------------------------------------------------------------------

TEST(DelegateTests, CombineVector_ThreeDelegates_AllInvoked) {
    int sum = 0;
    auto d1 = std::make_shared<Delegate>([&]{ sum += 1; });
    auto d2 = std::make_shared<Delegate>([&]{ sum += 2; });
    auto d3 = std::make_shared<Delegate>([&]{ sum += 4; });
    auto combined = Delegate::Combine({d1, d2, d3});
    ASSERT_TRUE(combined);
    combined->Invoke();
    EXPECT_EQ(sum, 7);
}

TEST(DelegateTests, CombineVector_Empty_ReturnsNull) {
    auto result = Delegate::Combine(std::vector<std::shared_ptr<Delegate>>{});
    EXPECT_FALSE(result);
}

// ---------------------------------------------------------------------------
// GetInvocationList
// ---------------------------------------------------------------------------

TEST(DelegateTests, GetInvocationList_SingleTarget_SizeOne) {
    auto d = std::make_shared<Delegate>([]{});
    auto list = d->GetInvocationList();
    ASSERT_EQ(list.size(), 1u);
    EXPECT_EQ(list[0].get(), d.get());
}

TEST(DelegateTests, GetInvocationList_Multicast_CorrectSize) {
    auto d1 = std::make_shared<Delegate>([]{});
    auto d2 = std::make_shared<Delegate>([]{});
    auto d3 = std::make_shared<Delegate>([]{});
    auto combined = Delegate::Combine(Delegate::Combine(d1, d2), d3);
    EXPECT_EQ(combined->GetInvocationList().size(), 3u);
}

// ---------------------------------------------------------------------------
// Remove
// ---------------------------------------------------------------------------

TEST(DelegateTests, Remove_LastOccurrence_Removed) {
    int a = 0, b = 0;
    auto d1 = std::make_shared<Delegate>([&]{ ++a; });
    auto d2 = std::make_shared<Delegate>([&]{ ++b; });
    auto combined = Delegate::Combine(d1, d2);
    auto result = Delegate::Remove(combined, d2);
    ASSERT_TRUE(result);
    result->Invoke();
    EXPECT_EQ(a, 1);
    EXPECT_EQ(b, 0);
}

TEST(DelegateTests, Remove_SingleTarget_ReturnsNull) {
    auto d = std::make_shared<Delegate>([]{});
    auto result = Delegate::Remove(d, d);
    EXPECT_FALSE(result);
}

TEST(DelegateTests, Remove_NotFound_ReturnsSamePointer) {
    auto d1 = std::make_shared<Delegate>([]{});
    auto d2 = std::make_shared<Delegate>([]{});
    auto result = Delegate::Remove(d1, d2);
    EXPECT_EQ(result.get(), d1.get());
}

TEST(DelegateTests, Remove_NullSource_ReturnsNull) {
    auto d = std::make_shared<Delegate>([]{});
    EXPECT_FALSE(Delegate::Remove(nullptr, d));
}

TEST(DelegateTests, Remove_NullValue_ReturnsSource) {
    auto d = std::make_shared<Delegate>([]{});
    auto result = Delegate::Remove(d, nullptr);
    EXPECT_EQ(result.get(), d.get());
}

// ---------------------------------------------------------------------------
// RemoveAll
// ---------------------------------------------------------------------------

TEST(DelegateTests, RemoveAll_AllOccurrencesRemoved) {
    int a = 0, b = 0;
    auto d1 = std::make_shared<Delegate>([&]{ ++a; });
    auto d2 = std::make_shared<Delegate>([&]{ ++b; });
    // d1 appears twice
    auto combined = Delegate::Combine(Delegate::Combine(d1, d2), d1);
    auto result = Delegate::RemoveAll(combined, d1);
    ASSERT_TRUE(result); // d2 should remain
    result->Invoke();
    EXPECT_EQ(a, 0);
    EXPECT_EQ(b, 1);
}

// ---------------------------------------------------------------------------
// Clone
// ---------------------------------------------------------------------------

TEST(DelegateTests, Clone_InvokesBothAfterCombine) {
    int count = 0;
    auto d = std::make_shared<Delegate>([&]{ ++count; });
    auto cloned = d->Clone();
    ASSERT_TRUE(cloned);
    cloned->Invoke();
    EXPECT_EQ(count, 1);
}

// ---------------------------------------------------------------------------
// DynamicInvoke
// ---------------------------------------------------------------------------

TEST(DelegateTests, DynamicInvoke_Throws) {
    auto d = std::make_shared<Delegate>([]{});
    EXPECT_THROW(d->DynamicInvoke({}), System::NotImplementedException);
}

// ---------------------------------------------------------------------------
// InvocationListEnumerator — explicit MoveNext / getCurrent
// ---------------------------------------------------------------------------

TEST(DelegateTests, InvocationListEnumerator_SingleTarget_OneIteration) {
    auto d = std::make_shared<Delegate>([]{});
    auto e = Delegate::EnumerateInvocationList<Delegate>(d);
    EXPECT_TRUE(e.MoveNext());
    EXPECT_EQ(e.getCurrentProperty().get(), d.get());
    EXPECT_FALSE(e.MoveNext());
}

TEST(DelegateTests, InvocationListEnumerator_MulticastThreeTargets_ThreeIterations) {
    auto d1 = std::make_shared<Delegate>([]{});
    auto d2 = std::make_shared<Delegate>([]{});
    auto d3 = std::make_shared<Delegate>([]{});
    auto combined = Delegate::Combine({d1, d2, d3});
    auto e = Delegate::EnumerateInvocationList<Delegate>(combined);
    int count = 0;
    while (e.MoveNext()) ++count;
    EXPECT_EQ(count, 3);
}

TEST(DelegateTests, InvocationListEnumerator_Null_ZeroIterations) {
    auto e = Delegate::EnumerateInvocationList<Delegate>(nullptr);
    EXPECT_FALSE(e.MoveNext());
}

// ---------------------------------------------------------------------------
// InvocationListEnumerator — range-based for loop
// ---------------------------------------------------------------------------

TEST(DelegateTests, InvocationListEnumerator_RangeFor_CollectsAll) {
    int sum = 0;
    auto d1 = std::make_shared<Delegate>([&]{ sum += 1; });
    auto d2 = std::make_shared<Delegate>([&]{ sum += 2; });
    auto combined = Delegate::Combine(d1, d2);
    for (auto target : Delegate::EnumerateInvocationList<Delegate>(combined)) {
        target->Invoke();
    }
    EXPECT_EQ(sum, 3);
}

TEST(DelegateTests, InvocationListEnumerator_RangeFor_SingleTarget_Invokes) {
    int count = 0;
    auto d = std::make_shared<Delegate>([&]{ ++count; });
    for (auto target : Delegate::EnumerateInvocationList<Delegate>(d)) {
        target->Invoke();
    }
    EXPECT_EQ(count, 1);
}

TEST(DelegateTests, InvocationListEnumerator_RangeFor_Null_Empty) {
    int count = 0;
    for (auto target : Delegate::EnumerateInvocationList<Delegate>(nullptr)) {
        (void)target; ++count;
    }
    EXPECT_EQ(count, 0);
}

// ---------------------------------------------------------------------------
// Equality operators
// ---------------------------------------------------------------------------

TEST(DelegateTests, Equality_SameObject_True) {
    auto d = std::make_shared<Delegate>([]{});
    EXPECT_TRUE(*d == *d);
}

TEST(DelegateTests, Equality_DifferentObjects_False) {
    auto d1 = std::make_shared<Delegate>([]{});
    auto d2 = std::make_shared<Delegate>([]{});
    EXPECT_TRUE(*d1 != *d2);
}

// ---------------------------------------------------------------------------
// GetHashCode / Target -- coverage gap found while auditing delegates (ticket 72): both
// were fully implemented/documented but had zero test coverage.
// ---------------------------------------------------------------------------

TEST(DelegateTests, GetHashCode_Default_IsZero) {
    auto d = std::make_shared<Delegate>();
    EXPECT_EQ(d->GetHashCode(), 0u);
}

TEST(DelegateTests, GetHashCode_SingleTarget_Consistent) {
    auto d = std::make_shared<Delegate>([]{});
    EXPECT_EQ(d->GetHashCode(), d->GetHashCode());
}

TEST(DelegateTests, GetHashCode_MulticastIsStable_AndEmptyIsTheDocumentedZero) {
    auto d1 = std::make_shared<Delegate>([]{});
    auto d2 = std::make_shared<Delegate>([]{});
    auto combined = Delegate::Combine(d1, d2);
    // Not asserting a specific value -- only documented as "XOR-folds the pointer hashes of
    // its entries", i.e. an implementation detail -- just that it is stable. The old body also
    // required the fold to be nonzero, which is not a contract: zero is a legal hash code for
    // any state (docs/HashAssertionContractRule.md R6), and an XOR fold can reach it. The one
    // value Delegate::GetHashCode actually documents is the empty delegate's, pinned directly.
    EXPECT_EQ(combined->GetHashCode(), combined->GetHashCode());
    Delegate empty;
    EXPECT_EQ(empty.GetHashCode(), 0u);
}

TEST(DelegateTests, GetTargetProperty_AlwaysNull) {
    auto d = std::make_shared<Delegate>([]{});
    EXPECT_EQ(d->getTargetProperty(), nullptr);
}

// ---------------------------------------------------------------------------
// #2271 / SR-AUD-118 — a composed delegate carries its concrete type
// ---------------------------------------------------------------------------
//
// .NET's CombineImpl (`MulticastDelegate.CoreCLR.cs:212-220`) and Delegate.Remove
// (`Delegate.cs:158-169`) both refuse operands whose runtime types differ, with
// ArgumentException(SR.Arg_DlgtTypeMis) — "Delegates must be of the same type."
//
// THE FINDING'S TWO HALVES ARE INSEPARABLE, and these tests are why. A same-type guard alone
// would break the chained form Combine(Combine(a, b), c): step one returns a multicast Delegate,
// whose own typeid is `Delegate` rather than the operands' type, so step two would compare
// `Delegate` against `C` and reject a combination .NET accepts. The repair reads a multicast's
// type from its ENTRIES — which Combine itself guarantees are uniform — so no data member was
// needed and sizeof(Delegate) is unchanged.

namespace {

class AlphaDelegate : public Delegate {
public:
    using Delegate::Delegate;
};

class BetaDelegate : public Delegate {
public:
    using Delegate::Delegate;
};

}  // namespace

TEST(DelegateTypeIdentityTests, Fix2271_CombiningDifferentConcreteTypesIsRejected) {
    auto alpha = std::make_shared<AlphaDelegate>([] {});
    auto beta  = std::make_shared<BetaDelegate>([] {});

    EXPECT_THROW((void)Delegate::Combine(alpha, beta), System::ArgumentException);
    EXPECT_THROW((void)Delegate::Combine(beta, alpha), System::ArgumentException);

    try {
        (void)Delegate::Combine(alpha, beta);
        ADD_FAILURE() << "expected ArgumentException";
    } catch (const System::ArgumentException& e) {
        // .NET's own sentence, transcribed rather than paraphrased.
        EXPECT_STREQ(e.what(), "Delegates must be of the same type.");
    }
}

TEST(DelegateTypeIdentityTests, Fix2271_TheChainedFormStillWorks) {
    // THE ROW THE FINDING SAID COULD NOT BE SATISFIED BY A GUARD ALONE. Step one returns a
    // multicast whose own typeid is `Delegate`; if that were the type compared in step two, this
    // would throw.
    int calls = 0;
    auto a = std::make_shared<AlphaDelegate>([&] { ++calls; });
    auto b = std::make_shared<AlphaDelegate>([&] { ++calls; });
    auto c = std::make_shared<AlphaDelegate>([&] { ++calls; });

    std::shared_ptr<Delegate> ab;
    ASSERT_NO_THROW(ab = Delegate::Combine(a, b));
    std::shared_ptr<Delegate> abc;
    ASSERT_NO_THROW(abc = Delegate::Combine(ab, c));
    ASSERT_NE(nullptr, abc);
    abc->Invoke();
    EXPECT_EQ(3, calls);

    // ...and the composed delegate really does carry the type, rather than merely tolerating the
    // second step: adding a foreign operand to it is still refused.
    auto foreign = std::make_shared<BetaDelegate>([] {});
    EXPECT_THROW((void)Delegate::Combine(abc, foreign), System::ArgumentException);
    EXPECT_THROW((void)Delegate::Combine(foreign, abc), System::ArgumentException);
}

TEST(DelegateTypeIdentityTests, Fix2271_RemoveAndRemoveAllRefuseAForeignType) {
    // `Delegate.cs:166-167` applies the SAME check to Remove, and RemoveAll inherits it because
    // it is defined in terms of Remove — in .NET and here alike.
    auto a = std::make_shared<AlphaDelegate>([] {});
    auto b = std::make_shared<AlphaDelegate>([] {});
    auto combined = Delegate::Combine(a, b);
    auto foreign = std::make_shared<BetaDelegate>([] {});

    EXPECT_THROW((void)Delegate::Remove(combined, foreign), System::ArgumentException);
    EXPECT_THROW((void)Delegate::RemoveAll(combined, foreign), System::ArgumentException);

    // The same-type removal is untouched.
    auto afterRemove = Delegate::Remove(combined, b);
    ASSERT_NE(nullptr, afterRemove);
    EXPECT_TRUE(afterRemove->getHasSingleTargetProperty());
}

TEST(DelegateTypeIdentityTests, Fix2271_TheNullOrderingIsDotNetsAndTheBaseTypeIsUnaffected) {
    // .NET checks the types INSIDE CombineImpl, which a null `a` never reaches, so
    // Combine(nullptr, b) returns b unchecked. Remove's check likewise runs after both null
    // tests. The ordering is deliberate and asserted so it cannot drift.
    auto beta = std::make_shared<BetaDelegate>([] {});
    EXPECT_EQ(beta, Delegate::Combine(nullptr, beta));
    EXPECT_EQ(beta, Delegate::Combine(beta, nullptr));
    EXPECT_EQ(nullptr, Delegate::Remove(nullptr, beta));
    EXPECT_EQ(beta, Delegate::Remove(beta, nullptr));

    // Plain Delegate instances all share one type, so every pre-existing combination keeps
    // working. That is the whole of the narrowing: it bites only across two DIFFERENT derived
    // types, which is exactly .NET's rule.
    int calls = 0;
    auto p = std::make_shared<Delegate>([&] { ++calls; });
    auto q = std::make_shared<Delegate>([&] { ++calls; });
    auto pq = Delegate::Combine(p, q);
    ASSERT_NE(nullptr, pq);
    pq->Invoke();
    EXPECT_EQ(2, calls);
}
