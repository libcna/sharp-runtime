// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Ticket #2157 created the original lifecycle matrix. The final audit follow-up extended it with
// a deterministic in-flight-destruction handshake after the documented raw-this callback hazard
// was removed.
//
// The lifecycle and concurrency matrix of the plan's §9 was measured by
// build-probe/2153_probe1_before.log and survived in every case. A probe log is not a regression
// test, so the cases that matter are made permanent here: each of them is a way a future change to
// `Close()`, `setEnabledProperty` or the callback lambda could deadlock, use-after-free, or
// silently stop firing, and none of them was covered by the module's original nine tests.
//
// Every wait is a condition variable signalled by the handler. No assertion here depends on a
// sleep.

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <type_traits>

#include "System/Timers/Timer.hpp"

using namespace System::Timers;

namespace SharpRuntime::Testing {

template <>
struct TimerStartAccess<System::Timers::Timer> {
    static void setBeforeArmHook(void (*hook)()) {
        System::Timers::Timer::beforeArmTestHook_.store(hook);
    }
};

} // namespace SharpRuntime::Testing

namespace {

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
            return cv_.wait_for(lk, std::chrono::milliseconds(timeoutMs), [&] { return count_ >= n; });
        }
        int count() {
            std::lock_guard<std::mutex> lk(mtx_);
            return count_;
        }
    };

    class CallbackBlocker {
        std::mutex mutex_;
        std::condition_variable condition_;
        bool entered_ = false;
        bool released_ = false;

    public:
        void enterAndWait() {
            std::unique_lock<std::mutex> lock(mutex_);
            entered_ = true;
            condition_.notify_all();
            condition_.wait(lock, [&] { return released_; });
        }

        bool waitUntilEntered(int timeoutMs = 5000) {
            std::unique_lock<std::mutex> lock(mutex_);
            return condition_.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                                       [&] { return entered_; });
        }

        void release() {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                released_ = true;
            }
            condition_.notify_all();
        }
    };

    CallbackBlocker* startArmBlocker = nullptr;

    void blockBeforeTimerIsArmed() {
        startArmBlocker->enterAndWait();
    }

    void blockThenThrowBeforeTimerIsArmed() {
        startArmBlocker->enterAndWait();
        throw std::runtime_error("injected pre-arm failure");
    }

    class ScopedStartArmHook final {
    public:
        explicit ScopedStartArmHook(CallbackBlocker& blocker) {
            startArmBlocker = &blocker;
            SharpRuntime::Testing::TimerStartAccess<Timer>::setBeforeArmHook(
                &blockBeforeTimerIsArmed);
        }

        ~ScopedStartArmHook() {
            SharpRuntime::Testing::TimerStartAccess<Timer>::setBeforeArmHook(nullptr);
            startArmBlocker = nullptr;
        }

        ScopedStartArmHook(const ScopedStartArmHook&) = delete;
        ScopedStartArmHook& operator=(const ScopedStartArmHook&) = delete;
    };

    class ScopedThrowingStartArmHook final {
    public:
        explicit ScopedThrowingStartArmHook(CallbackBlocker& blocker) {
            startArmBlocker = &blocker;
            SharpRuntime::Testing::TimerStartAccess<Timer>::setBeforeArmHook(
                &blockThenThrowBeforeTimerIsArmed);
        }

        ~ScopedThrowingStartArmHook() {
            SharpRuntime::Testing::TimerStartAccess<Timer>::setBeforeArmHook(nullptr);
            startArmBlocker = nullptr;
        }

        ScopedThrowingStartArmHook(const ScopedThrowingStartArmHook&) = delete;
        ScopedThrowingStartArmHook& operator=(const ScopedThrowingStartArmHook&) = delete;
    };

} // namespace

TEST(TimerLifecyclePinTests, CloseFromInsideTheHandlerDoesNotDeadlockOrCrash) {
    // Close invalidates the shared callback generation and releases the underlying
    // System::Threading::Timer without waiting for this handler to wait for itself. Signal only
    // AFTER Close returns, so the latch proves the self-close completed rather than merely entered.
    FireLatch latch;
    Timer timer(5);
    timer.Elapsed += [&](System::Object*, const ElapsedEventArgs&) {
        timer.Close();
        latch.signal();
    };
    timer.Start();
    ASSERT_TRUE(latch.waitFor(1));
    EXPECT_FALSE(timer.getEnabledProperty());
    EXPECT_NO_THROW(timer.Close());
}

TEST(TimerLifecyclePinTests, WorkerIsStoredPausedBeforeAHandlerCanCloseIt) {
    // Hold Start at the exact boundary after its local paused worker exists but before the gate
    // transfers it into timer_ and Change arms it. The former implementation passed the live 1 ms
    // due time to the worker constructor, so the handler could call Close while
    // make_unique/unique_ptr assignment was still in flight.
    CallbackBlocker beforeArm;
    ScopedStartArmHook hook(beforeArm);
    FireLatch elapsed;
    FireLatch startReturned;
    Timer timer(1);
    timer.Elapsed += [&](System::Object*, const ElapsedEventArgs&) {
        timer.Close();
        elapsed.signal();
    };

    std::thread starter([&] {
        timer.Start();
        startReturned.signal();
    });
    const bool reachedBeforeArm = beforeArm.waitUntilEntered();
    if (!reachedBeforeArm) {
        beforeArm.release();
        starter.join();
        FAIL() << "Start never reached the deterministic pre-arm seam";
        return;
    }
    EXPECT_FALSE(elapsed.waitFor(1, 50))
        << "a callback entered before the paused worker was safely stored and armed";
    EXPECT_FALSE(startReturned.waitFor(1, 0));

    beforeArm.release();
    EXPECT_TRUE(startReturned.waitFor(1));
    EXPECT_TRUE(elapsed.waitFor(1));
    starter.join();
    EXPECT_FALSE(timer.getEnabledProperty());
}

TEST(TimerLifecyclePinTests, CloseBeforeThePausedWorkerIsArmedCancelsStart) {
    // This is the other side of the pre-arm boundary: Close and Start may meet while Start owns a
    // local paused worker. Both the generation decision and every timer_ transfer are serialized
    // by the lifetime gate, so Start must discard its local worker instead of publishing it after
    // Close has returned.
    CallbackBlocker beforeArm;
    ScopedStartArmHook hook(beforeArm);
    FireLatch elapsed;
    FireLatch startReturned;
    Timer timer(1);
    timer.Elapsed += [&](System::Object*, const ElapsedEventArgs&) { elapsed.signal(); };

    std::thread starter([&] {
        timer.Start();
        startReturned.signal();
    });
    const bool reachedBeforeArm = beforeArm.waitUntilEntered();
    if (!reachedBeforeArm) {
        beforeArm.release();
        starter.join();
        FAIL() << "Start never reached the deterministic pre-arm seam";
        return;
    }

    timer.Close();
    beforeArm.release();
    EXPECT_TRUE(startReturned.waitFor(1));
    starter.join();
    EXPECT_FALSE(elapsed.waitFor(1, 25));
    EXPECT_FALSE(timer.getEnabledProperty());
}

TEST(TimerLifecyclePinTests, AStaleFailingStartCannotDestroyANewerGeneration) {
    // Start A owns only a local paused worker when its injected hook blocks. Close invalidates A,
    // then Start B publishes a newer timer. When A finally throws, its catch path must recognize
    // the generation mismatch: moving timer_ or clearing Enabled there destroys B, the exact
    // exceptional interleaving this regression separates from an ordinary failed Start.
    CallbackBlocker oldStartBeforeArm;
    ScopedThrowingStartArmHook hook(oldStartBeforeArm);
    FireLatch elapsed;
    Timer timer(20);
    timer.Elapsed += [&](System::Object*, const ElapsedEventArgs&) { elapsed.signal(); };

    std::atomic<bool> oldStartThrew{false};
    std::thread oldStarter([&] {
        try {
            timer.Start();
        } catch (const std::runtime_error&) {
            oldStartThrew.store(true);
        }
    });
    const bool reachedBeforeArm = oldStartBeforeArm.waitUntilEntered();
    if (!reachedBeforeArm) {
        oldStartBeforeArm.release();
        oldStarter.join();
        FAIL() << "the stale Start never reached the deterministic exception seam";
        return;
    }

    timer.Close();
    SharpRuntime::Testing::TimerStartAccess<Timer>::setBeforeArmHook(nullptr);
    timer.Start();
    EXPECT_TRUE(timer.getEnabledProperty());

    oldStartBeforeArm.release();
    oldStarter.join();

    EXPECT_TRUE(oldStartThrew.load());
    EXPECT_TRUE(timer.getEnabledProperty())
        << "the stale failing generation disabled the newer running timer";
    EXPECT_TRUE(elapsed.waitFor(1))
        << "the stale failing generation destroyed the newer timer worker";
    timer.Stop();
}

TEST(TimerLifecyclePinTests, AStaleSuccessfulStartCannotDisableANewerGeneration) {
    // The non-exception twin of the case above. A stale pre-arm path used to write Enabled=false
    // merely because its generation no longer matched, even when that mismatch was Start B.
    CallbackBlocker oldStartBeforeArm;
    ScopedStartArmHook hook(oldStartBeforeArm);
    FireLatch elapsed;
    FireLatch oldStartReturned;
    Timer timer(20);
    timer.Elapsed += [&](System::Object*, const ElapsedEventArgs&) { elapsed.signal(); };

    std::thread oldStarter([&] {
        timer.Start();
        oldStartReturned.signal();
    });
    const bool reachedBeforeArm = oldStartBeforeArm.waitUntilEntered();
    if (!reachedBeforeArm) {
        oldStartBeforeArm.release();
        oldStarter.join();
        FAIL() << "the stale Start never reached the deterministic pre-arm seam";
        return;
    }

    timer.Close();
    SharpRuntime::Testing::TimerStartAccess<Timer>::setBeforeArmHook(nullptr);
    timer.Start();
    oldStartBeforeArm.release();
    EXPECT_TRUE(oldStartReturned.waitFor(1));
    oldStarter.join();

    EXPECT_TRUE(timer.getEnabledProperty())
        << "the stale generation overwrote the newer Start's Enabled state";
    EXPECT_TRUE(elapsed.waitFor(1));
    timer.Stop();
}

TEST(TimerLifecyclePinTests, ReschedulingFromInsideTheHandlerKeepsTheTimerRunning) {
    // setIntervalProperty from the callback reaches Change() on the worker's own state while the
    // worker is mid-callback. The Threading::Timer generation counter is what makes this safe;
    // this pins that it stays safe from this caller.
    FireLatch latch;
    std::atomic<int> fires{0};
    Timer timer(5);
    timer.Elapsed += [&](System::Object*, const ElapsedEventArgs&) {
        fires.fetch_add(1);
        latch.signal();
        timer.setIntervalProperty(6);
    };
    timer.Start();
    EXPECT_TRUE(latch.waitFor(3)) << "the timer stopped after rescheduling from its own handler";
    timer.Stop();
    EXPECT_GE(fires.load(), 3);
}

TEST(TimerLifecyclePinTests, FlippingAutoResetFromInsideTheHandlerIsSafe) {
    FireLatch latch;
    Timer timer(5);
    timer.Elapsed += [&](System::Object*, const ElapsedEventArgs&) {
        latch.signal();
        timer.setAutoResetProperty(!timer.getAutoResetProperty());
    };
    timer.Start();
    EXPECT_TRUE(latch.waitFor(2));
    EXPECT_NO_THROW(timer.Stop());
}

TEST(TimerLifecyclePinTests, StopBeforeTheFirstFireSuppressesIt) {
    std::atomic<int> fires{0};
    Timer timer(400);
    timer.Elapsed += [&](System::Object*, const ElapsedEventArgs&) { fires.fetch_add(1); };
    timer.Start();
    timer.Stop();
    // The only assertion in this file that must observe an ABSENCE, so it has no latch to wait on.
    // 400 ms was the configured interval and the wait is deliberately shorter than a second fire.
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    EXPECT_EQ(fires.load(), 0);
}

TEST(TimerLifecyclePinTests, DestructionWaitsForAnEnteredHandlerBeforeDestroyingMembers) {
    // A callback which has crossed the lifetime gate may freely use Timer/Elapsed until Raise
    // returns. Destruction on another thread must therefore block while this handler is held, then
    // complete promptly once it leaves. The old raw-this implementation deterministically failed
    // the first assertion: ~Timer reset/detached the worker and returned while the handler was
    // still inside the destroyed object's callback path.
    CallbackBlocker handler;
    auto timer = std::make_unique<Timer>(5);
    timer->Elapsed += [&](System::Object*, const ElapsedEventArgs&) { handler.enterAndWait(); };
    timer->Start();
    ASSERT_TRUE(handler.waitUntilEntered());

    FireLatch destructionStarted;
    FireLatch destructionFinished;
    std::thread destroyer([&] {
        destructionStarted.signal();
        timer.reset();
        destructionFinished.signal();
    });

    const bool started = destructionStarted.waitFor(1);
    // The bounded wait is only the failure backstop for the negative observation; entry itself is
    // synchronised by the two condition variables above, not by a scheduling sleep.
    const bool returnedWhileHandlerWasBlocked = destructionFinished.waitFor(1, 250);
    handler.release();
    const bool returnedAfterHandlerLeft = destructionFinished.waitFor(1);
    destroyer.join();

    EXPECT_TRUE(started);
    EXPECT_FALSE(returnedWhileHandlerWasBlocked)
        << "the Timer destructor returned while its callback still owned Timer members";
    EXPECT_TRUE(returnedAfterHandlerLeft);
}

TEST(TimerLifecyclePinTests, DestructionAndCloseFromTheEnteredHandlerShareTheLifetimeGate) {
    // The destructor must not reset timer_ concurrently with the supported Close-from-handler
    // path. Holding the handler at the gate makes the ordering deterministic; TSan observes the
    // member access while the ordinary assertions prove neither side deadlocks.
    CallbackBlocker handlerEntered;
    FireLatch closeReturned;
    FireLatch destructionFinished;
    auto timer = std::make_unique<Timer>(5);
    Timer* const timerView = timer.get();
    timer->Elapsed += [&](System::Object*, const ElapsedEventArgs&) {
        handlerEntered.enterAndWait();
        timerView->Close();
        closeReturned.signal();
    };
    timer->Start();
    ASSERT_TRUE(handlerEntered.waitUntilEntered());

    std::thread destroyer([&] {
        timer.reset();
        destructionFinished.signal();
    });
    EXPECT_FALSE(destructionFinished.waitFor(1, 100));
    handlerEntered.release();
    EXPECT_TRUE(closeReturned.waitFor(1));
    EXPECT_TRUE(destructionFinished.waitFor(1));
    destroyer.join();
}

TEST(TimerLifecyclePinTests, RestartingAfterCloseWorks) {
    FireLatch latch;
    Timer timer(5);
    timer.Elapsed += [&](System::Object*, const ElapsedEventArgs&) { latch.signal(); };
    timer.Start();
    ASSERT_TRUE(latch.waitFor(1));
    timer.Close();
    const int afterClose = latch.count();
    timer.Start();
    EXPECT_TRUE(latch.waitFor(afterClose + 2)) << "a closed timer could not be restarted";
    timer.Stop();
}

TEST(TimerLifecyclePinTests, OneShotCanRestartAsPeriodicWithoutKeepingStaleCallbackState) {
    FireLatch latch;
    Timer timer(5);
    timer.setAutoResetProperty(false);
    timer.Elapsed += [&](System::Object*, const ElapsedEventArgs&) { latch.signal(); };
    timer.Start();
    ASSERT_TRUE(latch.waitFor(1));
    EXPECT_FALSE(timer.getEnabledProperty());

    // The underlying Threading::Timer object remains reusable after its one shot. Its callback
    // must read today's AutoReset value, not the false value captured when that object was first
    // created, or it marks Enabled false on every periodic tick and makes Stop a no-op.
    timer.setAutoResetProperty(true);
    timer.Start();
    ASSERT_TRUE(latch.waitFor(3));
    EXPECT_TRUE(timer.getEnabledProperty());
    timer.Stop();
    // Close/Stop permits a callback that already entered to finish. Drain that bounded window,
    // then observe a second interval: the stale-capture bug kept firing indefinitely here.
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    const int afterInFlightDrain = latch.count();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_EQ(latch.count(), afterInFlightDrain);
}

TEST(TimerLifecyclePinTests, DisposeIsClose) {
    // `Dispose()` is declared inline as `Close()`. Pinned because the two names are the public
    // contract and a future divergence between them would be silent.
    Timer timer(5);
    timer.Start();
    EXPECT_TRUE(timer.getEnabledProperty());
    timer.Dispose();
    EXPECT_FALSE(timer.getEnabledProperty());
    EXPECT_NO_THROW(timer.Dispose());
}

// ---------------------------------------------------------------------------
// The review's deferred and out-of-scope items, pinned so none can move silently
// ---------------------------------------------------------------------------

TEST(TimerReviewPinTests, Fix2155_TimerIsNowAPolymorphicObjectType) {
    // INVERTED BY #2155. Its predecessor said "both fail the build if #2155 lands" -- and BOTH DID,
    // at compile time, which is the evidence the pin was load-bearing rather than decorative. It is
    // restated here because this is the file a future reader of #2155 opens first.
    static_assert(std::is_convertible_v<Timer*, System::Object*>,
                  "#2155: Timer derives from System::Object so Elapsed can report its sender");
    static_assert(std::is_polymorphic_v<Timer>, "#2155: and is therefore polymorphic");
    EXPECT_TRUE((std::is_convertible_v<Timer*, System::Object*>));
}

TEST(TimerReviewPinTests, TimerIsStillNonCopyable) {
    // Not a finding — recorded because #2155's base-class change is the kind of edit that quietly
    // reintroduces a copy constructor. #2155 HAS NOW LANDED and this still holds, which is exactly
    // why the pin was written before it rather than after.
    static_assert(!std::is_copy_constructible_v<Timer>);
    static_assert(!std::is_copy_assignable_v<Timer>);
    SUCCEED();
}
