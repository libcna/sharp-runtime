// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <functional>
#include <stdexcept>
#include <string>
#include "System/ArgumentNullException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Exception.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/Lazy.hpp"

using System::Lazy;
using System::LazyThreadSafetyMode;

TEST(LazyTests, DefaultCtor_NotCreated) {
    Lazy<int> lz;
    EXPECT_FALSE(lz.getIsValueCreatedProperty());
}

TEST(LazyTests, DefaultCtor_AccessCreatesDefault) {
    Lazy<int> lz;
    EXPECT_EQ(lz.getValueProperty(), 0);
    EXPECT_TRUE(lz.getIsValueCreatedProperty());
}

TEST(LazyTests, PreInitCtor_IsValueCreatedTrue) {
    Lazy<int> lz(42);
    EXPECT_TRUE(lz.getIsValueCreatedProperty());
}

TEST(LazyTests, PreInitCtor_ValueCorrect) {
    Lazy<int> lz(42);
    EXPECT_EQ(lz.getValueProperty(), 42);
}

TEST(LazyTests, FactoryCtor_NotCreatedBeforeAccess) {
    int calls = 0;
    Lazy<int> lz([&] { ++calls; return 99; });
    EXPECT_EQ(calls, 0);
    EXPECT_FALSE(lz.getIsValueCreatedProperty());
}

TEST(LazyTests, FactoryCtor_ValueCorrectOnAccess) {
    Lazy<int> lz([] { return 7; });
    EXPECT_EQ(lz.getValueProperty(), 7);
}

TEST(LazyTests, FactoryCtor_FactoryCalledOnce) {
    int calls = 0;
    Lazy<int> lz([&] { ++calls; return 1; });
    lz.getValueProperty();
    lz.getValueProperty();
    EXPECT_EQ(calls, 1);
}

TEST(LazyTests, BoolCtor_ThreadSafe_DefaultConstruct) {
    Lazy<std::string> lz(true);
    EXPECT_FALSE(lz.getIsValueCreatedProperty());
    EXPECT_EQ(lz.getValueProperty(), "");
}

TEST(LazyTests, ModeCtor_DefaultConstruct) {
    Lazy<double> lz(LazyThreadSafetyMode::None);
    EXPECT_EQ(lz.getValueProperty(), 0.0);
}

TEST(LazyTests, FactoryBoolCtor_WorksCorrectly) {
    Lazy<int> lz([] { return 55; }, false);
    EXPECT_EQ(lz.getValueProperty(), 55);
}

TEST(LazyTests, FactoryModeCtor_WorksCorrectly) {
    Lazy<int> lz([] { return 33; }, LazyThreadSafetyMode::None);
    EXPECT_EQ(lz.getValueProperty(), 33);
}

TEST(LazyTests, IsValueCreated_AfterAccess_True) {
    Lazy<int> lz([] { return 1; });
    lz.getValueProperty();
    EXPECT_TRUE(lz.getIsValueCreatedProperty());
}

TEST(LazyTests, Value_Alias_SameAsGetValueProperty) {
    Lazy<int> lz([] { return 21; });
    EXPECT_EQ(lz.Value(), lz.getValueProperty());
}

TEST(LazyTests, ToString_BeforeAccess) {
    Lazy<int> lz([] { return 0; });
    EXPECT_NE(lz.ToString().find("not"), std::string::npos);
}

TEST(LazyTests, ToString_AfterAccess) {
    Lazy<int> lz([] { return 0; });
    lz.getValueProperty();
    EXPECT_EQ(lz.ToString().find("not"), std::string::npos);
}

TEST(LazyTests, GetMode_DefaultIsSafe) {
    Lazy<int> lz;
    EXPECT_EQ(lz.getModeProperty(), LazyThreadSafetyMode::ExecutionAndPublication);
}

// --- Fault-caching semantics (matches .NET's LazyHelper: None and
// ExecutionAndPublication cache and rethrow the same exception on every later
// access; PublicationOnly does not cache and retries the factory instead) ---

TEST(LazyTests, ExecutionAndPublication_FaultIsCached_SameExceptionRethrown) {
    int calls = 0;
    Lazy<int> lz([&]() -> int { ++calls; throw std::runtime_error("boom"); }, LazyThreadSafetyMode::ExecutionAndPublication);
    EXPECT_THROW(lz.getValueProperty(), std::runtime_error);
    EXPECT_THROW(lz.getValueProperty(), std::runtime_error);
    EXPECT_EQ(calls, 1); // factory not retried - same fault rethrown
    EXPECT_FALSE(lz.getIsValueCreatedProperty());
}

TEST(LazyTests, None_FaultIsCached_SameExceptionRethrown) {
    int calls = 0;
    Lazy<int> lz([&]() -> int { ++calls; throw std::runtime_error("boom"); }, LazyThreadSafetyMode::None);
    EXPECT_THROW(lz.getValueProperty(), std::runtime_error);
    EXPECT_THROW(lz.getValueProperty(), std::runtime_error);
    EXPECT_EQ(calls, 1);
}

TEST(LazyTests, PublicationOnly_FaultNotCached_FactoryRetried) {
    int calls = 0;
    Lazy<int> lz([&]() -> int {
        ++calls;
        if (calls < 3) throw std::runtime_error("not yet");
        return 42;
    }, LazyThreadSafetyMode::PublicationOnly);
    EXPECT_THROW(lz.getValueProperty(), std::runtime_error);
    EXPECT_THROW(lz.getValueProperty(), std::runtime_error);
    EXPECT_EQ(lz.getValueProperty(), 42); // third attempt succeeds
    EXPECT_EQ(calls, 3);
    EXPECT_TRUE(lz.getIsValueCreatedProperty());
}

TEST(LazyTests, ExecutionAndPublication_SucceedsAfterNoFault) {
    Lazy<int> lz([]() { return 7; }, LazyThreadSafetyMode::ExecutionAndPublication);
    EXPECT_EQ(lz.getValueProperty(), 7);
    EXPECT_EQ(lz.getValueProperty(), 7);
}

// --- Recursive Value() access from within the factory ---

TEST(LazyTests, RecursiveValueAccess_ThrowsInvalidOperationException) {
    Lazy<int>* self = nullptr;
    Lazy<int> lz([&]() -> int { return self->getValueProperty(); });
    self = &lz;
    EXPECT_THROW(lz.getValueProperty(), System::InvalidOperationException);
}

// --- ToString() forwards to the value's own string representation ---

TEST(LazyTests, ToString_ForwardsToValueToString_ForTypesThatHaveIt) {
    struct Widget {
        [[nodiscard]] std::string ToString() const { return "a-widget"; }
    };
    Lazy<Widget> lz([] { return Widget{}; });
    lz.getValueProperty();
    EXPECT_EQ(lz.ToString(), "a-widget");
}

// --- Empty std::function factory rejected at construction (#1867 / SR-AUD-065 / CCF-011) ---
//
// The three factory constructors used to accept an empty std::function<T()>, store it and
// raise the native std::bad_function_call from the first getValueProperty() -- outside the
// System::Exception hierarchy, so ported `catch (const Exception&)` code could not see it.
// .NET rejects a null Func<T> in the constructor (`Lazy.cs`:
// `ArgumentNullException.ThrowIfNull(valueFactory)`). See docs/EmptyCallableBoundaryPlan.md.

TEST(LazyTests, EmptyFactory_ThrowsArgumentNullExceptionAtConstruction) {
    EXPECT_THROW(Lazy<int>{std::function<int()>{}}, System::ArgumentNullException);
}

TEST(LazyTests, EmptyFactory_WithIsThreadSafeFlag_ThrowsAtConstruction) {
    EXPECT_THROW((Lazy<int>{std::function<int()>{}, true}), System::ArgumentNullException);
    EXPECT_THROW((Lazy<int>{std::function<int()>{}, false}), System::ArgumentNullException);
}

TEST(LazyTests, EmptyFactory_WithMode_ThrowsAtConstruction) {
    EXPECT_THROW((Lazy<int>{std::function<int()>{}, LazyThreadSafetyMode::None}),
                 System::ArgumentNullException);
    EXPECT_THROW((Lazy<int>{std::function<int()>{}, LazyThreadSafetyMode::PublicationOnly}),
                 System::ArgumentNullException);
    EXPECT_THROW((Lazy<int>{std::function<int()>{}, LazyThreadSafetyMode::ExecutionAndPublication}),
                 System::ArgumentNullException);
}

TEST(LazyTests, EmptyFactory_ParamNameIsValueFactory) {
    try {
        Lazy<int> lz{std::function<int()>{}};
        FAIL() << "expected ArgumentNullException";
    } catch (const System::ArgumentNullException& e) {
        EXPECT_STREQ(e.what(), "Value cannot be null. (Parameter 'valueFactory')");
    }
}

TEST(LazyTests, EmptyFactory_IsCatchableAsSystemException) {
    // std::bad_function_call was not: this is the whole point of the repair.
    bool caught = false;
    try {
        Lazy<int> lz{std::function<int()>{}};
    } catch (const System::Exception&) {
        caught = true;
    }
    EXPECT_TRUE(caught);
}

TEST(LazyTests, EmptyFactory_NeverReachesBadFunctionCall) {
    EXPECT_NO_THROW({
        try {
            Lazy<int> lz{std::function<int()>{}};
        } catch (const System::ArgumentNullException&) {
            // expected; a std::bad_function_call would escape this handler and fail the test
        }
    });
}

TEST(LazyTests, NonEmptyStdFunctionFactory_StillWorks) {
    std::function<int()> factory = [] { return 41; };
    Lazy<int> lz{factory};
    EXPECT_FALSE(lz.getIsValueCreatedProperty());
    EXPECT_EQ(lz.getValueProperty(), 41);
    EXPECT_TRUE(lz.getIsValueCreatedProperty());
}

TEST(LazyTests, NonFactoryConstructors_UnaffectedByTheEmptyCheck) {
    // These synthesise their own factory, which is never empty.
    EXPECT_NO_THROW({ Lazy<int> a; (void)a.getValueProperty(); });
    EXPECT_NO_THROW({ Lazy<int> b(5); (void)b.getValueProperty(); });
    EXPECT_NO_THROW({ Lazy<int> c(true); (void)c.getValueProperty(); });
    EXPECT_NO_THROW({ Lazy<int> d(LazyThreadSafetyMode::None); (void)d.getValueProperty(); });
}

TEST(LazyTests, EmptyFactory_ThrowingConstructorLeavesNothingBehind) {
    // A constructor that throws destroys the members it already built; repeat it enough
    // times that a leaked std::function target would be visible to LeakSanitizer.
    for (int i = 0; i < 1000; ++i) {
        try {
            Lazy<std::string> lz{std::function<std::string()>{}};
            FAIL() << "expected ArgumentNullException";
        } catch (const System::ArgumentNullException&) {
        }
    }
    SUCCEED();
}

// --- Invalid LazyThreadSafetyMode rejected at construction (#2236 / SR-AUD-064) ---
//
// Both mode-taking constructors used to store any LazyThreadSafetyMode value. The value
// stayed observable through getModeProperty(), and getValueProperty()'s switch routed it
// through its `default:` label into the PublicationOnly arm -- so the instance did not
// merely tolerate the invalid mode, it silently acquired PublicationOnly's fault-caching
// contract. .NET's LazyHelper.Create throws ArgumentOutOfRangeException(nameof(mode)) from
// the constructor instead. See docs/CoreLazyThreadSafetyModeFamilyPlan.md section 4.1.

namespace {
constexpr LazyThreadSafetyMode kInvalidMode  = static_cast<LazyThreadSafetyMode>(99);
constexpr LazyThreadSafetyMode kInvalidMode2 = static_cast<LazyThreadSafetyMode>(-1);
} // namespace

TEST(LazyTests, InvalidMode_DefaultConstructForm_Throws) {
    EXPECT_THROW(Lazy<int>{kInvalidMode},  System::ArgumentOutOfRangeException);
    EXPECT_THROW(Lazy<int>{kInvalidMode2}, System::ArgumentOutOfRangeException);
}

TEST(LazyTests, InvalidMode_FactoryForm_Throws) {
    EXPECT_THROW((Lazy<int>{[] { return 1; }, kInvalidMode}),
                 System::ArgumentOutOfRangeException);
    EXPECT_THROW((Lazy<int>{[] { return 1; }, kInvalidMode2}),
                 System::ArgumentOutOfRangeException);
}

TEST(LazyTests, InvalidMode_ParamNameIsMode) {
    try {
        Lazy<int> lz(kInvalidMode);
        FAIL() << "expected ArgumentOutOfRangeException";
    } catch (const System::ArgumentOutOfRangeException& e) {
        EXPECT_EQ(e.getParamNameProperty(), "mode");
        EXPECT_STREQ(e.what(),
                     "The mode argument specifies an invalid value. (Parameter 'mode')");
    }
}

TEST(LazyTests, InvalidMode_IsCatchableAsSystemException) {
    try {
        Lazy<int> lz([] { return 1; }, kInvalidMode);
        FAIL() << "expected ArgumentOutOfRangeException";
    } catch (const System::Exception&) {
        SUCCEED();
    }
}

TEST(LazyTests, InvalidMode_FactoryIsCheckedFirst) {
    // .NET's Lazy(Func<T>, LazyThreadSafetyMode) does ThrowIfNull(valueFactory) before
    // LazyHelper.Create(mode), so an empty factory wins even when the mode is also invalid.
    EXPECT_THROW((Lazy<int>{std::function<int()>{}, kInvalidMode}),
                 System::ArgumentNullException);
}

TEST(LazyTests, ValidModes_StillConstructThroughBothModeTakingCtors) {
    for (auto mode : {LazyThreadSafetyMode::None,
                      LazyThreadSafetyMode::PublicationOnly,
                      LazyThreadSafetyMode::ExecutionAndPublication}) {
        EXPECT_NO_THROW({ Lazy<int> a(mode); EXPECT_EQ(a.getModeProperty(), mode); });
        EXPECT_NO_THROW({
            Lazy<int> b([] { return 8; }, mode);
            EXPECT_EQ(b.getModeProperty(), mode);
            EXPECT_EQ(b.getValueProperty(), 8);
        });
    }
}

TEST(LazyTests, NonModeConstructors_UnaffectedByTheModeCheck) {
    EXPECT_NO_THROW({ Lazy<int> a; (void)a.getValueProperty(); });
    EXPECT_NO_THROW({ Lazy<int> b(5); (void)b.getValueProperty(); });
    EXPECT_NO_THROW({ Lazy<int> c(true);  (void)c.getValueProperty(); });
    EXPECT_NO_THROW({ Lazy<int> d(false); (void)d.getValueProperty(); });
    EXPECT_NO_THROW({ Lazy<int> e([] { return 1; }, true);  (void)e.getValueProperty(); });
    EXPECT_NO_THROW({ Lazy<int> f([] { return 1; }, false); (void)f.getValueProperty(); });
    EXPECT_NO_THROW({ Lazy<int> g([] { return 1; }); (void)g.getValueProperty(); });
}

TEST(LazyTests, InvalidMode_ThrowingConstructorLeavesNothingBehind) {
    // The constructor throws from its body, after factory_ was built; repeat it enough
    // times that a leaked std::function target would be visible to LeakSanitizer.
    for (int i = 0; i < 1000; ++i) {
        try {
            Lazy<std::string> lz([] { return std::string(64, 'x'); }, kInvalidMode);
            FAIL() << "expected ArgumentOutOfRangeException";
        } catch (const System::ArgumentOutOfRangeException&) {
        }
    }
    SUCCEED();
}

// --- PublicationOnly reentrancy is a DELIBERATE, documented deviation (#2237 / SR-AUD-066) ---
//
// .NET raises InvalidOperationException for a recursive Value() in None and
// ExecutionAndPublication only; its PublicationOnly does not detect recursion at all.
// This port raises it in all three modes because the recursive call would otherwise
// re-lock the non-recursive publicationOnlyMutex_ -- undefined behaviour per
// [thread.mutex.requirements.mutex] -- and because both routes to .NET's behaviour let an
// unconditionally recursive factory recurse without bound. These tests pin the deviation
// so it cannot move silently in either direction; ticket #2238 is the reopening path.
// See docs/CoreLazyThreadSafetyModeFamilyPlan.md section 4.2.

namespace {
// Builds a Lazy<int> whose factory re-enters getValueProperty() on the same instance.
void expectRecursionRejected(LazyThreadSafetyMode mode) {
    Lazy<int>* self = nullptr;
    Lazy<int> lz([&self]() -> int { return self->getValueProperty(); }, mode);
    self = &lz;
    EXPECT_THROW((void)lz.getValueProperty(), System::InvalidOperationException);
}
} // namespace

TEST(LazyTests, RecursiveValueAccess_RejectedInAllThreeModes) {
    expectRecursionRejected(LazyThreadSafetyMode::None);
    expectRecursionRejected(LazyThreadSafetyMode::ExecutionAndPublication);
    // The deliberate deviation: .NET would invoke the factory again here.
    expectRecursionRejected(LazyThreadSafetyMode::PublicationOnly);
}

TEST(LazyTests, RecursiveValueAccess_MessageIsStable) {
    Lazy<int>* self = nullptr;
    Lazy<int> lz([&self]() -> int { return self->getValueProperty(); },
                 LazyThreadSafetyMode::PublicationOnly);
    self = &lz;
    try {
        (void)lz.getValueProperty();
        FAIL() << "expected InvalidOperationException";
    } catch (const System::InvalidOperationException& e) {
        EXPECT_STREQ(e.what(),
                     "ValueFactory attempted to access the Value property of this instance.");
    }
}

TEST(LazyTests, PublicationOnly_NonRecursiveFactory_IsUnaffected) {
    int calls = 0;
    Lazy<int> lz([&calls] { ++calls; return 5; }, LazyThreadSafetyMode::PublicationOnly);
    EXPECT_EQ(lz.getValueProperty(), 5);
    EXPECT_EQ(lz.getValueProperty(), 5);
    EXPECT_EQ(calls, 1);
    EXPECT_TRUE(lz.getIsValueCreatedProperty());
}

TEST(LazyTests, PublicationOnly_FaultStillRetries_AfterARejectedRecursion) {
    // The reentrancy guard must not poison the instance: PublicationOnly does not cache
    // faults, so a later non-recursive access still succeeds.
    Lazy<int>* self = nullptr;
    bool recurse = true;
    Lazy<int> lz([&self, &recurse]() -> int {
                     if (recurse) return self->getValueProperty();
                     return 42;
                 },
                 LazyThreadSafetyMode::PublicationOnly);
    self = &lz;
    EXPECT_THROW((void)lz.getValueProperty(), System::InvalidOperationException);
    EXPECT_FALSE(lz.getIsValueCreatedProperty());
    recurse = false;
    EXPECT_EQ(lz.getValueProperty(), 42);
    EXPECT_TRUE(lz.getIsValueCreatedProperty());
}

TEST(LazyTests, RecursionIntoADifferentInstance_StaysLegalInEveryMode) {
    for (auto mode : {LazyThreadSafetyMode::None,
                      LazyThreadSafetyMode::PublicationOnly,
                      LazyThreadSafetyMode::ExecutionAndPublication}) {
        Lazy<int> inner([] { return 3; }, mode);
        Lazy<int> outer([&inner] { return inner.getValueProperty() + 1; }, mode);
        EXPECT_EQ(outer.getValueProperty(), 4);
    }
}
