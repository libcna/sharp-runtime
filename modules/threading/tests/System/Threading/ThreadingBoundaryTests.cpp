// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Permanent regressions for the System::Threading *public argument boundary* remediation
// family recorded in docs/ThreadingNamespaceReviewPlan.md:
//
//   ticket #1951 (cause T-B) -- empty std::function values crossing public boundaries,
//                               i.e. CCF-011 in modules/threading;
//   ticket #1952 (cause T-C) -- WaitHandle's four static multi-wait entry points, which
//                               validated neither the collection nor the timeout;
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

// ===========================================================================
// #1952 / SR-AUD-183 (cause T-C) -- WaitHandle's four static multi-wait entries
//
// Every one of them validated nothing: an empty collection produced `true`, `258`, or an
// unbounded loop; a null element was silently skipped; and a timeout below -1 produced
// `true` for an empty collection and `false` for a non-empty one. .NET's WaitMultiple
// validates in the order empty-collection, timeout, null-element, and all four entries here
// now do too.
// ===========================================================================

namespace {

    // Minimal concrete WaitHandle: signalled or not, no OS object involved.
    class SignalHandle final : public WaitHandle {
        bool signalled_;

    public:
        explicit SignalHandle(bool signalled) : signalled_(signalled) {}
        bool WaitOne() override { return signalled_; }
        bool WaitOne(SharpRuntime::intcs) override { return signalled_; }
    };

    // Asserts System::ArgumentException (not a derived type) naming `paramName`, so an
    // empty-collection rejection can never be satisfied by an ArgumentNullException.
    template <typename F>
    void ExpectEmptyCollection(F&& call) {
        bool threw = false;
        try {
            call();
        } catch (const System::ArgumentNullException&) {
            ADD_FAILURE() << "empty collection reported as a null element";
        } catch (const System::ArgumentException& e) {
            threw = true;
            EXPECT_EQ(e.getParamNameProperty(), "waitHandles");
            EXPECT_NE(std::string(e.what()).find("Waithandle array may not be empty."),
                      std::string::npos);
        }
        EXPECT_TRUE(threw);
    }

    template <typename F>
    void ExpectNullElement(F&& call, const char* paramName) {
        bool threw = false;
        try {
            call();
        } catch (const System::ArgumentNullException& e) {
            threw = true;
            EXPECT_EQ(e.getParamNameProperty(), paramName);
            EXPECT_NE(std::string(e.what()).find("At least one element in the specified array was null."),
                      std::string::npos);
        }
        EXPECT_TRUE(threw);
    }

}  // namespace

TEST(ThreadingMultiWaitValidationTests, EmptyCollection_RejectedByAllFourEntries) {
    const std::vector<WaitHandle*> empty{};
    ExpectEmptyCollection([&] { (void)WaitHandle::WaitAll(empty); });
    ExpectEmptyCollection([&] { (void)WaitHandle::WaitAll(empty, 0); });
    ExpectEmptyCollection([&] { (void)WaitHandle::WaitAny(empty); });
    ExpectEmptyCollection([&] { (void)WaitHandle::WaitAny(empty, 0); });
}

// The three loops that never terminated. Each of these calls used to spin forever with no
// handle to poll; if any regresses, this case hangs the suite rather than failing quietly,
// which is the only signal a non-terminating function can give.
TEST(ThreadingMultiWaitValidationTests, UnboundedWaitAnyShapes_TerminateWithADiagnostic) {
    const std::vector<WaitHandle*> empty{};
    const std::vector<WaitHandle*> nullOnly{nullptr};
    ExpectEmptyCollection([&] { (void)WaitHandle::WaitAny(empty); });
    ExpectEmptyCollection([&] { (void)WaitHandle::WaitAny(empty, -1); });
    ExpectNullElement([&] { (void)WaitHandle::WaitAny(nullOnly); }, "waitHandles[0]");
}

TEST(ThreadingMultiWaitValidationTests, NullElement_RejectedWithItsIndex) {
    SignalHandle signalled(true);
    const std::vector<WaitHandle*> nullOnly{nullptr};
    const std::vector<WaitHandle*> secondNull{&signalled, nullptr};
    const std::vector<WaitHandle*> thirdNull{&signalled, &signalled, nullptr};

    ExpectNullElement([&] { (void)WaitHandle::WaitAll(nullOnly); }, "waitHandles[0]");
    ExpectNullElement([&] { (void)WaitHandle::WaitAll(nullOnly, 0); }, "waitHandles[0]");
    ExpectNullElement([&] { (void)WaitHandle::WaitAny(nullOnly, 0); }, "waitHandles[0]");
    ExpectNullElement([&] { (void)WaitHandle::WaitAll(secondNull, 0); }, "waitHandles[1]");
    ExpectNullElement([&] { (void)WaitHandle::WaitAny(thirdNull, 0); }, "waitHandles[2]");
}

// The sharpest observable change in this ticket: a mixed collection used to return a
// plausible answer by silently skipping the null. .NET rejects it, so the port does now.
TEST(ThreadingMultiWaitValidationTests, MixedCollection_NoLongerSkipsTheNullSilently) {
    SignalHandle signalled(true);
    const std::vector<WaitHandle*> mixed{&signalled, nullptr};
    ExpectNullElement([&] { (void)WaitHandle::WaitAny(mixed, 0); }, "waitHandles[1]");
    ExpectNullElement([&] { (void)WaitHandle::WaitAll(mixed, 0); }, "waitHandles[1]");
}

TEST(ThreadingMultiWaitValidationTests, TimeoutBelowMinusOne_Rejected) {
    SignalHandle signalled(true);
    const std::vector<WaitHandle*> valid{&signalled};

    for (SharpRuntime::intcs bad : {static_cast<SharpRuntime::intcs>(-2),
                                    static_cast<SharpRuntime::intcs>(-100)}) {
        bool threw = false;
        try {
            (void)WaitHandle::WaitAll(valid, bad);
        } catch (const System::ArgumentOutOfRangeException& e) {
            threw = true;
            EXPECT_EQ(e.getParamNameProperty(), "millisecondsTimeout");
        }
        EXPECT_TRUE(threw);
        EXPECT_THROW((void)WaitHandle::WaitAny(valid, bad), System::ArgumentOutOfRangeException);
    }
}

// .NET's WaitMultiple checks the collection before the timeout, and the elements after it.
// Both boundaries are pinned so a later edit cannot reorder them unnoticed.
TEST(ThreadingMultiWaitValidationTests, ValidationOrder_EmptyBeatsTimeoutBeatsNullElement) {
    SignalHandle signalled(true);
    const std::vector<WaitHandle*> empty{};
    const std::vector<WaitHandle*> withNull{&signalled, nullptr};

    // empty + invalid timeout -> the collection error wins
    ExpectEmptyCollection([&] { (void)WaitHandle::WaitAll(empty, -2); });
    ExpectEmptyCollection([&] { (void)WaitHandle::WaitAny(empty, -2); });

    // null element + invalid timeout -> the timeout error wins
    EXPECT_THROW((void)WaitHandle::WaitAll(withNull, -2), System::ArgumentOutOfRangeException);
    EXPECT_THROW((void)WaitHandle::WaitAny(withNull, -2), System::ArgumentOutOfRangeException);
}

// The documented sequential/polling adaptation for valid input must be untouched, including
// the -1 and 0 special cases both overloads carry.
TEST(ThreadingMultiWaitValidationTests, ValidInput_BehaviourUnchanged) {
    SignalHandle signalled(true);
    SignalHandle unsignalled(false);
    const std::vector<WaitHandle*> allSignalled{&signalled, &signalled};
    const std::vector<WaitHandle*> secondSignalled{&unsignalled, &signalled};

    EXPECT_TRUE(WaitHandle::WaitAll(allSignalled));
    EXPECT_TRUE(WaitHandle::WaitAll(allSignalled, -1));
    EXPECT_TRUE(WaitHandle::WaitAll(allSignalled, 0));

    EXPECT_EQ(WaitHandle::WaitAny(allSignalled), 0);
    EXPECT_EQ(WaitHandle::WaitAny(allSignalled, -1), 0);
    EXPECT_EQ(WaitHandle::WaitAny(allSignalled, 0), 0);
    EXPECT_EQ(WaitHandle::WaitAny(secondSignalled, 0), 1);

    EXPECT_FALSE(WaitHandle::WaitAll(secondSignalled, 0));
    const std::vector<WaitHandle*> noneSignalled{&unsignalled};
    EXPECT_EQ(WaitHandle::WaitAny(noneSignalled, 20), WaitHandle::WaitTimeout);
}
