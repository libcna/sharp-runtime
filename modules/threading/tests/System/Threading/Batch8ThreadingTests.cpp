// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <type_traits>
#include <atomic>
#include <stdexcept>
#include "System/Threading/ApartmentState.hpp"
#include "System/Threading/Thread.hpp"
#include "System/Threading/ThreadAbortException.hpp"
#include "System/Threading/ThreadExceptionEventArgs.hpp"
#include "System/Threading/ThreadInterruptedException.hpp"
#include "System/Threading/ThreadPriority.hpp"
#include "System/Threading/ThreadStart.hpp"
#include "System/Threading/ThreadStartException.hpp"
#include "System/Threading/ThreadState.hpp"
#include "System/Threading/ThreadStateException.hpp"

using namespace System::Threading;

// ===========================================================================
// ApartmentState — uncovered values
// ===========================================================================

TEST(ApartmentStateTests, BelowNormal_STA_MTA_Distinct) {
    EXPECT_NE(ApartmentState::STA, ApartmentState::MTA);
    EXPECT_NE(ApartmentState::MTA, ApartmentState::Unknown);
}

// ===========================================================================
// ThreadPriority — uncovered values
// ===========================================================================

TEST(ThreadPriorityTests, BelowNormal_IsOne) {
    EXPECT_EQ(static_cast<int>(ThreadPriority::BelowNormal), 1);
}

TEST(ThreadPriorityTests, AboveNormal_IsThree) {
    EXPECT_EQ(static_cast<int>(ThreadPriority::AboveNormal), 3);
}

TEST(ThreadPriorityTests, ValuesAreOrdered) {
    EXPECT_LT(static_cast<int>(ThreadPriority::Lowest),
              static_cast<int>(ThreadPriority::Normal));
    EXPECT_LT(static_cast<int>(ThreadPriority::Normal),
              static_cast<int>(ThreadPriority::Highest));
}

// ===========================================================================
// ThreadState — uncovered values and flags
// ===========================================================================

TEST(ThreadStateTests, Background_IsFour) {
    EXPECT_EQ(static_cast<int>(ThreadState::Background), 4);
}

TEST(ThreadStateTests, WaitSleepJoin_Is32) {
    EXPECT_EQ(static_cast<int>(ThreadState::WaitSleepJoin), 32);
}

TEST(ThreadStateTests, Suspended_Is64) {
    EXPECT_EQ(static_cast<int>(ThreadState::Suspended), 64);
}

TEST(ThreadStateTests, AbortRequested_Is128) {
    EXPECT_EQ(static_cast<int>(ThreadState::AbortRequested), 128);
}

TEST(ThreadStateTests, Aborted_Is256) {
    EXPECT_EQ(static_cast<int>(ThreadState::Aborted), 256);
}

TEST(ThreadStateTests, AndOperator) {
    auto combined = ThreadState::Background | ThreadState::Unstarted;
    auto masked   = combined & ThreadState::Background;
    EXPECT_EQ(masked, ThreadState::Background);
}

// ===========================================================================
// Thread — new API
// ===========================================================================

TEST(ThreadingTests, Thread_Priority_DefaultIsNormal) {
    Thread t([](){});
    EXPECT_EQ(t.getPriorityProperty(), ThreadPriority::Normal);
}

TEST(ThreadingTests, Thread_Priority_RoundTrip) {
    Thread t([](){});
    t.setPriorityProperty(ThreadPriority::Highest);
    EXPECT_EQ(t.getPriorityProperty(), ThreadPriority::Highest);
}

TEST(ThreadingTests, Thread_IsThreadPoolThread_FalseForUserThread) {
    Thread t([](){});
    EXPECT_FALSE(t.getIsThreadPoolThreadProperty());
}

TEST(ThreadingTests, Thread_ThreadState_UnstartedBeforeStart) {
    Thread t([](){});
    EXPECT_EQ(t.getThreadStateProperty(), ThreadState::Unstarted);
}

// SR-AUD-195 (ticket #1949): this case previously accepted
//   state == ThreadState::Running || (int(state) & int(ThreadState::Running)) == 0
// and ThreadState::Running is 0, so the second disjunct is `state & 0 == 0` -- true for
// Unstarted, Stopped and every other value. The case could not fail. Running being the zero
// value means "contains Running" is not expressible as a mask test at all; the assertion that
// carries meaning is the exact state a started, still-executing thread must report, which is
// Running for a foreground thread and Running|Background for a background one.
TEST(ThreadingTests, Thread_ThreadState_RunningAfterStart) {
    std::atomic<bool> running{false};
    std::atomic<bool> done{false};
    Thread t([&](){ running.store(true); while (!done.load()) {} });
    t.Start();
    while (!running.load()) {}
    auto state = t.getThreadStateProperty();
    const bool background = t.getIsBackgroundProperty();
    EXPECT_EQ(state, background ? (ThreadState::Running | ThreadState::Background)
                                : ThreadState::Running);
    EXPECT_NE(state, ThreadState::Unstarted);
    EXPECT_NE(state, ThreadState::Stopped);
    done.store(true);
    t.Join();
}

TEST(ThreadingTests, Thread_ThreadState_StoppedAfterJoin) {
    Thread t([](){});
    t.Start();
    t.Join();
    EXPECT_EQ(t.getThreadStateProperty(), ThreadState::Stopped);
}

TEST(ThreadingTests, Thread_GetApartmentState_ReturnsUnknown) {
    Thread t([](){});
    EXPECT_EQ(t.GetApartmentState(), ApartmentState::Unknown);
}

TEST(ThreadingTests, Thread_TrySetApartmentState_ReturnsFalse) {
    Thread t([](){});
    EXPECT_FALSE(t.TrySetApartmentState(ApartmentState::MTA));
}

TEST(ThreadingTests, Thread_Interrupt_DoesNotThrow) {
    Thread t([](){});
    EXPECT_NO_THROW(t.Interrupt());
}

TEST(ThreadingTests, Thread_SpinWait_DoesNotThrow) {
    EXPECT_NO_THROW(Thread::SpinWait(10));
}

TEST(ThreadingTests, Thread_MemoryBarrier_DoesNotThrow) {
    EXPECT_NO_THROW(Thread::MemoryBarrier());
}

TEST(ThreadingTests, Thread_Yield_ReturnsTrue) {
    EXPECT_TRUE(Thread::Yield());
}

TEST(ThreadingTests, Thread_GetCurrentProcessorId_NonNegative) {
    EXPECT_GE(Thread::GetCurrentProcessorId(), 0);
}

TEST(ThreadingTests, Thread_Join_WithTimeout_ReturnsTrueOnCompletion) {
    Thread t([](){});
    t.Start();
    bool result = t.Join(5000);
    EXPECT_TRUE(result);
}

// Regression test for a wave-3 audit finding: Start()'s spawned-thread lambda captured raw
// `this`, and ~Thread() detaches (not joins) -- destroying the Thread wrapper before its OS
// thread finishes left the lambda writing into freed memory. Confirmed via a standalone
// AddressSanitizer repro against the pre-fix code (reliably crashed with a heap-use-after-free
// write inside Thread::Start()'s lambda). Fixed by moving finished_/isBackground_/
// managedThreadId_ into a heap-allocated RunState captured by shared_ptr instead of raw `this`.
TEST(ThreadingTests, Thread_DestroyedWhileRunning_DoesNotCrash) {
    std::atomic<bool> started{false};
    std::atomic<bool> completed{false};
    {
        auto t = std::make_unique<Thread>([&]{
            started.store(true);
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            completed.store(true);
        });
        t->Start();
        while (!started.load()) std::this_thread::yield();
        // t destructs here (detach, not join) while the spawned OS thread is still sleeping.
    }
    // Give the detached thread time to finish; a crash/UB would most likely surface here.
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    EXPECT_TRUE(completed.load());
}

// Regression test: CurrentThreadProxy must still resolve the correct ManagedThreadId/
// IsBackground even when the owning Thread object has already been destroyed before (or
// while) the spawned thread runs its body -- exercising the RunState indirection directly.
TEST(ThreadingTests, CurrentThread_ResolvesCorrectly_AfterOwningThreadObjectDestroyed) {
    std::atomic<intcs> observedId{-1};
    std::atomic<bool> observedBackground{false};
    std::atomic<bool> done{false};
    intcs expectedId;
    {
        auto t = std::make_unique<Thread>([&]{
            observedId.store(Thread::CurrentThread().getManagedThreadIdProperty());
            observedBackground.store(Thread::CurrentThread().getIsBackgroundProperty());
            done.store(true);
        });
        t->setIsBackgroundProperty(true);
        expectedId = t->getManagedThreadIdProperty();
        t->Start();
        // t destructs here (detach) -- possibly before the spawned thread runs its body at all.
    }
    for (int i = 0; i < 500 && !done.load(); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(2));
    ASSERT_TRUE(done.load());
    EXPECT_EQ(observedId.load(), expectedId);
    EXPECT_TRUE(observedBackground.load());
}

// ===========================================================================
// ThreadAbortException
// ===========================================================================

TEST(ThreadAbortExceptionTests, DefaultCtor_WhatNotEmpty_New) {
    ThreadAbortException ex;
    EXPECT_FALSE(std::string(ex.what()).empty());
}

TEST(ThreadAbortExceptionTests, MessageCtor_WhatContainsMessage) {
    ThreadAbortException ex("abort reason");
    EXPECT_NE(std::string(ex.what()).find("abort reason"), std::string::npos);
}

TEST(ThreadAbortExceptionTests, IsA_SystemException) {
    ThreadAbortException ex;
    EXPECT_NE(dynamic_cast<System::SystemException*>(&ex), nullptr);
}

TEST(ThreadAbortExceptionTests, ExceptionState_IsAlwaysNull) {
    ThreadAbortException ex;
    EXPECT_EQ(ex.getExceptionStateProperty(), nullptr);
}

// ===========================================================================
// ThreadInterruptedException
// ===========================================================================

TEST(ThreadInterruptedExceptionTests, DefaultCtor_WhatNotEmpty_New) {
    ThreadInterruptedException ex;
    EXPECT_FALSE(std::string(ex.what()).empty());
}

TEST(ThreadInterruptedExceptionTests, MessageCtor_WhatContainsMessage) {
    ThreadInterruptedException ex("interrupted");
    EXPECT_NE(std::string(ex.what()).find("interrupted"), std::string::npos);
}

TEST(ThreadInterruptedExceptionTests, InnerExceptionCtor_WhatContainsBoth) {
    auto inner = std::make_exception_ptr(std::runtime_error("inner"));
    ThreadInterruptedException ex("outer", inner);
    std::string w = ex.what();
    EXPECT_NE(w.find("outer"), std::string::npos);
}

// ===========================================================================
// ThreadStartException
// ===========================================================================

TEST(ThreadStartExceptionTests, DefaultCtor_WhatNotEmpty) {
    ThreadStartException ex;
    EXPECT_FALSE(std::string(ex.what()).empty());
}

// #1958/SR-AUD-196: the two message-taking constructors are GONE. .NET's type has exactly two
// constructors and both pass the fixed SR.Arg_ThreadStartException, "Thread failed to start."
// (ThreadStartException.cs:13-24) -- it has no message-taking constructor at all, so
// ThreadStartException("anything") produced an exception .NET can never produce while still
// claiming COR_E_THREADSTART. The two cases that asserted a caller-supplied message are replaced
// by these, which assert the fixed message and the surviving reason-taking constructor.

TEST(ThreadStartExceptionTests, Fix1958_TheMessageIsFixedAndNotSuppliable) {
    ThreadStartException ex;
    EXPECT_EQ(std::string(ex.what()), "Thread failed to start.");
    static_assert(!std::is_constructible_v<ThreadStartException, std::string>,
                  "#1958/SR-AUD-196: .NET has no message-taking ThreadStartException constructor");
    static_assert(!std::is_constructible_v<ThreadStartException, const char*>,
                  "#1958/SR-AUD-196: nor one reachable through a string literal");
    static_assert(!std::is_constructible_v<ThreadStartException, std::string, std::exception_ptr>,
                  "#1958/SR-AUD-196: nor a (message, inner) pair");
}

TEST(ThreadStartExceptionTests, Fix1958_TheReasonCtorKeepsTheFixedMessage) {
    // .NET's `internal ThreadStartException(Exception? reason)` takes the REASON ALONE and still
    // uses the fixed message -- which is exactly why it replaces the old (message, inner) pair
    // rather than sitting beside it.
    auto inner = std::make_exception_ptr(std::runtime_error("cause"));
    ThreadStartException ex(inner);
    EXPECT_EQ(std::string(ex.what()), "Thread failed to start.");
    EXPECT_NE(ex.getInnerExceptionProperty(), nullptr) << "the reason is retained";
}

TEST(ThreadStartExceptionTests, Decl2390_TheConstructorsAreDeliberatelyPublic) {
    // #2390 / docs/StandingApprovals.md SA-12. .NET makes both constructors `internal`. This port
    // keeps them PUBLIC, deliberately, because SA-12's rule is conditional on whether a creator
    // exists here -- and for this type none does: .NET's runtime throws it when a managed thread
    // fails after the OS thread starts but before user code runs, a window std::thread does not
    // have (it either constructs or throws std::system_error).
    //
    // This pin is the DECLARATION. If a future ticket makes them private, this fails and the
    // change is a deliberate act rather than a drift.
    static_assert(std::is_default_constructible_v<ThreadStartException>,
                  "#2390/SA-12: the default constructor stays public -- no creator exists here");
    static_assert(std::is_constructible_v<ThreadStartException, std::exception_ptr>,
                  "#2390/SA-12: the reason constructor stays public for the same reason");

    // The divergence is in ACCESSIBILITY ALONE. Everything a caller can observe is .NET's, which
    // is what makes the divergence tolerable -- assert that here so the two halves cannot drift
    // apart: a future change that widened the surface would have to fail one of these.
    ThreadStartException ex;
    EXPECT_STREQ("Thread failed to start.", ex.what());
    EXPECT_EQ(static_cast<SharpRuntime::intcs>(0x80131525u), ex.getHResultProperty());
    static_assert(std::is_final_v<ThreadStartException>);
    static_assert(!std::is_constructible_v<ThreadStartException, std::string>,
                  "#2390: 'public' must not be read as 'anything goes' -- the shape is still .NET's");
}

TEST(ThreadStartExceptionTests, Decl1958_TheTypeIsSealed) {
    // .NET: `public sealed class ThreadStartException`.
    static_assert(std::is_final_v<ThreadStartException>,
                  "#1958/SR-AUD-196: .NET's ThreadStartException is sealed");
}

TEST(ThreadStartExceptionTests, IsA_SystemException) {
    ThreadStartException ex;
    EXPECT_NE(dynamic_cast<System::SystemException*>(&ex), nullptr);
}

// ===========================================================================
// ThreadStateException
// ===========================================================================

TEST(ThreadStateExceptionTests, DefaultCtor_WhatNotEmpty_New) {
    ThreadStateException ex;
    EXPECT_FALSE(std::string(ex.what()).empty());
}

TEST(ThreadStateExceptionTests, MessageCtor_WhatContainsMessage) {
    ThreadStateException ex("bad state");
    EXPECT_NE(std::string(ex.what()).find("bad state"), std::string::npos);
}

TEST(ThreadStateExceptionTests, InnerExceptionCtor_WhatContainsBoth) {
    auto inner = std::make_exception_ptr(std::runtime_error("root cause"));
    ThreadStateException ex("outer", inner);
    std::string w = ex.what();
    EXPECT_NE(w.find("outer"), std::string::npos);
}

// ===========================================================================
// ThreadExceptionEventHandler delegate
// ===========================================================================

TEST(ThreadExceptionEventHandlerTests, Callable_WithEventArgs) {
    bool called = false;
    ThreadExceptionEventArgs args(std::make_exception_ptr(std::runtime_error("test")));
    ThreadExceptionEventHandler handler = [&](void*, ThreadExceptionEventArgs& e) {
        called = true;
        EXPECT_NE(e.getExceptionProperty(), nullptr);
    };
    handler(nullptr, args);
    EXPECT_TRUE(called);
}

TEST(ThreadExceptionEventHandlerTests, CanBeReassigned) {
    int count = 0;
    ThreadExceptionEventArgs args(std::make_exception_ptr(std::runtime_error("x")));
    ThreadExceptionEventHandler h = [&](void*, ThreadExceptionEventArgs&) { count = 1; };
    h = [&](void*, ThreadExceptionEventArgs&) { count = 2; };
    h(nullptr, args);
    EXPECT_EQ(count, 2);
}
