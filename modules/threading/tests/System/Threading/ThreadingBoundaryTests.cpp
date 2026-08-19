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
//   ticket #1953 (cause T-C) -- the two boundaries that reached a null dereference:
//                               ThreadPool::RegisterWaitForSingleObject and
//                               CancellationToken's shared-state constructor;
//   ticket #1954 (cause T-C) -- the out-of-domain enum and timeout values;
//
// Every case here pins an input a caller can supply that used to reach
// std::bad_function_call, std::terminate, a silent no-op or a silently wrong value, plus
// the neighbouring valid input that must keep working unchanged.
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#include "System/ArgumentNullException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Exception.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/NullReferenceException.hpp"
#include "System/ObjectDisposedException.hpp"
#include "System/OperationCanceledException.hpp"
#include "System/Threading/Barrier.hpp"
#include "System/Threading/CancellationToken.hpp"
#include "System/Threading/CancellationTokenRegistration.hpp"
#include "System/Threading/CancellationTokenSource.hpp"
#include "System/Threading/EventWaitHandle.hpp"
#include "System/Threading/LazyInitializer.hpp"
#include "System/Threading/AutoResetEvent.hpp"
#include "System/Threading/ManualResetEvent.hpp"
#include "System/Threading/Mutex.hpp"
#include "System/Threading/PeriodicTimer.hpp"
#include "System/Threading/ReaderWriterLockSlim.hpp"
#include "System/Threading/Semaphore.hpp"
#include "System/Threading/SynchronizationLockException.hpp"
#include "System/Threading/SpinWait.hpp"
#include "System/Threading/SynchronizationContext.hpp"
#include "System/Threading/Thread.hpp"
#include "System/Threading/ThreadPool.hpp"
#include "System/Threading/ThreadLocal.hpp"
#include "System/Threading/Timer.hpp"
#include "System/TimeProvider.hpp"
#include "System/TimeSpan.hpp"

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

// ===========================================================================
// #1953 / SR-AUD-188 + SR-AUD-199 (cause T-C) -- the two boundaries that reached a null
// dereference rather than a wrong diagnostic.
// ===========================================================================

// SR-AUD-188. A null waitObject used to produce a registration that RETURNED NORMALLY and
// then killed the process from the background thread at waitObject->WaitOne() -- a crash the
// caller could neither observe nor catch. The check now lives in RegisteredWaitHandle's
// constructor, so no std::thread exists for an invalid registration.
TEST(ThreadingNullArgumentTests, RegisterWaitForSingleObject_NullWaitObject_ThrowsSynchronously) {
    ExpectArgumentNull(
        [] {
            (void)ThreadPool::RegisterWaitForSingleObject(
                nullptr, [](void*, bool) {}, nullptr, 100, true);
        },
        "waitObject");
}

// Recorded in RegisteredWaitHandle.hpp.audit.md's own "Other missing assertions" as part of
// SR-AUD-188's repair: the C++ probe reported empty_callback=normal and timeout_minus2=normal
// where the managed probe reported argument_null and argument_out_of_range.
TEST(ThreadingNullArgumentTests, RegisterWaitForSingleObject_EmptyCallback_Throws) {
    Semaphore s(1, 1);
    ExpectArgumentNull(
        [&s] {
            (void)ThreadPool::RegisterWaitForSingleObject(
                &s, WaitOrTimerCallback{}, nullptr, 50, true);
        },
        "callBack");
}

TEST(ThreadingNullArgumentTests, RegisterWaitForSingleObject_TimeoutBelowMinusOne_Throws) {
    Semaphore s(1, 1);
    bool threw = false;
    try {
        (void)ThreadPool::RegisterWaitForSingleObject(&s, [](void*, bool) {}, nullptr, -2, true);
    } catch (const System::ArgumentOutOfRangeException& e) {
        threw = true;
        EXPECT_EQ(e.getParamNameProperty(), "millisecondsTimeOutInterval");
    }
    EXPECT_TRUE(threw);
}

// .NET runs the range check in the public overload and the two null checks in the private one
// it delegates to, so an out-of-range timeout wins over a null waitObject.
TEST(ThreadingNullArgumentTests, RegisterWaitForSingleObject_RangeCheckPrecedesNullChecks) {
    EXPECT_THROW((void)ThreadPool::RegisterWaitForSingleObject(
                     nullptr, WaitOrTimerCallback{}, nullptr, -2, true),
                 System::ArgumentOutOfRangeException);
}

TEST(ThreadingNullArgumentTests, RegisterWaitForSingleObject_ValidRegistration_StillFires) {
    Semaphore s(1, 1);
    std::atomic<int> fired{0};
    auto reg = ThreadPool::RegisterWaitForSingleObject(
        &s, [&fired](void*, bool) { fired.fetch_add(1); }, nullptr, 1000, true);
    for (int i = 0; i < 200 && fired.load() == 0; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    reg.Unregister(nullptr);
    EXPECT_GE(fired.load(), 1);
}

// SR-AUD-199. An absent shared state is now the never-cancellable token .NET's null _source
// describes, instead of a zero-page dereference on the next property read. The constructor is
// unchanged in signature and accessibility, so nothing is narrowed or removed.
TEST(ThreadingNullArgumentTests, CancellationToken_EmptyState_IsNeverCancelled) {
    CancellationToken token{std::shared_ptr<Detail::CancellationState>{}};
    EXPECT_FALSE(token.getIsCancellationRequestedProperty());
    EXPECT_NO_THROW(token.ThrowIfCancellationRequested());
}

// .NET: "Nothing to do for tokens than can never reach the canceled state. Give back a dummy
// registration."
TEST(ThreadingNullArgumentTests, CancellationToken_EmptyState_RegisterGivesInactiveRegistration) {
    CancellationToken token{std::shared_ptr<Detail::CancellationState>{}};
    std::atomic<int> ran{0};
    auto reg = token.Register([&ran] { ran.fetch_add(1); });
    EXPECT_FALSE(reg.getIsActiveProperty());
    EXPECT_NO_THROW(reg.Dispose());
    EXPECT_EQ(ran.load(), 0);
    // the empty-callable rejection from #1951 still wins over the absent state
    EXPECT_THROW((void)token.Register(std::function<void()>{}), System::ArgumentNullException);
}

// Not named by SR-AUD-199, found while reproducing it: the implicitly declared move
// constructor leaves the source's shared_ptr empty, which crashed identically. One tolerant
// definition of "no state" covers both routes.
TEST(ThreadingNullArgumentTests, CancellationToken_MovedFrom_IsNeverCancelled) {
    CancellationToken source;
    CancellationToken moved = std::move(source);

    EXPECT_FALSE(source.getIsCancellationRequestedProperty());  // NOLINT(bugprone-use-after-move)
    EXPECT_NO_THROW(source.ThrowIfCancellationRequested());     // NOLINT(bugprone-use-after-move)
    auto reg = source.Register([] {});                          // NOLINT(bugprone-use-after-move)
    EXPECT_FALSE(reg.getIsActiveProperty());

    EXPECT_FALSE(moved.getIsCancellationRequestedProperty());
}

// The tolerance must not blunt a real token: a live shared state still observes cancellation
// and still runs its callbacks.
TEST(ThreadingNullArgumentTests, CancellationToken_LiveState_Unaffected) {
    auto state = std::make_shared<Detail::CancellationState>();
    CancellationToken token{state};
    EXPECT_FALSE(token.getIsCancellationRequestedProperty());

    std::atomic<int> ran{0};
    auto reg = token.Register([&ran] { ran.fetch_add(1); });
    EXPECT_TRUE(reg.getIsActiveProperty());

    CancellationTokenSource sourceObject;
    CancellationToken sourceToken = sourceObject.getTokenProperty();
    auto sourceReg = sourceToken.Register([&ran] { ran.fetch_add(10); });
    sourceObject.Cancel();
    EXPECT_TRUE(sourceToken.getIsCancellationRequestedProperty());
    EXPECT_THROW(sourceToken.ThrowIfCancellationRequested(), System::OperationCanceledException);
    EXPECT_EQ(ran.load(), 10);
    (void)sourceReg;
}

// ===========================================================================
// #1954 / SR-AUD-184, 205 and SR-AUD-213 (timeout half), cause T-C -- out-of-domain values.
//
// SR-AUD-200 (PeriodicTimer's fractional period) was deliberately NOT repaired here. Ticket
// #1963 has since settled it against the reference tree and REFUTED the finding's conclusion;
// see the two cases at the end of this section.
// ===========================================================================

// SR-AUD-184. An undeclared EventResetMode used to produce a handle that is neither kind of
// event: Set() took the AutoReset notify_one branch because 42 is not ManualReset, while
// WaitOne() did not reset because 42 is not AutoReset.
//
// The assertion is on the exception CATEGORY, not a derived type: the audit's managed probe
// records only "an argument exception", and the reference tree is unavailable here, so the
// port throws System::ArgumentException -- the base of both .NET candidates.
TEST(ThreadingArgumentDomainTests, EventWaitHandle_UndeclaredMode_Rejected) {
    for (int raw : {2, 42, -1}) {
        bool threw = false;
        try {
            EventWaitHandle e(false, static_cast<EventResetMode>(raw));
        } catch (const System::ArgumentException& e) {
            threw = true;
            EXPECT_EQ(e.getParamNameProperty(), "mode");
        }
        EXPECT_TRUE(threw) << "raw mode " << raw << " was accepted";
    }
}

TEST(ThreadingArgumentDomainTests, EventWaitHandle_DeclaredModes_Unchanged) {
    EventWaitHandle manual(false, EventResetMode::ManualReset);
    manual.Set();
    EXPECT_TRUE(manual.WaitOne(50));
    EXPECT_TRUE(manual.WaitOne(50));  // manual reset stays signalled
    manual.Reset();
    EXPECT_FALSE(manual.WaitOne(20));

    EventWaitHandle als(false, EventResetMode::AutoReset);
    als.Set();
    EXPECT_TRUE(als.WaitOne(50));
    EXPECT_FALSE(als.WaitOne(20));  // auto reset consumed the signal

    EventWaitHandle initiallySet(true, EventResetMode::ManualReset);
    EXPECT_TRUE(initiallySet.WaitOne(50));
}

// SR-AUD-205. .NET stores a bool and derives RecursionPolicy, so an undeclared value can
// never be read back; this port stored and reflected it verbatim. Normalisation, NOT
// rejection -- the opposite of EventWaitHandle above, because .NET treats the two enums
// differently and this port must not unify them.
TEST(ThreadingArgumentDomainTests, ReaderWriterLockSlim_UndeclaredPolicy_NormalisedToNoRecursion) {
    for (int raw : {2, 7, -1}) {
        ReaderWriterLockSlim lock(static_cast<LockRecursionPolicy>(raw));
        EXPECT_EQ(lock.getRecursionPolicyProperty(), LockRecursionPolicy::NoRecursion)
            << "raw policy " << raw << " was reflected verbatim";
    }
}

TEST(ThreadingArgumentDomainTests, ReaderWriterLockSlim_DeclaredPolicies_Unchanged) {
    ReaderWriterLockSlim defaulted;
    EXPECT_EQ(defaulted.getRecursionPolicyProperty(), LockRecursionPolicy::NoRecursion);

    ReaderWriterLockSlim none(LockRecursionPolicy::NoRecursion);
    EXPECT_EQ(none.getRecursionPolicyProperty(), LockRecursionPolicy::NoRecursion);

    ReaderWriterLockSlim recursive(LockRecursionPolicy::SupportsRecursion);
    EXPECT_EQ(recursive.getRecursionPolicyProperty(), LockRecursionPolicy::SupportsRecursion);
    recursive.EnterReadLock();
    EXPECT_NO_THROW(recursive.EnterReadLock());
    recursive.ExitReadLock();
    recursive.ExitReadLock();
}

// The behavioural half was already correct before #1954 -- isReentrant() tests for
// SupportsRecursion, so an undeclared policy already locked like NoRecursion. Pinned so the
// property and the behaviour cannot drift apart again in either direction.
TEST(ThreadingArgumentDomainTests, ReaderWriterLockSlim_UndeclaredPolicy_AlsoBehavesAsNoRecursion) {
    ReaderWriterLockSlim lock(static_cast<LockRecursionPolicy>(2));
    lock.EnterReadLock();
    EXPECT_THROW(lock.EnterReadLock(), System::Exception);
    lock.ExitReadLock();
}

// SR-AUD-213, timeout half. -2 used to return false, which a caller cannot tell apart from a
// legitimate expiry.
TEST(ThreadingArgumentDomainTests, SpinWait_TimeoutBelowMinusOne_Rejected) {
    for (SharpRuntime::intcs bad : {static_cast<SharpRuntime::intcs>(-2),
                                    static_cast<SharpRuntime::intcs>(-1000)}) {
        bool threw = false;
        try {
            (void)SpinWait::SpinUntil([] { return true; }, bad);
        } catch (const System::ArgumentOutOfRangeException& e) {
            threw = true;
            EXPECT_EQ(e.getParamNameProperty(), "millisecondsTimeout");
        }
        EXPECT_TRUE(threw);
    }
}

// The ordering requirement recorded in docs/ThreadingNamespaceReviewPlan.md 17.3: .NET
// validates millisecondsTimeout before condition, so a call invalid in both ways reports the
// timeout. Between #1951 and #1954 this port reported the condition.
TEST(ThreadingArgumentDomainTests, SpinWait_TimeoutCheckPrecedesConditionCheck) {
    bool threw = false;
    try {
        (void)SpinWait::SpinUntil(std::function<bool()>{}, -2);
    } catch (const System::ArgumentOutOfRangeException& e) {
        threw = true;
        EXPECT_EQ(e.getParamNameProperty(), "millisecondsTimeout");
    }
    EXPECT_TRUE(threw);
}

TEST(ThreadingArgumentDomainTests, SpinWait_ValidTimeouts_Unchanged) {
    EXPECT_TRUE(SpinWait::SpinUntil([] { return true; }, -1));
    EXPECT_TRUE(SpinWait::SpinUntil([] { return true; }, 0));
    EXPECT_FALSE(SpinWait::SpinUntil([] { return false; }, 0));
    EXPECT_FALSE(SpinWait::SpinUntil([] { return false; }, 5));
}

// SR-AUD-200's CONCLUSION IS REFUTED, and #1963 settled it from the reference tree rather
// than from recollection. PeriodicTimer.TryGetMilliseconds (PeriodicTimer.cs:110-121) reads
//
//     long ms = (long)value.TotalMilliseconds;
//     if ((ms >= 1 && ms <= Timer.MaxSupportedTimeout) || value == Timeout.InfiniteTimeSpan)
//
// so .NET truncates a fractional period and schedules it. #1954 declined to implement the
// finding's "the input must be rejected" on exactly this reading, and the reading was right;
// rejecting would narrow AWAY from .NET. The cases below are therefore no longer a pin on an
// unendorsed status quo -- they assert .NET's own domain.
TEST(ThreadingArgumentDomainTests, PeriodicTimer_FractionalPeriodIsTruncated_NotRejected) {
    EXPECT_NO_THROW({
        PeriodicTimer t(System::TimeSpan::FromMilliseconds(1.5));
        t.Dispose();
    });
    EXPECT_NO_THROW({
        PeriodicTimer t(System::TimeSpan::FromMilliseconds(2.0));
        t.Dispose();
    });
    // A period that truncates BELOW one millisecond is out of domain in .NET too, because the
    // cast runs before the >= 1 test.
    EXPECT_THROW(PeriodicTimer(System::TimeSpan::FromMilliseconds(0.9)),
                 System::ArgumentOutOfRangeException);
    EXPECT_THROW(PeriodicTimer(System::TimeSpan::FromMilliseconds(0.0)),
                 System::ArgumentOutOfRangeException);
    EXPECT_THROW(PeriodicTimer(System::TimeSpan::FromMilliseconds(-5.0)),
                 System::ArgumentOutOfRangeException);
}

// The order of truncation and comparison is observable at the CEILING, and this is where
// #1963 changed behaviour. .NET truncates first, so a period whose fractional part alone
// pushes it past Timer.MaxSupportedTimeout is still accepted; this port compared the double
// and rejected it. Timer.MaxSupportedTimeout is 0xFFFFFFFE.
TEST(ThreadingArgumentDomainTests, PeriodicTimer_CeilingIsTestedAfterTruncation) {
    constexpr double kMax = 4294967294.0;   // 0xFFFFFFFE
    EXPECT_NO_THROW({
        PeriodicTimer t(System::TimeSpan::FromMilliseconds(kMax));
        t.Dispose();
    });
    EXPECT_NO_THROW({
        PeriodicTimer t(System::TimeSpan::FromMilliseconds(kMax + 0.5));
        t.Dispose();
    }) << "the fractional part alone must not push a period out of domain";
    EXPECT_THROW(PeriodicTimer(System::TimeSpan::FromMilliseconds(kMax + 1.0)),
                 System::ArgumentOutOfRangeException);
}

// =============================================================================================
// Ticket #1957 / SR-AUD-201 — PeriodicTimer::WaitForNextTick is single-consumer.
//
// Two concurrent waiters both returned true for ONE tick (the audit's probe measured
// `concurrent=1,1`), so a caller that accidentally shared a timer got twice the intended work
// rate with no diagnostic at all.
//
// .NET carries `private bool _activeWait` (PeriodicTimer.cs:192) and opens
// WaitForNextTickAsync by testing it and throwing (PeriodicTimer.cs:199-203), under the comment
// "WaitForNextTickAsync should only be used by one consumer at a time. Failing to do so is an
// error." The type's own summary says the same: "This timer is intended to be used only by a
// single consumer at a time" (PeriodicTimer.cs:13-14).
//
// This resolves the design record's one [unverified] flag, which asked "whether .NET throws or
// blocks the second consumer must be confirmed against the reference before landing"
// (docs/ThreadingNamespaceReviewPlan.md section 20.2 item 4). It throws.
//
// Landed under SA-3 (a private data member, sizeof pinned) + SA-5 (the behaviour is derived).
// =============================================================================================

TEST(PeriodicTimerSingleConsumerTests, Decl1957_TheGuardCostsNoLayout) {
    // SA-3's pinned measurement. The new bool fits in padding the type already had, so the size
    // is unchanged and no consumer needs a rebuild for layout -- measured 128 before and 128
    // after (build-probe/1957_probe1_layout.cpp).
    static_assert(sizeof(System::Threading::PeriodicTimer) == 128,
                  "#1957/SR-AUD-201 must not grow PeriodicTimer");
    static_assert(alignof(System::Threading::PeriodicTimer) == 8);
    EXPECT_EQ(sizeof(System::Threading::PeriodicTimer), 128u);
}

TEST(PeriodicTimerSingleConsumerTests, Fix1957_ASecondConcurrentConsumerThrows) {
    System::Threading::PeriodicTimer timer(System::TimeSpan::FromMilliseconds(3000));

    std::atomic<bool> firstIsWaiting{false};
    std::atomic<bool> secondThrew{false};
    std::atomic<bool> secondReturned{false};

    // The first consumer parks in a long wait. Detached with a bounded handshake, because a
    // regression here is a HANG rather than a wrong value, and joining a stuck thread would take
    // the whole executable down instead of failing one assertion.
    std::thread first([&] {
        firstIsWaiting.store(true);
        (void)timer.WaitForNextTick();
    });

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(4);
    while (!firstIsWaiting.load() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::yield();
    ASSERT_TRUE(firstIsWaiting.load()) << "the first consumer never started";
    // Give the first consumer time to reach the wait and publish activeWait_ under the mutex.
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    std::thread second([&] {
        try {
            (void)timer.WaitForNextTick();
            secondReturned.store(true);
        } catch (const System::InvalidOperationException&) {
            secondThrew.store(true);
        } catch (...) {
        }
    });
    second.join();

    EXPECT_TRUE(secondThrew.load())
        << "a second concurrent consumer must be refused, not silently served";
    EXPECT_FALSE(secondReturned.load())
        << "two waiters must not both consume the same tick";

    timer.Dispose();
    first.join();
}

TEST(PeriodicTimerSingleConsumerTests, Fix1957_TheFlagIsClearedSoSequentialWaitsStillWork) {
    // The half a naive guard gets wrong: if the flag is not cleared on every exit, the FIRST
    // wait locks the timer out for ever and ordinary sequential use breaks.
    System::Threading::PeriodicTimer timer(System::TimeSpan::FromMilliseconds(1));
    EXPECT_TRUE(timer.WaitForNextTick());
    EXPECT_TRUE(timer.WaitForNextTick());
    EXPECT_TRUE(timer.WaitForNextTick());
}

TEST(PeriodicTimerSingleConsumerTests, Fix1957_TheFlagIsClearedAfterADisposedReturn) {
    // The exit path that returns false rather than a tick must clear the flag too, or a disposed
    // timer would start throwing instead of returning false to subsequent callers.
    System::Threading::PeriodicTimer timer(System::TimeSpan::FromMilliseconds(1));
    timer.Dispose();
    EXPECT_FALSE(timer.WaitForNextTick());
    EXPECT_FALSE(timer.WaitForNextTick()) << "a disposed timer keeps returning false, not throwing";
}

TEST(PeriodicTimerSingleConsumerTests, Fix1957_SingleConsumerUseIsCompletelyUnchanged) {
    // The contract everybody actually uses: one consumer, ticks delivered, disposal ends it.
    System::Threading::PeriodicTimer timer(System::TimeSpan::FromMilliseconds(1));
    int ticks = 0;
    for (int i = 0; i < 5; ++i) {
        ASSERT_TRUE(timer.WaitForNextTick());
        ++ticks;
    }
    EXPECT_EQ(ticks, 5);
    timer.Dispose();
    EXPECT_FALSE(timer.WaitForNextTick());
}

// =============================================================================================
// Ticket #1957 / SR-AUD-210 — the Barrier's post-phase action can read the barrier.
//
// FinishPhase() invokes the post-phase action while HOLDING mutex_, and
// getCurrentPhaseNumberProperty() took that same non-recursive mutex, so a legal call from
// inside the action self-deadlocked. #1955 fixed the sibling property
// (getParticipantCountProperty) the same way and named this one as the remaining case.
//
// .NET's CurrentPhaseNumber is `Volatile.Read(ref _currentPhase)` (Barrier.cs:184-188) -- a
// lock-free read of a plain field -- so the reference settles the design: the property must not
// take the lock.
//
// THE SECOND HALF, which the design record did not name: .NET increments the phase in
// SetResetEvents, called from FinishPhase's `finally` AFTER the action runs
// (Barrier.cs:804-812, 834-836). This port incremented FIRST, which was unobservable only
// because the property that would have seen it deadlocked. Fixing the deadlock alone would have
// shipped a newly reachable WRONG ANSWER in place of a hang.
//
// Landed under SA-5, with SA-3's layout condition discharged as layout-neutral: sizeof(Barrier)
// is 160 before and after (build-probe/1957_probe2_barrier.cpp).
// =============================================================================================

TEST(BarrierPostPhaseReadabilityTests, Decl1957_TheAtomicPhaseIsLayoutNeutral) {
    static_assert(sizeof(System::Threading::Barrier) == 160,
                  "#1957/SR-AUD-210 must not change Barrier's layout");
    static_assert(alignof(System::Threading::Barrier) == 8);
    EXPECT_EQ(sizeof(System::Threading::Barrier), 160u);
}

TEST(BarrierPostPhaseReadabilityTests, Fix1957_ThePostPhaseActionCanReadThePhaseNumber) {
    // The deadlock itself. Before the repair this test HANGS rather than fails, which is why the
    // work runs on a detached thread behind a bounded handshake: a stuck join would take the whole
    // executable down instead of failing one assertion.
    std::atomic<bool> done{false};
    std::atomic<long long> seenInsideAction{-1};

    std::thread worker([&] {
        System::Threading::Barrier barrier(1, [&](System::Threading::Barrier& b) {
            seenInsideAction.store(b.getCurrentPhaseNumberProperty());
        });
        barrier.SignalAndWait();
        done.store(true);
    });
    worker.detach();

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(4);
    while (!done.load() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));

    ASSERT_TRUE(done.load())
        << "reading the phase number from the post-phase action deadlocked";
    EXPECT_EQ(seenInsideAction.load(), 0)
        << "inside the action the phase is the one ENDING, matching .NET";
}

TEST(BarrierPostPhaseReadabilityTests, Fix1957_ThePhaseAdvancesAfterTheActionNotBefore) {
    // The ordering half, asserted across three phases so an off-by-one cannot pass by accident.
    std::atomic<bool> done{false};
    std::vector<long long> insideAction;
    std::vector<long long> afterSignal;
    std::mutex recordMutex;

    std::thread worker([&] {
        System::Threading::Barrier barrier(1, [&](System::Threading::Barrier& b) {
            std::lock_guard<std::mutex> g(recordMutex);
            insideAction.push_back(b.getCurrentPhaseNumberProperty());
        });
        for (int i = 0; i < 3; ++i) {
            barrier.SignalAndWait();
            std::lock_guard<std::mutex> g(recordMutex);
            afterSignal.push_back(barrier.getCurrentPhaseNumberProperty());
        }
        done.store(true);
    });
    worker.detach();

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(4);
    while (!done.load() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    ASSERT_TRUE(done.load()) << "the barrier deadlocked";

    std::lock_guard<std::mutex> g(recordMutex);
    ASSERT_EQ(insideAction.size(), 3u);
    ASSERT_EQ(afterSignal.size(), 3u);
    EXPECT_EQ(insideAction, (std::vector<long long>{0, 1, 2})) << "the phase that is ending";
    EXPECT_EQ(afterSignal,  (std::vector<long long>{1, 2, 3})) << "the phase that has begun";
}

TEST(BarrierPostPhaseReadabilityTests, Fix1957_ThePhaseStillAdvancesWhenTheActionThrows) {
    // .NET increments in the `finally`, so a throwing action still advances the phase. Easy to
    // break by moving the increment into the success path only.
    System::Threading::Barrier barrier(1, [](System::Threading::Barrier&) {
        throw System::InvalidOperationException("boom");
    });
    EXPECT_EQ(barrier.getCurrentPhaseNumberProperty(), 0);
    EXPECT_THROW(barrier.SignalAndWait(), System::Threading::BarrierPostPhaseException);
    EXPECT_EQ(barrier.getCurrentPhaseNumberProperty(), 1)
        << "a throwing post-phase action must still advance the phase";
}

TEST(BarrierPostPhaseReadabilityTests, Fix1957_ThePhaseIsUnchangedForOutsideObservers) {
    // Nothing a caller outside the action can see moved: the increment is still inside the
    // critical section, before notify_all and before the lock is released.
    System::Threading::Barrier barrier(1);
    EXPECT_EQ(barrier.getCurrentPhaseNumberProperty(), 0);
    barrier.SignalAndWait();
    EXPECT_EQ(barrier.getCurrentPhaseNumberProperty(), 1);
    barrier.SignalAndWait();
    EXPECT_EQ(barrier.getCurrentPhaseNumberProperty(), 2);
}

TEST(BarrierPostPhaseReadabilityTests, Fix1957_TheOtherMembersStillRefuseReentrancy) {
    // The boundary this ticket must NOT move: the members that mutate still throw rather than
    // deadlock, because each guards before taking the lock. Only the two READ-ONLY properties are
    // callable from the action.
    std::atomic<bool> done{false};
    std::atomic<int> threwCount{0};
    std::atomic<long long> phase{-1};
    std::atomic<int> participants{-1};

    std::thread worker([&] {
        System::Threading::Barrier barrier(1, [&](System::Threading::Barrier& b) {
            phase.store(b.getCurrentPhaseNumberProperty());
            participants.store(static_cast<int>(b.getParticipantCountProperty()));
            for (auto call : std::vector<std::function<void()>>{
                     [&] { (void)b.AddParticipant(); },
                     [&] { b.RemoveParticipant(); },
                     [&] { b.SignalAndWait(); },
                     [&] { b.Dispose(); }}) {
                try { call(); } catch (const System::InvalidOperationException&) { ++threwCount; }
                catch (...) {}
            }
        });
        barrier.SignalAndWait();
        done.store(true);
    });
    worker.detach();

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(4);
    while (!done.load() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));

    ASSERT_TRUE(done.load()) << "a mutating member deadlocked instead of throwing";
    EXPECT_EQ(threwCount.load(), 4) << "all four mutating members must refuse reentrancy";
    EXPECT_EQ(phase.load(), 0);
    EXPECT_EQ(participants.load(), 1);
}

// =============================================================================================
// Ticket #1957 / SR-AUD-204 — a waiting writer blocks subsequent readers.
//
// The read-admission predicate was `!writerActive_` alone, with no waiting-writer term, so a
// steady stream of new readers entered past a writer already blocked in TryEnterWriteLock and
// could starve it INDEFINITELY.
//
// .NET keeps the same signal packed into its single `_owners` word: WaitOnEvent sets
// WAITING_WRITERS as soon as the first writer begins waiting, under the comment "Setting these
// bits will prevent new readers from getting in" (ReaderWriterLockSlim.cs:1005-1010). Both that
// bit and WAITING_UPGRADER sit above MAX_READER, so .NET's single admission test
// `_owners < MAX_READER` refuses a new reader whenever a writer holds the lock OR is waiting.
//
// Landed under SA-5, with SA-3's layout condition discharged as layout-neutral:
// sizeof(ReaderWriterLockSlim) is 120 before and after (build-probe/1957_probe3_rwls.cpp).
//
// THIS IS A FAIRNESS CHANGE: a reader-heavy workload that never blocked can now block. That is
// the point -- it is what stops the writer starving -- and it is .NET's documented behaviour.
//
// EVERY probing reader below runs on ITS OWN THREAD. The default LockRecursionPolicy is
// NoRecursion, so a second read acquisition on the thread that already holds one throws
// LockRecursionException rather than testing admission -- an earlier draft did exactly that and
// took the executable down with `terminate called without an active exception`, because the
// escaping exception left two std::threads joinable.
// =============================================================================================

namespace {
    /// Tries to take a read lock on a fresh thread and reports whether it got in.
    bool ReaderOnItsOwnThreadCanEnter(System::Threading::ReaderWriterLockSlim& lock,
                                      SharpRuntime::intcs timeoutMs) {
        std::atomic<bool> entered{false};
        std::thread probe([&] {
            if (lock.TryEnterReadLock(timeoutMs)) {
                entered.store(true);
                lock.ExitReadLock();
            }
        });
        probe.join();
        return entered.load();
    }
}

TEST(ReaderWriterWriterPreferenceTests, Decl1957_TheWaitingWriterCountIsLayoutNeutral) {
    static_assert(sizeof(System::Threading::ReaderWriterLockSlim) == 120,
                  "#1957/SR-AUD-204 must not change ReaderWriterLockSlim's layout");
    static_assert(alignof(System::Threading::ReaderWriterLockSlim) == 8);
    EXPECT_EQ(sizeof(System::Threading::ReaderWriterLockSlim), 120u);
}

TEST(ReaderWriterWriterPreferenceTests, Fix1957_ANewReaderWaitsBehindABlockedWriter) {
    // THE DEFECT. A reader holds the lock; a writer blocks behind it; a NEW reader arrives on
    // another thread. Before the repair that reader walked straight in, so an endless supply of
    // readers starved the writer.
    System::Threading::ReaderWriterLockSlim lock;
    std::atomic<bool> writerWaiting{false};
    std::atomic<bool> writerAcquired{false};

    lock.EnterReadLock();   // first reader, held by this thread

    std::thread writer([&] {
        writerWaiting.store(true);
        lock.EnterWriteLock();
        writerAcquired.store(true);
        lock.ExitWriteLock();
    });

    while (!writerWaiting.load()) std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    ASSERT_FALSE(writerAcquired.load()) << "the writer must still be blocked behind the reader";

    EXPECT_FALSE(ReaderOnItsOwnThreadCanEnter(lock, 200))
        << "a new reader entered past a waiting writer -- the starvation this ticket removes";

    lock.ExitReadLock();    // release the original reader; the writer now gets in
    writer.join();
    EXPECT_TRUE(writerAcquired.load());

    // ...and once the writer is done, readers flow again.
    EXPECT_TRUE(ReaderOnItsOwnThreadCanEnter(lock, 1000));
}

TEST(ReaderWriterWriterPreferenceTests, Fix1957_ATimedOutWriterStopsBlockingReaders) {
    // .NET clears its bit in WaitOnEvent's `finally` (ReaderWriterLockSlim.cs:1039-1042), so a
    // writer that gives up must stop holding readers back. A guard that decremented only on
    // success would wedge every future reader -- the failure mode is permanent, not transient.
    System::Threading::ReaderWriterLockSlim lock;
    lock.EnterReadLock();

    std::thread writer([&] {
        EXPECT_FALSE(lock.TryEnterWriteLock(100)) << "the writer cannot get in past the reader";
    });
    writer.join();

    EXPECT_TRUE(ReaderOnItsOwnThreadCanEnter(lock, 1000))
        << "a timed-out writer must stop blocking readers";
    lock.ExitReadLock();
}

TEST(ReaderWriterWriterPreferenceTests, Fix1957_TheUncontendedPathsAreUnchanged) {
    // Writer preference must cost nothing when no writer is waiting.
    System::Threading::ReaderWriterLockSlim lock;
    EXPECT_TRUE(lock.TryEnterReadLock(0));
    lock.ExitReadLock();
    EXPECT_TRUE(lock.TryEnterWriteLock(0));
    lock.ExitWriteLock();
    lock.EnterReadLock();
    lock.ExitReadLock();
    lock.EnterWriteLock();
    lock.ExitWriteLock();
    EXPECT_TRUE(ReaderOnItsOwnThreadCanEnter(lock, 0));
}

TEST(ReaderWriterWriterPreferenceTests, Fix1957_AnUpgraderWaitingForWriteAlsoBlocksNewReaders) {
    // .NET sets WAITING_UPGRADER for the upgrade-to-write case as well, and it too sits above
    // MAX_READER -- so BOTH kinds of writer block new readers. Counting only plain writers would
    // leave the upgrade path starvable, which is the easy half to miss.
    System::Threading::ReaderWriterLockSlim lock;
    std::atomic<bool> upgraderWaiting{false};
    std::atomic<bool> upgraded{false};

    lock.EnterReadLock();   // a reader the upgrader must wait to drain

    std::thread upgrader([&] {
        lock.EnterUpgradeableReadLock();
        upgraderWaiting.store(true);
        lock.EnterWriteLock();      // waits for readers_ == 0
        upgraded.store(true);
        lock.ExitWriteLock();
        lock.ExitUpgradeableReadLock();
    });

    while (!upgraderWaiting.load()) std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    ASSERT_FALSE(upgraded.load()) << "the upgrader must still be waiting for the reader";

    EXPECT_FALSE(ReaderOnItsOwnThreadCanEnter(lock, 200))
        << "a new reader entered past an upgrader waiting for the write lock";

    lock.ExitReadLock();
    upgrader.join();
    EXPECT_TRUE(upgraded.load());
}

// =============================================================================================
// Ticket #1956 (cause T-G) — disposal is a real state across System::Threading.
//
// Four findings, ONE guard idiom, and one member deliberately EXCLUDED from it.
//
//   SR-AUD-208  Mutex/AutoResetEvent/ManualResetEvent::Close() were EMPTY BODIES, so a closed
//               handle stayed fully usable -- Close() then WaitOne(0) returned success. The
//               headers already CLAIMED Close "closes the handle", so documentation and
//               behaviour disagreed.
//   SR-AUD-219  ThreadLocal::IsValueCreated was the one accessor on the type that did not check,
//               so a disposed instance answered `false` -- indistinguishable from "alive, no
//               value yet".
//   SR-AUD-203  ReaderWriterLockSlim::Dispose() succeeded with a lock held.
//   (T-G/timer)  ITimer::Change returned true unconditionally, so a disposed timer reported it
//               had been rescheduled.
//
// .NET: WaitHandle.Close() is `=> Dispose()` and every wait path then throws
// ObjectDisposedException (WaitHandle.cs:87-98, 118); ThreadLocal<T>.IsValueCreated opens with
// ObjectDisposedException.ThrowIf (ThreadLocal.cs:478-488); ReaderWriterLockSlim.Dispose throws
// SynchronizationLockException (ReaderWriterLockSlim.cs:1250-1258).
//
// BUT ITimer::Change must NOT throw: .NET's Timer.Change opens `if (_canceled) { return false; }`
// (Timer.cs:539-542). A false RETURN is the documented contract, and making it throw for
// symmetry would contradict the interface. The design record excluded it and the reference
// confirms the exclusion -- that asymmetry is pinned below rather than left to look like an
// oversight.
//
// Landed under SA-5 (the approval question was "may a previously-succeeding call start
// throwing?", which SA-5 grants verbatim), with SA-3's layout condition discharged: all three
// wait-handle sizes are UNCHANGED, the flags landing in padding they already had.
// =============================================================================================

TEST(DisposalIsARealStateTests, Decl1956_TheClosedFlagsAreLayoutNeutral) {
    static_assert(sizeof(System::Threading::Mutex) == 64, "#1956 must not grow Mutex");
    static_assert(sizeof(System::Threading::AutoResetEvent) == 96, "#1956 must not grow AutoResetEvent");
    static_assert(sizeof(System::Threading::ManualResetEvent) == 96, "#1956 must not grow ManualResetEvent");
    EXPECT_EQ(sizeof(System::Threading::Mutex), 64u);
    EXPECT_EQ(sizeof(System::Threading::AutoResetEvent), 96u);
    EXPECT_EQ(sizeof(System::Threading::ManualResetEvent), 96u);
}

TEST(DisposalIsARealStateTests, Fix1956_AClosedAutoResetEventRefusesEveryOperation) {
    System::Threading::AutoResetEvent e(false);
    e.Close();
    EXPECT_THROW(e.Set(), System::ObjectDisposedException);
    EXPECT_THROW(e.Reset(), System::ObjectDisposedException);
    EXPECT_THROW(e.WaitOne(), System::ObjectDisposedException);
    EXPECT_THROW((void)e.WaitOne(0), System::ObjectDisposedException);
    EXPECT_NO_THROW(e.Close()) << "Close stays idempotent, as .NET's Dispose is";
}

TEST(DisposalIsARealStateTests, Fix1956_AClosedManualResetEventRefusesEveryOperation) {
    System::Threading::ManualResetEvent e(false);
    e.Close();
    EXPECT_THROW(e.Set(), System::ObjectDisposedException);
    EXPECT_THROW(e.Reset(), System::ObjectDisposedException);
    EXPECT_THROW(e.WaitOne(), System::ObjectDisposedException);
    EXPECT_THROW((void)e.WaitOne(0), System::ObjectDisposedException);
    EXPECT_NO_THROW(e.Close());
}

TEST(DisposalIsARealStateTests, Fix1956_AClosedMutexRefusesEveryOperation) {
    // THE MEASURED DEFECT: before the repair, Close() then WaitOne(0) returned TRUE.
    System::Threading::Mutex m(false);
    m.Close();
    EXPECT_THROW((void)m.WaitOne(), System::ObjectDisposedException);
    EXPECT_THROW((void)m.WaitOne(0), System::ObjectDisposedException);
    EXPECT_THROW(m.ReleaseMutex(), System::ObjectDisposedException);
    EXPECT_NO_THROW(m.Close());
}

TEST(DisposalIsARealStateTests, Fix1956_MutexCloseReachesTheOverriddenDispose) {
    // Mutex overrides Dispose() rather than shadowing Close(), so the inherited
    // WaitHandle::Close() -- which is `{ Dispose(); }` -- reaches it. That is .NET's own
    // arrangement (`public virtual void Close() => Dispose();`). Both spellings must work, and
    // through a base reference too.
    {
        System::Threading::Mutex m(false);
        m.Dispose();
        EXPECT_THROW((void)m.WaitOne(0), System::ObjectDisposedException);
    }
    {
        System::Threading::Mutex m(false);
        System::Threading::WaitHandle& base = m;
        base.Close();
        EXPECT_THROW((void)m.WaitOne(0), System::ObjectDisposedException);
    }
}

TEST(DisposalIsARealStateTests, Fix1956_ADisposedThreadLocalRefusesIsValueCreated) {
    // It used to answer `false`, which a caller cannot tell from "alive, and no value yet".
    System::Threading::ThreadLocal<int> local(std::function<int()>([] { return 7; }));
    EXPECT_FALSE(local.getIsValueCreatedProperty());
    EXPECT_EQ(local.getValueProperty(), 7);
    EXPECT_TRUE(local.getIsValueCreatedProperty());
    local.Dispose();
    EXPECT_THROW((void)local.getIsValueCreatedProperty(), System::ObjectDisposedException);
}

TEST(DisposalIsARealStateTests, Fix1956_DisposingAHeldReaderWriterLockThrows) {
    {
        System::Threading::ReaderWriterLockSlim lock;
        lock.EnterReadLock();
        EXPECT_THROW(lock.Dispose(), System::Threading::SynchronizationLockException);
        lock.ExitReadLock();
        EXPECT_NO_THROW(lock.Dispose()) << "released, so disposal is now allowed";
    }
    {
        System::Threading::ReaderWriterLockSlim lock;
        lock.EnterWriteLock();
        EXPECT_THROW(lock.Dispose(), System::Threading::SynchronizationLockException);
        lock.ExitWriteLock();
        EXPECT_NO_THROW(lock.Dispose());
    }
    {
        System::Threading::ReaderWriterLockSlim lock;
        lock.EnterUpgradeableReadLock();
        EXPECT_THROW(lock.Dispose(), System::Threading::SynchronizationLockException);
        lock.ExitUpgradeableReadLock();
        EXPECT_NO_THROW(lock.Dispose());
    }
}

TEST(DisposalIsARealStateTests, Fix1956_AnUnheldReaderWriterLockDisposesQuietly) {
    System::Threading::ReaderWriterLockSlim lock;
    EXPECT_NO_THROW(lock.Dispose());
    EXPECT_NO_THROW(lock.Dispose()) << "disposal stays idempotent";
}

TEST(DisposalIsARealStateTests, Decl1956_ITimerChangeReturnsFalseAndDoesNotThrow) {
    // THE DELIBERATE EXCLUSION, pinned so a later "consistency" pass cannot quietly make it
    // throw. .NET: `if (_canceled) { return false; }` (Timer.cs:539-542).
    std::atomic<int> fired{0};
    auto timer = System::TimeProvider::getSystemProperty().CreateTimer(
        [&](void*) { ++fired; }, nullptr,
        System::TimeSpan::FromMilliseconds(10000), System::TimeSpan::FromMilliseconds(10000));
    ASSERT_NE(timer, nullptr);

    EXPECT_TRUE(timer->Change(System::TimeSpan::FromMilliseconds(5000),
                              System::TimeSpan::FromMilliseconds(5000)))
        << "a live timer reschedules";

    timer->Dispose();
    bool result = true;
    EXPECT_NO_THROW(result = timer->Change(System::TimeSpan::FromMilliseconds(1),
                                           System::TimeSpan::FromMilliseconds(1)))
        << "ITimer::Change must NOT throw -- that is the one member excluded from this group";
    EXPECT_FALSE(result) << "a disposed timer reports false, it does not claim success";
}
