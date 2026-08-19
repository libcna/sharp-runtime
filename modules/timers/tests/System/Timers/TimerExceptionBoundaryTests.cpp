// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Ticket #2154 / SR-AUD-238 (cause TM-A of docs/SystemTimersNamespaceReviewPlan.md): an exception
// from an Elapsed handler escaped the background std::thread entry point and called
// std::terminate, killing the process -- and with it every other timer running in that process.
//
// Measured before-matrix: build-probe/2153_probe1_before.log, 7 of 7 SIGABRT.
//
// These tests exist BECAUSE the pre-repair behaviour is a process abort: if the repair regresses,
// this executable dies rather than reporting a failure. That is the loudest possible signal and is
// the reason a plain EXPECT is enough here -- there is no "wrong value" state to assert against.
//
// Every wait is a condition variable signalled by the handler, with a timeout only as a failure
// backstop. No assertion in this file depends on a sleep.

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <string>

#include "System/ArgumentException.hpp"
#include "System/Timers/Timer.hpp"

using namespace System::Timers;

namespace {

    // A condition-variable latch: the handler signals, the test waits. Deterministic in the sense
    // that matters -- the test never proceeds before the handler has actually run.
    class FireLatch {
        std::mutex mtx_;
        std::condition_variable cv_;
        int count_ = 0;

    public:
        void signal() {
            {
                std::lock_guard<std::mutex> lk(mtx_);
                ++count_;
            }
            cv_.notify_all();
        }
        bool waitFor(int n, int timeoutMs = 5000) {
            std::unique_lock<std::mutex> lk(mtx_);
            return cv_.wait_for(lk, std::chrono::milliseconds(timeoutMs),
                                [&] { return count_ >= n; });
        }
        int count() {
            std::lock_guard<std::mutex> lk(mtx_);
            return count_;
        }
    };

    enum class ThrowKind { Std, SharpRuntime, NonStd };

    void throwOne(ThrowKind kind) {
        switch (kind) {
            case ThrowKind::Std:          throw std::runtime_error("from the Elapsed handler");
            case ThrowKind::SharpRuntime: throw System::ArgumentException("from the Elapsed handler", "x");
            case ThrowKind::NonStd:       throw 42;
        }
    }

    // One case of the SR-AUD-238 matrix: throw after `throwAfter` successful fires, with the given
    // AutoReset value and exception type. Reaching the end of this function at all is the
    // assertion -- before #2154 the process was already gone.
    void runThrowingHandler(ThrowKind kind, bool autoReset, int throwAfter, const char* label) {
        // A SENTINEL timer, started first and never touched by the throwing case, is what makes
        // each case a real discriminator. Simply reaching the end of the test is NOT enough: the
        // exception unwinds on a background thread, so a test that stops waiting the moment the
        // handler signals can finish before the abort lands and pass against a broken boundary.
        // Measured -- a mutation narrowing the production catch to `catch (const std::exception&)`
        // SURVIVED the first version of this file for exactly that reason. Waiting for the sentinel
        // to produce further fires AFTER the throw both gives the abort time to happen and proves
        // the process is still running when it has not.
        FireLatch sentinelLatch;
        Timer sentinel(5);
        sentinel.Elapsed += [&](System::Object*, const ElapsedEventArgs&) { sentinelLatch.signal(); };
        sentinel.Start();
        ASSERT_TRUE(sentinelLatch.waitFor(1)) << label << ": the sentinel never started";

        FireLatch latch;
        std::atomic<int> fires{0};
        {
            Timer timer(5);
            timer.setAutoResetProperty(autoReset);
            timer.Elapsed += [&](System::Object*, const ElapsedEventArgs&) {
                const int k = fires.fetch_add(1);
                latch.signal();
                if (k >= throwAfter) throwOne(kind);
            };
            timer.Start();
            EXPECT_TRUE(latch.waitFor(throwAfter + 1)) << label << ": the handler never reached the throw";

            const int sentinelAtThrow = sentinelLatch.count();
            EXPECT_TRUE(sentinelLatch.waitFor(sentinelAtThrow + 5))
                << label << ": the process did not survive long enough for the sentinel to fire again";
            timer.Stop();
        }
        sentinel.Stop();
        SUCCEED() << label << ": the process survived a throwing handler";
    }

} // namespace

TEST(TimerExceptionBoundaryTests, AStdExceptionFromAOneShotHandlerDoesNotKillTheProcess) {
    runThrowingHandler(ThrowKind::Std, /*autoReset=*/false, 0, "std/one-shot");
}

TEST(TimerExceptionBoundaryTests, AStdExceptionFromAPeriodicHandlerDoesNotKillTheProcess) {
    runThrowingHandler(ThrowKind::Std, /*autoReset=*/true, 0, "std/periodic");
}

TEST(TimerExceptionBoundaryTests, AThrowAfterSeveralSuccessfulFiresDoesNotKillTheProcess) {
    runThrowingHandler(ThrowKind::Std, /*autoReset=*/true, 3, "std/periodic/after-3");
}

TEST(TimerExceptionBoundaryTests, ASharpRuntimeExceptionFromAHandlerDoesNotKillTheProcess) {
    runThrowingHandler(ThrowKind::SharpRuntime, /*autoReset=*/false, 0, "sharp-runtime/one-shot");
}

TEST(TimerExceptionBoundaryTests, ANonStdExceptionFromAHandlerDoesNotKillTheProcess) {
    // `throw 42` is the case a `catch (const std::exception&)` boundary would have missed, which is
    // why the production catch is `catch (...)`.
    runThrowingHandler(ThrowKind::NonStd, /*autoReset=*/false, 0, "non-std/one-shot");
}

TEST(TimerExceptionBoundaryTests, APeriodicTimerKeepsFiringAfterItsHandlerThrows) {
    // The behavioural half of the repair, and the reason it is not just "swallow and stop": .NET's
    // timer keeps running. A handler that throws every time therefore loops silently, which the
    // header documents so a subscriber is not surprised by it.
    FireLatch latch;
    std::atomic<int> fires{0};
    Timer timer(5);
    timer.Elapsed += [&](System::Object*, const ElapsedEventArgs&) {
        fires.fetch_add(1);
        latch.signal();
        throw std::runtime_error("every time");
    };
    timer.Start();
    EXPECT_TRUE(latch.waitFor(3)) << "the timer stopped after the first throwing invocation";
    timer.Stop();
    EXPECT_GE(fires.load(), 3);
}

TEST(TimerExceptionBoundaryTests, AnUnrelatedTimerSurvivesAnotherTimersThrowingHandler) {
    // The sentence the finding does not contain: before #2154 this killed BOTH timers, because the
    // failure was process death rather than thread death.
    FireLatch victimLatch;
    std::atomic<int> victimFires{0};
    Timer victim(5);
    victim.Elapsed += [&](System::Object*, const ElapsedEventArgs&) {
        victimFires.fetch_add(1);
        victimLatch.signal();
    };
    victim.Start();

    FireLatch throwerLatch;
    Timer thrower(5);
    thrower.setAutoResetProperty(false);
    thrower.Elapsed += [&](System::Object*, const ElapsedEventArgs&) {
        throwerLatch.signal();
        throw std::runtime_error("boom");
    };
    thrower.Start();

    EXPECT_TRUE(throwerLatch.waitFor(1));
    EXPECT_TRUE(victimLatch.waitFor(3)) << "the unrelated timer stopped firing";
    thrower.Stop();
    victim.Stop();
    EXPECT_GE(victimFires.load(), 3);
}

TEST(TimerExceptionBoundaryTests, ASecondSubscribedHandlerIsNotReachedWhenTheFirstThrows_Pinned) {
    // EventHandler::Raise invokes a snapshot of the handler list in order. The production catch is
    // OUTSIDE that loop, so a throwing handler still aborts the remaining handlers for THAT
    // invocation -- matching a C# multicast delegate, where an exception in one target stops the
    // rest of the invocation list too. The next tick reaches both again, which is what this pins.
    FireLatch latch;
    std::atomic<int> secondHandlerRuns{0};
    Timer timer(5);
    timer.Elapsed += [&](System::Object*, const ElapsedEventArgs&) {
        throw std::runtime_error("first handler");
    };
    timer.Elapsed += [&](System::Object*, const ElapsedEventArgs&) {
        secondHandlerRuns.fetch_add(1);
        latch.signal();
    };
    timer.Start();
    const bool reached = latch.waitFor(1, 1000);
    timer.Stop();
    // Documented, not asserted as desirable: the second handler is NOT reached, because the first
    // one's exception unwinds out of Raise's loop. Pinned so a future change to Raise is deliberate.
    EXPECT_FALSE(reached) << "Raise now continues past a throwing handler -- deliberate?";
    EXPECT_EQ(secondHandlerRuns.load(), 0);
}

TEST(TimerExceptionBoundaryTests, CloseAfterAFailedCallbackStillWorks) {
    FireLatch latch;
    Timer timer(5);
    timer.setAutoResetProperty(false);
    timer.Elapsed += [&](System::Object*, const ElapsedEventArgs&) {
        latch.signal();
        throw std::runtime_error("boom");
    };
    timer.Start();
    EXPECT_TRUE(latch.waitFor(1));
    EXPECT_NO_THROW(timer.Close());
    EXPECT_NO_THROW(timer.Close());
    EXPECT_FALSE(timer.getEnabledProperty());
}

// ---------------------------------------------------------------------------
// PINS — what this ticket deliberately did NOT change
// ---------------------------------------------------------------------------

TEST(TimerExceptionBoundaryPinTests, Fix2155_ElapsedReportsTheRaisingTimerAsItsSender) {
    // INVERTED BY #2155 (2026-08-19). Its predecessor asserted a nullptr sender and said in terms
    // "#2155 landed without updating its pin" -- the approval was granted per action, so the pin
    // now records what was bought.
    //
    // .NET passes the timer too: intervalElapsed(this, elapsedEventArgs) (Timer.cs:313). The port
    // reported nullptr for a purely STRUCTURAL reason -- EventHandler<T>::Raise types its sender as
    // System::Object* and Timer had no such base, so nullptr was the only value that compiled.
    static_assert(std::is_convertible_v<Timer*, System::Object*>,
                  "#2155: Timer now derives from System::Object");
    static_assert(std::is_polymorphic_v<Timer>, "#2155: and is therefore polymorphic");

    FireLatch latch;
    System::Object* seen = reinterpret_cast<System::Object*>(1);
    Timer timer(5);
    timer.setAutoResetProperty(false);
    timer.Elapsed += [&](System::Object* sender, const ElapsedEventArgs&) {
        seen = sender;
        latch.signal();
    };
    timer.Start();
    ASSERT_TRUE(latch.waitFor(1));
    timer.Stop();

    // Not merely non-null -- it is THIS timer. A mutation passing some other Object* would satisfy
    // a null check and fail this one.
    EXPECT_EQ(seen, static_cast<System::Object*>(&timer));
}

TEST(TimerExceptionBoundaryPinTests, Fix2155_TheLayoutCostIsExactlyOneVptr) {
    // The price the approval paid for, pinned so it cannot grow unnoticed: sizeof 104 -> 112 and a
    // new vtable. Measured in build-probe before and after.
    //
    // Asserted as a RELATIONSHIP as well as a literal, so a platform with a different pointer width
    // keeps the pin meaningful and a SECOND base (or a stored member) still breaks it.
    static_assert(sizeof(Timer) == 112, "#2155: 104 + one vptr");
    static_assert(sizeof(Timer) == 104 + sizeof(void*), "#2155: the growth is exactly one vptr");
    static_assert(alignof(Timer) == 8);

    // The vptr is the FIRST subobject: an Object* obtained from a Timer* points at the same
    // address. Pinned because it is the half sizeof cannot express.
    Timer timer(5);
    EXPECT_EQ(reinterpret_cast<const char*>(static_cast<System::Object*>(&timer)),
              reinterpret_cast<const char*>(&timer));

    // And the base's pure virtual really is satisfied by this type rather than by a sibling.
    EXPECT_EQ(timer.GetTypeName(), "System.Timers.Timer");
}

TEST(TimerExceptionBoundaryPinTests, Decl2155_TheDivergenceIsInTheBaseNotTheSender) {
    // .NET's Timer derives from Component (Timer.cs:15), NOT from Object directly. This port has
    // NO ComponentModel Component class at all, so the Object base is the only available route.
    // Pinned as a DECLARATION so that a future Component, if one is ever added, is a deliberate
    // re-basing rather than a silent one -- and so the divergence is recorded where it is, in the
    // BASE, not in the observable sender, which now matches .NET exactly.
    static_assert(std::is_base_of_v<System::Object, Timer>);
    SUCCEED();
}

// The layer below -- System::Threading::Timer -- deliberately does NOT catch its own callback
// either, matching .NET's System.Threading.Timer, where an unhandled callback exception on a
// thread-pool thread terminates the process. That is measured OUT OF PROCESS, in
// build-probe/2153_probe1_before.log's last SR-AUD-238 row, and is not asserted here for two
// reasons: an in-process assertion is impossible by construction (the abort would take this
// executable with it), and `Threading` is a PRIVATE dependency of this component, so pulling it
// into the test target would add a component-graph edge for a test that cannot assert anything.
