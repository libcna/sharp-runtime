// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Ticket #2157 — the closing ticket of the System::Timers review (#2153). Zero executable
// production change.
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
#include <mutex>
#include <type_traits>

#include "System/Timers/Timer.hpp"

using namespace System::Timers;

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

} // namespace

TEST(TimerLifecyclePinTests, CloseFromInsideTheHandlerDoesNotDeadlockOrCrash) {
    // The handler destroys the underlying System::Threading::Timer from inside its own callback.
    // That Dispose() detaches rather than joins, which is what makes it survivable -- a join here
    // would be a self-join deadlock.
    FireLatch latch;
    Timer timer(5);
    timer.Elapsed += [&](System::Object*, const ElapsedEventArgs&) {
        latch.signal();
        timer.Close();
    };
    timer.Start();
    ASSERT_TRUE(latch.waitFor(1));
    EXPECT_FALSE(timer.getEnabledProperty());
    EXPECT_NO_THROW(timer.Close());
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

TEST(TimerLifecyclePinTests, DestructionWhileEnabledIsSafe) {
    FireLatch latch;
    {
        Timer timer(5);
        timer.Elapsed += [&](System::Object*, const ElapsedEventArgs&) { latch.signal(); };
        timer.Start();
        ASSERT_TRUE(latch.waitFor(1));
    }
    // Reaching here at all is the assertion: the destructor runs Close(), which resets the
    // underlying timer while a callback may be in flight.
    SUCCEED();
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

TEST(TimerReviewPinTests, TimerRemainsANonPolymorphicNonObjectType_SeeBlockedTicket2155) {
    // The same compile-time pin as TimerExceptionBoundaryPinTests, restated here because this is
    // the file a future reader of ticket #2155 will open. Both fail the build if #2155 lands.
    static_assert(!std::is_convertible_v<Timer*, System::Object*>,
                  "#2155 landed without updating its pin");
    static_assert(!std::is_polymorphic_v<Timer>, "#2155 landed without updating its pin");
    EXPECT_FALSE((std::is_convertible_v<Timer*, System::Object*>));
}

TEST(TimerReviewPinTests, TimerIsStillNonCopyable) {
    // Not a finding — recorded because #2155's base-class change is the kind of edit that quietly
    // reintroduces a copy constructor.
    static_assert(!std::is_copy_constructible_v<Timer>);
    static_assert(!std::is_copy_assignable_v<Timer>);
    SUCCEED();
}
