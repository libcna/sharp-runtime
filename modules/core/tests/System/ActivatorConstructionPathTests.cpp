// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Ticket #2266 (finding SR-AUD-109). Activator::CreateInstance returned
// T{std::forward<Args>(args)...}, and braces prefer an initializer_list
// constructor over every other candidate, so CreateInstance<std::vector<int>>(3, 7)
// built the two-element sequence {3, 7} instead of the three sevens the caller
// asked for, while the otherwise equivalent CreateInstancePtr reached the
// intended constructor through make_unique.
//
// These tests pin BOTH halves of the repaired contract, because the audited
// repair -- switching unconditionally to parentheses -- would have fixed the
// first half by breaking the second:
//
//   * where the type has a constructor to forward to, the arguments reach it;
//   * where it does not -- an aggregate, or a type reachable only through an
//     initializer_list constructor -- the braced form is retained, since
//     parentheses can express neither brace elision nor an initializer_list.
//
// docs/CoreActivatorConstructionPathPlan.md
#include <gtest/gtest.h>

#include "System/Activator.hpp"

#include <array>
#include <initializer_list>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

struct Aggregate {
    int x;
    int y;
};

struct NestedAggregate {
    int a;
    Aggregate inner;
};

// The classic ambiguity: both a two-argument constructor and an
// initializer_list constructor are viable for (1, 2), and only the spelling
// decides which one runs.
struct BothConstructors {
    int tag;
    BothConstructors(int, int) : tag(1) {}
    BothConstructors(std::initializer_list<int>) : tag(2) {}
};

struct InitializerListOnly {
    int count;
    InitializerListOnly(std::initializer_list<int> values)
        : count(static_cast<int>(values.size())) {}
};

struct TwoArgConstructor {
    int value;
    TwoArgConstructor(int a, int b) : value(a * 100 + b) {}
};

struct ExplicitConstructor {
    int value;
    explicit ExplicitConstructor(int a) : value(a) {}
};

struct MoveOnlyArgument {
    std::unique_ptr<int> owned;
    explicit MoveOnlyArgument(std::unique_ptr<int> p) : owned(std::move(p)) {}
};

// Records which reference overload the argument actually bound to, so a change
// to the construction path cannot silently cost perfect forwarding.
struct ValueCategoryRecorder {
    const char* how;
    ValueCategoryRecorder(int&) : how("lvalue") {}
    ValueCategoryRecorder(const int&) : how("const lvalue") {}
    ValueCategoryRecorder(int&&) : how("rvalue") {}
};

struct ThrowingConstructor {
    explicit ThrowingConstructor(int) { throw std::string("from ctor"); }
};

} // namespace

// ===========================================================================
// The finding's own reproducer, and the pointer form it disagreed with
// ===========================================================================

TEST(ActivatorConstructionPathTests, VectorCountValueReachesTheConstructor) {
    const auto v = System::Activator::CreateInstance<std::vector<int>>(3, 7);
    ASSERT_EQ(v.size(), 3u);
    EXPECT_EQ(v[0], 7);
    EXPECT_EQ(v[1], 7);
    EXPECT_EQ(v[2], 7);
}

TEST(ActivatorConstructionPathTests, ValueAndPointerFormsNowAgree) {
    const auto value = System::Activator::CreateInstance<std::vector<int>>(3, 7);
    const auto pointer = System::Activator::CreateInstancePtr<std::vector<int>>(3, 7);
    ASSERT_NE(pointer.get(), nullptr);
    EXPECT_EQ(value, *pointer);

    // The second shape the two forms used to disagree about: braces made this
    // {1, 2}, the constructor makes it a single element with value 2.
    const auto value2 = System::Activator::CreateInstance<std::vector<int>>(1, 2);
    const auto pointer2 = System::Activator::CreateInstancePtr<std::vector<int>>(1, 2);
    ASSERT_NE(pointer2.get(), nullptr);
    EXPECT_EQ(value2, *pointer2);
    ASSERT_EQ(value2.size(), 1u);
    EXPECT_EQ(value2[0], 2);
}

TEST(ActivatorConstructionPathTests, TwoArgConstructorWinsOverInitializerList) {
    const auto both = System::Activator::CreateInstance<BothConstructors>(1, 2);
    EXPECT_EQ(both.tag, 1) << "expected the (int, int) constructor, not initializer_list";

    const auto pointer = System::Activator::CreateInstancePtr<BothConstructors>(1, 2);
    ASSERT_NE(pointer.get(), nullptr);
    EXPECT_EQ(both.tag, pointer->tag);
}

// ===========================================================================
// The half the audited repair would have broken — these categories have no
// constructor to forward to and must keep braced initialization
// ===========================================================================

TEST(ActivatorConstructionPathTests, StdArrayAggregateStillBuildsWithBraceElision) {
    const auto a = System::Activator::CreateInstance<std::array<int, 3>>(1, 2, 3);
    EXPECT_EQ(a[0], 1);
    EXPECT_EQ(a[1], 2);
    EXPECT_EQ(a[2], 3);
}

TEST(ActivatorConstructionPathTests, NestedAggregateStillUsesBraceElision) {
    const auto n = System::Activator::CreateInstance<NestedAggregate>(1, 2, 3);
    EXPECT_EQ(n.a, 1);
    EXPECT_EQ(n.inner.x, 2);
    EXPECT_EQ(n.inner.y, 3);
}

TEST(ActivatorConstructionPathTests, PlainAggregateStillInitializesMemberwise) {
    const auto g = System::Activator::CreateInstance<Aggregate>(1, 2);
    EXPECT_EQ(g.x, 1);
    EXPECT_EQ(g.y, 2);
}

TEST(ActivatorConstructionPathTests, InitializerListElementsAreStillReachable) {
    const auto v = System::Activator::CreateInstance<std::vector<int>>(1, 2, 3);
    ASSERT_EQ(v.size(), 3u);
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[2], 3);

    const auto only = System::Activator::CreateInstance<InitializerListOnly>(1, 2, 3);
    EXPECT_EQ(only.count, 3);
}

// ===========================================================================
// Selection predicate — the rule itself, stated directly
// ===========================================================================
//
// A SFINAE probe on CreateInstance would prove nothing here: both templates are
// unconstrained, so an ill-formed body is a hard error rather than a
// substitution failure and every call looks "well-formed" to a detection idiom.
// The compile DOMAIN is therefore pinned two ways instead: every preserved shape
// above is really instantiated, so this file stops compiling if one is lost, and
// the two shapes that must stay REJECTED are pinned per-site by
// test/consumer/core_activator_construction_negative.cpp, which is the only
// mechanism that can attribute a compile rejection to its own source line.

TEST(ActivatorConstructionPathTests, SelectionPredicateRoutesEachCategory) {
    using System::detail::activatorForwardsToConstructor;
    // Forward to a constructor: non-aggregate class types that have one.
    static_assert(activatorForwardsToConstructor<std::vector<int>, int, int>);
    static_assert(activatorForwardsToConstructor<BothConstructors, int, int>);
    static_assert(activatorForwardsToConstructor<TwoArgConstructor, int, int>);
    static_assert(activatorForwardsToConstructor<ExplicitConstructor, int>);
    static_assert(activatorForwardsToConstructor<std::string, int, char>);
    // Keep braces: aggregates have no constructor to forward to.
    static_assert(!activatorForwardsToConstructor<Aggregate, int, int>);
    static_assert(!activatorForwardsToConstructor<NestedAggregate, int, int, int>);
    static_assert(!activatorForwardsToConstructor<std::array<int, 3>, int, int, int>);
    // Keep braces: reachable only through an initializer_list constructor.
    static_assert(!activatorForwardsToConstructor<std::vector<int>, int, int, int>);
    static_assert(!activatorForwardsToConstructor<InitializerListOnly, int, int, int>);
    // Keep braces: non-class types, so scalar narrowing stays a compile error.
    static_assert(!activatorForwardsToConstructor<int, double>);
    static_assert(!activatorForwardsToConstructor<int, int>);
    SUCCEED();
}

// ===========================================================================
// Forwarding, which the repair must not have cost
// ===========================================================================

TEST(ActivatorConstructionPathTests, ValueCategoryIsStillForwarded) {
    int lvalue = 1;
    const int constLvalue = 1;
    EXPECT_STREQ(System::Activator::CreateInstance<ValueCategoryRecorder>(lvalue).how,
                 "lvalue");
    EXPECT_STREQ(System::Activator::CreateInstance<ValueCategoryRecorder>(constLvalue).how,
                 "const lvalue");
    EXPECT_STREQ(System::Activator::CreateInstance<ValueCategoryRecorder>(1).how, "rvalue");
}

TEST(ActivatorConstructionPathTests, MoveOnlyArgumentsStillForward) {
    const auto m = System::Activator::CreateInstance<MoveOnlyArgument>(std::make_unique<int>(11));
    ASSERT_NE(m.owned.get(), nullptr);
    EXPECT_EQ(*m.owned, 11);
}

TEST(ActivatorConstructionPathTests, ExplicitConstructorsAreStillReachable) {
    // Both spellings are direct-initialization, so an explicit constructor is
    // usable either way; pinned so a future switch to copy-initialization is
    // caught.
    const auto e = System::Activator::CreateInstance<ExplicitConstructor>(5);
    EXPECT_EQ(e.value, 5);
}

TEST(ActivatorConstructionPathTests, ConstructorExceptionsStillPropagate) {
    bool caught = false;
    try {
        (void)System::Activator::CreateInstance<ThrowingConstructor>(1);
    } catch (const std::string& message) {
        caught = true;
        EXPECT_EQ(message, "from ctor");
    }
    EXPECT_TRUE(caught);
}

TEST(ActivatorConstructionPathTests, OrdinaryConstructorsAreUnaffected) {
    EXPECT_EQ(System::Activator::CreateInstance<TwoArgConstructor>(1, 2).value, 102);
    EXPECT_EQ(System::Activator::CreateInstance<std::string>(std::string("hello")), "hello");
    // The zero-argument overload is a separate template and was not touched.
    EXPECT_EQ(System::Activator::CreateInstance<int>(), 0);
    EXPECT_TRUE(System::Activator::CreateInstance<std::string>().empty());
}

TEST(ActivatorConstructionPathTests, PostForwardingNarrowingCallIsNowAccepted) {
    // The one widening: std::string(3, 'x') is ill-formed under braces once the
    // 3 has been perfectly forwarded, because the narrowing exemption for
    // constant expressions no longer applies to a function parameter. The
    // pointer form already accepted it, so accepting it here removes a
    // value/pointer disagreement rather than creating one.
    const auto s = System::Activator::CreateInstance<std::string>(3, 'x');
    EXPECT_EQ(s, "xxx");
    const auto p = System::Activator::CreateInstancePtr<std::string>(3, 'x');
    ASSERT_NE(p.get(), nullptr);
    EXPECT_EQ(s, *p);
}
