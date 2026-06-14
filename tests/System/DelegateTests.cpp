// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
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
