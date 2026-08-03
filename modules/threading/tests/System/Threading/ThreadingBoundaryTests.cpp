// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Permanent regressions for the System::Threading *public argument boundary* remediation
// family recorded in docs/ThreadingNamespaceReviewPlan.md:
//
//   ticket #1951 (cause T-B) -- empty std::function values crossing public boundaries,
//                               i.e. CCF-011 in modules/threading;
//
// Every case here pins an input a caller can supply that used to reach
// std::bad_function_call, std::terminate, a silent no-op or a silently wrong value, plus
// the neighbouring valid input that must keep working unchanged.
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <thread>

#include "System/ArgumentNullException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Exception.hpp"
#include "System/NullReferenceException.hpp"
#include "System/Threading/CancellationToken.hpp"
#include "System/Threading/CancellationTokenRegistration.hpp"
#include "System/Threading/CancellationTokenSource.hpp"
#include "System/Threading/LazyInitializer.hpp"
#include "System/Threading/SpinWait.hpp"
#include "System/Threading/SynchronizationContext.hpp"
#include "System/Threading/Thread.hpp"
#include "System/Threading/ThreadLocal.hpp"
#include "System/Threading/Timer.hpp"

using namespace System::Threading;

namespace {

    // Asserts that `call` throws System::ArgumentNullException naming `paramName`, and that
    // the thrown object is reachable through the base handlers ported C# code actually
    // writes. The last part is the substance of CCF-011: std::bad_function_call satisfies
    // none of them.
    template <typename F>
    void ExpectArgumentNull(F&& call, const char* paramName) {
        bool threw = false;
        try {
            call();
        } catch (const System::ArgumentNullException& e) {
            threw = true;
            EXPECT_EQ(e.getParamNameProperty(), paramName);
        }
        EXPECT_TRUE(threw) << "expected ArgumentNullException(" << paramName << ")";
        EXPECT_THROW(call(), System::ArgumentException);
        EXPECT_THROW(call(), System::Exception);
    }

    struct Boxed {
        int value = 7;
    };

}  // namespace

// ===========================================================================
// #1951 / SR-AUD-190 -- Timer's callback
// ===========================================================================

TEST(ThreadingEmptyCallableTests, Timer_EmptyCallback_ThrowsArgumentNullException) {
    ExpectArgumentNull([] { Timer t(std::function<void(void*)>{}, nullptr, 0, -1); }, "callback");
}

// .NET's Timer constructor runs ThrowIfLessThan(dueTime, -1) and ThrowIfLessThan(period, -1)
// before TimerSetup's ThrowIfNull(callback), so an out-of-range due time wins over a null
// callback. The order is part of the observable, not an implementation detail.
TEST(ThreadingEmptyCallableTests, Timer_EmptyCallbackAndBadRange_RangeErrorWins) {
    EXPECT_THROW(Timer(std::function<void(void*)>{}, nullptr, -5, 0),
                 System::ArgumentOutOfRangeException);
    EXPECT_THROW(Timer(std::function<void(void*)>{}, nullptr, 0, -5),
                 System::ArgumentOutOfRangeException);
}

TEST(ThreadingEmptyCallableTests, Timer_NonEmptyCallback_StillFires) {
    std::atomic<int> fired{0};
    Timer t([&fired](void*) { fired.fetch_add(1); }, nullptr, 1, -1);
    for (int i = 0; i < 200 && fired.load() == 0; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    EXPECT_GE(fired.load(), 1);
}

// ===========================================================================
// #1951 / SR-AUD-192 -- Thread's start function
// ===========================================================================

TEST(ThreadingEmptyCallableTests, Thread_EmptyStart_ThrowsArgumentNullException) {
    ExpectArgumentNull([] { Thread t{std::function<void()>{}}; }, "start");
}

// The no-partial-state half of the acceptance criteria, made observable: a rejected
// construction must not consume a managed thread id, so two successful constructions
// separated by any number of failed ones stay consecutive. Before the repair the
// construction succeeded outright, so there was no rejected construction to test.
TEST(ThreadingEmptyCallableTests, Thread_EmptyStart_ConsumesNoManagedThreadId) {
    Thread before{std::function<void()>([] {})};
    const auto idBefore = before.getManagedThreadIdProperty();

    for (int i = 0; i < 3; ++i)
        EXPECT_THROW(Thread{std::function<void()>{}}, System::ArgumentNullException);

    Thread after{std::function<void()>([] {})};
    EXPECT_EQ(after.getManagedThreadIdProperty(), idBefore + 1);
}

TEST(ThreadingEmptyCallableTests, Thread_NonEmptyStart_StillRuns) {
    std::atomic<int> ran{0};
    Thread t{std::function<void()>([&ran] { ran.store(1); })};
    t.Start();
    t.Join();
    EXPECT_EQ(ran.load(), 1);
}

// ===========================================================================
// #1951 / SR-AUD-198 -- CancellationToken::Register
// ===========================================================================

TEST(ThreadingEmptyCallableTests, CancellationToken_RegisterEmpty_ThrowsArgumentNullException) {
    CancellationToken token;
    ExpectArgumentNull([&token] { (void)token.Register(std::function<void()>{}); }, "callback");
}

// .NET rejects the null delegate before consulting the token's state, so the
// already-cancelled fast path -- which would otherwise invoke the callback immediately --
// throws too rather than silently doing nothing.
TEST(ThreadingEmptyCallableTests, CancellationToken_RegisterEmptyOnCancelledToken_Throws) {
    CancellationTokenSource source;
    source.Cancel();
    CancellationToken token = source.getTokenProperty();
    ExpectArgumentNull([&token] { (void)token.Register(std::function<void()>{}); }, "callback");
}

TEST(ThreadingEmptyCallableTests, CancellationToken_RegisterEmpty_RecordsNoRegistration) {
    CancellationTokenSource source;
    CancellationToken token = source.getTokenProperty();

    EXPECT_THROW((void)token.Register(std::function<void()>{}), System::ArgumentNullException);

    std::atomic<int> ran{0};
    auto reg = token.Register([&ran] { ran.fetch_add(1); });
    source.Cancel();
    EXPECT_EQ(ran.load(), 1);  // exactly the one real registration, no skipped empty slot
    (void)reg;
}

// ===========================================================================
// #1951 / SR-AUD-213 (callable half) -- SpinWait::SpinUntil
// ===========================================================================

TEST(ThreadingEmptyCallableTests, SpinWait_SpinUntilEmptyCondition_ThrowsArgumentNullException) {
    ExpectArgumentNull([] { SpinWait::SpinUntil(std::function<bool()>{}); }, "condition");
}

TEST(ThreadingEmptyCallableTests, SpinWait_SpinUntilTimedEmptyCondition_ThrowsArgumentNullException) {
    ExpectArgumentNull([] { (void)SpinWait::SpinUntil(std::function<bool()>{}, 50); }, "condition");
}

// The infinite-timeout overload is the case that proves the check precedes the loop: with
// -1 there is no deadline to end the spin, so a deferred check would hang the suite instead
// of failing it.
TEST(ThreadingEmptyCallableTests, SpinWait_SpinUntilEmptyConditionInfinite_ThrowsRatherThanHangs) {
    ExpectArgumentNull([] { (void)SpinWait::SpinUntil(std::function<bool()>{}, -1); }, "condition");
}

TEST(ThreadingEmptyCallableTests, SpinWait_NonEmptyCondition_StillCompletes) {
    int calls = 0;
    SpinWait::SpinUntil([&calls] { return ++calls >= 3; });
    EXPECT_GE(calls, 3);
    EXPECT_TRUE(SpinWait::SpinUntil([] { return true; }, 50));
    EXPECT_FALSE(SpinWait::SpinUntil([] { return false; }, 1));
}

// ===========================================================================
// #1951 / SR-AUD-217 -- LazyInitializer's factory
//
// The one site in this family whose .NET answer is NullReferenceException rather than
// ArgumentNullException: LazyInitializer.EnsureInitialized performs no null check and is
// short-circuited by an already-initialized target, so both the exception type and the
// data-dependence are reproduced deliberately.
// ===========================================================================

TEST(ThreadingEmptyCallableTests, LazyInitializer_EmptyFactoryNullTarget_ThrowsNullReference) {
    Boxed* target = nullptr;
    EXPECT_THROW(LazyInitializer::EnsureInitialized<Boxed>(target, std::function<Boxed*()>{}),
                 System::NullReferenceException);
    // and it is a System::Exception, which std::bad_function_call was not
    EXPECT_THROW(LazyInitializer::EnsureInitialized<Boxed>(target, std::function<Boxed*()>{}),
                 System::Exception);
    EXPECT_EQ(target, nullptr);  // nothing was published
}

TEST(ThreadingEmptyCallableTests, LazyInitializer_EmptyFactoryInitializedTarget_ReturnsExisting) {
    Boxed existing;
    Boxed* target = &existing;
    Boxed& got = LazyInitializer::EnsureInitialized<Boxed>(target, std::function<Boxed*()>{});
    EXPECT_EQ(&got, &existing);
    EXPECT_EQ(got.value, 7);
}

TEST(ThreadingEmptyCallableTests, LazyInitializer_NonEmptyFactory_StillInitializes) {
    Boxed* target = nullptr;
    Boxed& got = LazyInitializer::EnsureInitialized<Boxed>(
        target, std::function<Boxed*()>([] { return new Boxed(); }));
    EXPECT_EQ(got.value, 7);
    EXPECT_EQ(target, &got);
    delete target;
}

// ===========================================================================
// #1951 / SR-AUD-219 (factory half) -- ThreadLocal's factory constructors
// ===========================================================================

TEST(ThreadingEmptyCallableTests, ThreadLocal_EmptyFactory_ThrowsArgumentNullException) {
    ExpectArgumentNull([] { ThreadLocal<int> tl{std::function<int()>{}}; }, "valueFactory");
}

TEST(ThreadingEmptyCallableTests, ThreadLocal_EmptyFactoryWithTracking_ThrowsArgumentNullException) {
    ExpectArgumentNull([] { ThreadLocal<int> tl(std::function<int()>{}, true); }, "valueFactory");
}

// Pins the *corrected* consequence of SR-AUD-219 measured by
// build-probe/1951_probe1_threading_empty_callables.cpp: an accepted empty factory did not
// raise bad_function_call at first access, it silently handed back a default-constructed
// value. No factory constructor may reach that path again.
TEST(ThreadingEmptyCallableTests, ThreadLocal_EmptyFactory_NeverYieldsSilentDefault) {
    bool constructed = false;
    try {
        ThreadLocal<int> tl{std::function<int()>{}};
        constructed = true;
        EXPECT_NE(tl.getValueProperty(), 0) << "an empty factory silently produced T{}";
    } catch (const System::ArgumentNullException&) {
        // expected: rejected at construction
    }
    EXPECT_FALSE(constructed);
}

// The two non-factory constructors legitimately leave the factory empty and must keep
// defaulting; the new check must not reach them.
TEST(ThreadingEmptyCallableTests, ThreadLocal_NonFactoryConstructors_StillDefault) {
    ThreadLocal<int> plain;
    EXPECT_EQ(plain.getValueProperty(), 0);

    ThreadLocal<int> tracked{true};
    EXPECT_EQ(tracked.getValueProperty(), 0);
}

TEST(ThreadingEmptyCallableTests, ThreadLocal_NonEmptyFactory_StillInitializes) {
    ThreadLocal<int> tl{std::function<int()>([] { return 42; })};
    EXPECT_FALSE(tl.getIsValueCreatedProperty());
    EXPECT_EQ(tl.getValueProperty(), 42);
    EXPECT_TRUE(tl.getIsValueCreatedProperty());

    ThreadLocal<int> tracked(std::function<int()>([] { return 43; }), true);
    EXPECT_EQ(tracked.getValueProperty(), 43);
}

// ===========================================================================
// #1951 / SR-AUD-222 -- SynchronizationContext::Send
// ===========================================================================

TEST(ThreadingEmptyCallableTests, SynchronizationContext_SendEmpty_ThrowsNullReference) {
    SynchronizationContext ctx;
    EXPECT_THROW(ctx.Send(SendOrPostCallback{}, nullptr), System::NullReferenceException);
    EXPECT_THROW(ctx.Send(SendOrPostCallback{}, nullptr), System::Exception);
}

TEST(ThreadingEmptyCallableTests, SynchronizationContext_SendNonEmpty_StillRunsSynchronously) {
    SynchronizationContext ctx;
    int hits = 0;
    int state = 5;
    ctx.Send([&hits, &state](void* s) { hits += *static_cast<int*>(s); }, &state);
    EXPECT_EQ(hits, 5);
}

// .NET's base Post returns normally to its caller for a null delegate, so this port must
// too. Recorded as a control: SR-AUD-222 names Send only.
TEST(ThreadingEmptyCallableTests, SynchronizationContext_PostEmpty_ReturnsNormally) {
    SynchronizationContext ctx;
    EXPECT_NO_THROW(ctx.Post(SendOrPostCallback{}, nullptr));
}
