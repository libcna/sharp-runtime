// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Regressions for SR-AUD-119 (ticket #2272): Delegate::Equals compared multicast
// invocation-list entries by shared_ptr address instead of asking the entries
// themselves, so two independently built lists over the same methods compared
// unequal, and GetHashCode folded the same addresses.
//
// The suite covers both directions: the widening the finding requires, and the
// answers that must NOT move. See docs/CoreDelegateCompositionContractPlan.md 5.

#include <gtest/gtest.h>

#include <memory>

#include "System/Delegate.hpp"
#include "System/MulticastDelegate.hpp"

using System::Delegate;
using System::MulticastDelegate;

namespace {

    int firstCalls  = 0;
    int secondCalls = 0;
    int thirdCalls  = 0;

    void First()  { ++firstCalls; }
    void Second() { ++secondCalls; }
    void Third()  { ++thirdCalls; }

    std::shared_ptr<Delegate> Pair(void (*a)(), void (*b)()) {
        return Delegate::Combine(std::make_shared<Delegate>(a),
                                 std::make_shared<Delegate>(b));
    }

} // namespace

// ---------------------------------------------------------------------------
// The finding: independently allocated entries over the same methods
// ---------------------------------------------------------------------------

TEST(DelegateInvocationListEqualityTests, IndependentListsOverSameFunctions_AreEqual) {
    auto l1 = Pair(First, Second);
    auto l2 = Pair(First, Second);
    ASSERT_TRUE(l1 && l2);
    // Not the same objects, and not built from the same entries.
    EXPECT_NE(l1.get(), l2.get());
    EXPECT_NE(l1->GetInvocationList()[0].get(), l2->GetInvocationList()[0].get());
    EXPECT_TRUE(l1->Equals(*l2));
    EXPECT_TRUE(*l1 == *l2);
    EXPECT_FALSE(*l1 != *l2);
}

TEST(DelegateInvocationListEqualityTests, IndependentListsOverSameFunctions_HashEqual) {
    auto l1 = Pair(First, Second);
    auto l2 = Pair(First, Second);
    ASSERT_TRUE(l1 && l2);
    EXPECT_EQ(l1->GetHashCode(), l2->GetHashCode());
}

TEST(DelegateInvocationListEqualityTests, ThreeEntryIndependentLists_AreEqual) {
    auto l1 = Delegate::Combine({std::make_shared<Delegate>(First),
                                 std::make_shared<Delegate>(Second),
                                 std::make_shared<Delegate>(Third)});
    auto l2 = Delegate::Combine({std::make_shared<Delegate>(First),
                                 std::make_shared<Delegate>(Second),
                                 std::make_shared<Delegate>(Third)});
    ASSERT_TRUE(l1 && l2);
    EXPECT_TRUE(l1->Equals(*l2));
    EXPECT_EQ(l1->GetHashCode(), l2->GetHashCode());
}

// The entries already answered Equals correctly before the repair -- the list
// loop simply never asked them. Pinning that keeps the diagnosis honest.
TEST(DelegateInvocationListEqualityTests, EntriesThemselvesWereAlreadyEqual) {
    auto a1 = std::make_shared<Delegate>(First);
    auto a2 = std::make_shared<Delegate>(First);
    EXPECT_NE(a1.get(), a2.get());
    EXPECT_TRUE(a1->Equals(*a2));
}

// ---------------------------------------------------------------------------
// Answers that must not move
// ---------------------------------------------------------------------------

TEST(DelegateInvocationListEqualityTests, ListsOverDifferentFunctions_NotEqual) {
    auto l1 = Pair(First, Second);
    auto l2 = Pair(First, Third);
    ASSERT_TRUE(l1 && l2);
    EXPECT_FALSE(l1->Equals(*l2));
}

TEST(DelegateInvocationListEqualityTests, ReorderedList_NotEqual) {
    auto l1 = Pair(First, Second);
    auto l2 = Pair(Second, First);
    ASSERT_TRUE(l1 && l2);
    EXPECT_FALSE(l1->Equals(*l2));
}

TEST(DelegateInvocationListEqualityTests, DifferentLengths_NotEqual) {
    auto l1 = Pair(First, Second);
    auto l2 = Delegate::Combine({std::make_shared<Delegate>(First),
                                 std::make_shared<Delegate>(Second),
                                 std::make_shared<Delegate>(Third)});
    ASSERT_TRUE(l1 && l2);
    EXPECT_FALSE(l1->Equals(*l2));
    EXPECT_FALSE(l2->Equals(*l1));
}

TEST(DelegateInvocationListEqualityTests, SingleTargetVersusMulticast_NotEqual) {
    auto single = std::make_shared<Delegate>(First);
    auto list   = Pair(First, First);
    ASSERT_TRUE(list);
    EXPECT_FALSE(single->Equals(*list));
    EXPECT_FALSE(list->Equals(*single));
}

// A closure has no comparable target in C++, so lists of them stay
// identity-only. This is the documented C++-vs-C# delegate limitation, and the
// repair must not pretend otherwise.
TEST(DelegateInvocationListEqualityTests, LambdaEntryLists_StayIdentityOnly) {
    auto l1 = Delegate::Combine(std::make_shared<Delegate>([]{}),
                                std::make_shared<Delegate>([]{}));
    auto l2 = Delegate::Combine(std::make_shared<Delegate>([]{}),
                                std::make_shared<Delegate>([]{}));
    ASSERT_TRUE(l1 && l2);
    EXPECT_FALSE(l1->Equals(*l2));
}

TEST(DelegateInvocationListEqualityTests, SameEntryPointers_StayEqual) {
    auto t1 = std::make_shared<Delegate>([]{});
    auto t2 = std::make_shared<Delegate>([]{});
    auto l1 = Delegate::Combine(t1, t2);
    auto l2 = Delegate::Combine(t1, t2);
    ASSERT_TRUE(l1 && l2);
    EXPECT_TRUE(l1->Equals(*l2));
    EXPECT_EQ(l1->GetHashCode(), l2->GetHashCode());
}

// A list whose entries have no comparable target folds exactly the addresses the
// pre-repair code folded, so its hash is unchanged by this ticket.
TEST(DelegateInvocationListEqualityTests, LambdaEntryList_HashIsStableAndNonZero) {
    auto l = Delegate::Combine(std::make_shared<Delegate>([]{}),
                               std::make_shared<Delegate>([]{}));
    ASSERT_TRUE(l);
    EXPECT_EQ(l->GetHashCode(), l->GetHashCode());
    EXPECT_NE(l->GetHashCode(), 0u);
}

// ---------------------------------------------------------------------------
// Removal reads the same equality, so it follows the widening
// ---------------------------------------------------------------------------

TEST(DelegateInvocationListEqualityTests, Remove_EqualButDistinctEntry_IsRemoved) {
    auto source = Pair(First, Second);
    ASSERT_TRUE(source);
    auto result = Delegate::Remove(source, std::make_shared<Delegate>(Second));
    ASSERT_TRUE(result);
    firstCalls = secondCalls = 0;
    result->Invoke();
    EXPECT_EQ(firstCalls, 1);
    EXPECT_EQ(secondCalls, 0);
}

// ---------------------------------------------------------------------------
// The subclass sees the same contract
// ---------------------------------------------------------------------------

TEST(DelegateInvocationListEqualityTests, MulticastDelegateEntries_IndependentListsEqual) {
    auto l1 = Delegate::Combine(std::make_shared<MulticastDelegate>(First),
                                std::make_shared<MulticastDelegate>(Second));
    auto l2 = Delegate::Combine(std::make_shared<MulticastDelegate>(First),
                                std::make_shared<MulticastDelegate>(Second));
    ASSERT_TRUE(l1 && l2);
    EXPECT_TRUE(l1->Equals(*l2));
    EXPECT_EQ(l1->GetHashCode(), l2->GetHashCode());
}
