// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Ticket #2341 / SR-AUD-202.  Monitor::Wait used to release exactly one recursion level,
// so a caller holding the monitor at depth n >= 2 kept n-1 levels locked for the whole
// wait; no other thread could Enter() the monitor to Pulse() it, and the wait never ended.
// The audit reproduced that as a bounded child process hitting a two-second timeout
// (exit 124).  Every case below is time-boxed for the same reason: the pre-fix behaviour
// of the discriminating cases is a deadlock, and a deadlock must fail one assertion here
// rather than hang the whole executable.
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <thread>

#include "System/Threading/Monitor.hpp"
#include "System/Threading/SynchronizationLockException.hpp"

using System::Threading::Monitor;
using namespace std::chrono_literals;

namespace {

    /**
     * Everything a scenario touches lives here and is owned by a shared_ptr that the
     * worker thread captures by value.  The worker is deliberately DETACHED rather than
     * joined: joining a deadlocked thread would hang the executable instead of failing
     * the assertion, and a detached thread that is still stuck when the test returns must
     * not be able to write through a dangling reference to a destroyed stack frame.
     */
    struct MonitorScenario {
        int lockTarget = 0;
        std::atomic<bool> finished{false};
        std::atomic<bool> waiterIsWaiting{false};
        std::atomic<int>  exitsCompleted{0};
        std::atomic<bool> foreignThreadCouldEnter{false};
        std::atomic<bool> timedWaitResult{false};
        std::atomic<bool> unexpectedException{false};
    };

    /**
     * Runs one helper-thread body, converting any escaping exception into a recorded flag.
     * A regression in the depth bookkeeping makes Enter/Exit/Pulse throw
     * SynchronizationLockException on threads that are not the one under test; letting that
     * escape a std::thread body calls std::terminate and takes the whole executable with it.
     */
    void Guarded(const std::shared_ptr<MonitorScenario>& s, const std::function<void()>& body) {
        try { body(); } catch (...) { s->unexpectedException.store(true); }
    }

    /** Runs body on a detached thread; returns true if it finished inside the budget. */
    bool RunBounded(const std::shared_ptr<MonitorScenario>& scenario,
                    std::function<void(const std::shared_ptr<MonitorScenario>&)> body,
                    std::chrono::milliseconds budget = 4000ms) {
        std::thread([scenario, body = std::move(body)] {
            Guarded(scenario, [&] { body(scenario); });
            scenario->finished.store(true);
        }).detach();
        const auto deadline = std::chrono::steady_clock::now() + budget;
        while (!scenario->finished.load() && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(1ms);
        return scenario->finished.load();
    }

    /**
     * Enters the monitor `depth` times, waits, and is woken by a second thread that must
     * be able to Enter() the very monitor the waiter holds recursively.  The second thread
     * only starts once the waiter has published that it is about to wait, so the wake-up
     * is a real handshake and not a timing guess; the monitor's own predicate loop is not
     * needed here because the waiter is the only consumer of this pulse.
     */
    void RecursiveWaitScenario(const std::shared_ptr<MonitorScenario>& s, int depth) {
        std::thread signaller([s] {
            Guarded(s, [s] {
                while (!s->waiterIsWaiting.load()) std::this_thread::sleep_for(1ms);
                std::this_thread::sleep_for(50ms); // let the waiter reach the wait itself
                Monitor::Enter(&s->lockTarget);    // before #2341 this blocked forever
                Monitor::PulseAll(&s->lockTarget);
                Monitor::Exit(&s->lockTarget);
            });
        });
        for (int i = 0; i < depth; ++i) Monitor::Enter(&s->lockTarget);
        s->waiterIsWaiting.store(true);
        Monitor::Wait(&s->lockTarget);
        // Wait can only have returned once it reacquired the monitor, which the signaller
        // released in its Exit, so the join here is immediate.  It happens BEFORE the Exit
        // loop on purpose: a too-shallow restore makes one of those Exit calls throw, and
        // unwinding past a still-joinable std::thread would abort the whole executable
        // instead of failing this one test.
        signaller.join();
        try {
            for (int i = 0; i < depth; ++i) {
                Monitor::Exit(&s->lockTarget);
                s->exitsCompleted.fetch_add(1);
            }
        } catch (...) {
            s->unexpectedException.store(true);
            return;
        }
        std::thread foreign([s] {
            Guarded(s, [s] {
                if (Monitor::TryEnter(&s->lockTarget)) {
                    s->foreignThreadCouldEnter.store(true);
                    Monitor::Exit(&s->lockTarget);
                }
            });
        });
        foreign.join();
    }

} // namespace

// --- the discriminating cases: depth >= 2 used to deadlock -------------------------

TEST(ThreadingMonitorRecursionTests, Wait_AtDepthTwo_ReleasesEveryLevelSoASignallerCanEnter) {
    auto s = std::make_shared<MonitorScenario>();
    ASSERT_TRUE(RunBounded(s, [](const std::shared_ptr<MonitorScenario>& c) { RecursiveWaitScenario(c, 2); }))
        << "Monitor::Wait at recursion depth 2 did not complete: the signalling thread "
           "could not Enter the monitor, which is SR-AUD-202";
    EXPECT_FALSE(s->unexpectedException.load()) << "an Exit after Wait threw: the depth was not restored";
    EXPECT_EQ(s->exitsCompleted.load(), 2);
    EXPECT_TRUE(s->foreignThreadCouldEnter.load());
}

TEST(ThreadingMonitorRecursionTests, Wait_AtDepthThree_ReleasesEveryLevelSoASignallerCanEnter) {
    auto s = std::make_shared<MonitorScenario>();
    ASSERT_TRUE(RunBounded(s, [](const std::shared_ptr<MonitorScenario>& c) { RecursiveWaitScenario(c, 3); }))
        << "Monitor::Wait at recursion depth 3 did not complete (SR-AUD-202)";
    // The full depth must be RESTORED, not collapsed: exactly three Exit calls must
    // succeed, and only after the third may another thread take the monitor.
    EXPECT_FALSE(s->unexpectedException.load()) << "an Exit after Wait threw: the depth was not restored";
    EXPECT_EQ(s->exitsCompleted.load(), 3);
    EXPECT_TRUE(s->foreignThreadCouldEnter.load());
}

TEST(ThreadingMonitorRecursionTests, Wait_AtDepthThree_RestoresExactlyThreeLevels_NotFewer) {
    // A repair that released the depth but restored only one level would pass the test
    // above's Exit loop only by throwing; assert the fourth Exit is the one that throws.
    auto s = std::make_shared<MonitorScenario>();
    ASSERT_TRUE(RunBounded(s, [](const std::shared_ptr<MonitorScenario>& c) {
        RecursiveWaitScenario(c, 3);
        if (c->unexpectedException.load()) return;
        // Depth is now 0 for this thread, so one more Exit must be rejected.
        try { Monitor::Exit(&c->lockTarget); } catch (const System::Threading::SynchronizationLockException&) {
            c->timedWaitResult.store(true); // reused flag: "the extra Exit threw"
        }
    }));
    EXPECT_FALSE(s->unexpectedException.load()) << "an Exit after Wait threw: the depth was not restored";
    EXPECT_EQ(s->exitsCompleted.load(), 3);
    EXPECT_TRUE(s->timedWaitResult.load())
        << "a fourth Exit after a depth-3 Wait must throw SynchronizationLockException";
}

// --- the timed overload takes the same path ---------------------------------------

TEST(ThreadingMonitorRecursionTests, TimedWait_AtDepthTwo_ReleasesEveryLevelWhileItWaits) {
    auto s = std::make_shared<MonitorScenario>();
    ASSERT_TRUE(RunBounded(s, [](const std::shared_ptr<MonitorScenario>& c) {
        std::thread prober([c] {
            Guarded(c, [c] {
                while (!c->waiterIsWaiting.load()) std::this_thread::sleep_for(1ms);
                std::this_thread::sleep_for(50ms);
                // The waiter is inside a TIMED wait holding depth 2; the monitor must be free.
                if (Monitor::TryEnter(&c->lockTarget)) {
                    c->foreignThreadCouldEnter.store(true);
                    Monitor::Exit(&c->lockTarget);
                }
            });
        });
        Monitor::Enter(&c->lockTarget);
        Monitor::Enter(&c->lockTarget);
        c->waiterIsWaiting.store(true);
        c->timedWaitResult.store(Monitor::Wait(&c->lockTarget, 400));
        prober.join();
        try {
            Monitor::Exit(&c->lockTarget);
            c->exitsCompleted.fetch_add(1);
            Monitor::Exit(&c->lockTarget);
            c->exitsCompleted.fetch_add(1);
        } catch (...) {
            c->unexpectedException.store(true);
        }
    }));
    EXPECT_FALSE(s->unexpectedException.load()) << "an Exit after the timed Wait threw: the depth was not restored";
    EXPECT_TRUE(s->foreignThreadCouldEnter.load())
        << "a TryEnter from another thread must succeed while a depth-2 timed Wait is blocked";
    EXPECT_FALSE(s->timedWaitResult.load()) << "an unpulsed timed Wait must still return false";
    EXPECT_EQ(s->exitsCompleted.load(), 2);
}

// --- unaffected controls ----------------------------------------------------------

TEST(ThreadingMonitorRecursionTests, Wait_AtDepthOne_IsUnchanged) {
    auto s = std::make_shared<MonitorScenario>();
    ASSERT_TRUE(RunBounded(s, [](const std::shared_ptr<MonitorScenario>& c) { RecursiveWaitScenario(c, 1); }));
    EXPECT_FALSE(s->unexpectedException.load());
    EXPECT_EQ(s->exitsCompleted.load(), 1);
    EXPECT_TRUE(s->foreignThreadCouldEnter.load());
}

TEST(ThreadingMonitorRecursionTests, Wait_WithoutHoldingTheMonitor_StillThrows) {
    int target = 0;
    EXPECT_THROW(Monitor::Wait(&target), System::Threading::SynchronizationLockException);
    EXPECT_THROW(Monitor::Wait(&target, 10), System::Threading::SynchronizationLockException);
}

TEST(ThreadingMonitorRecursionTests, TimedWait_AtDepthOne_ReturnsFalseOnTimeout) {
    int target = 0;
    Monitor::Enter(&target);
    EXPECT_FALSE(Monitor::Wait(&target, 20));
    EXPECT_NO_THROW(Monitor::Exit(&target));
}
