// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <csignal>
#include <mutex>
#include <thread>
#include <vector>
#include <unistd.h>
#include "System/ArgumentNullException.hpp"
#include "System/PlatformNotSupportedException.hpp"
#include "System/Runtime/InteropServices/PosixSignalRegistration.hpp"

using namespace System::Runtime::InteropServices;

namespace {
    bool waitFor(std::atomic<int>& counter, int target, int timeoutMs) {
        auto start = std::chrono::steady_clock::now();
        while (counter.load() < target) {
            if (std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start).count() > timeoutMs) return false;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return true;
    }
}

TEST(PosixSignalTests, Create_NullHandler_Throws) {
    EXPECT_THROW(PosixSignalRegistration::Create(PosixSignal::Sigwinch, nullptr), System::ArgumentNullException);
}

TEST(PosixSignalTests, Create_Sigkill_Throws) {
    EXPECT_THROW(PosixSignalRegistration::Create(PosixSignal::Sigkill, [](PosixSignalContext&) {}),
                 System::PlatformNotSupportedException);
}

TEST(PosixSignalTests, HandlerFires_OnMatchingSignal) {
    std::atomic<int> fired{0};
    auto reg = PosixSignalRegistration::Create(PosixSignal::Sigwinch, [&](PosixSignalContext& ctx) {
        EXPECT_EQ(ctx.getSignalProperty(), PosixSignal::Sigwinch);
        fired++;
    });
    kill(getpid(), SIGWINCH);
    EXPECT_TRUE(waitFor(fired, 1, 2000));
}

TEST(PosixSignalTests, MultipleHandlers_FireInReverseRegistrationOrder) {
    std::atomic<int> fired{0};
    std::vector<int> order;
    std::mutex orderMutex;

    // fired++ must be the LAST action in each handler with nothing implicit executing after it
    // (e.g. a lock_guard destructor) -- otherwise the main thread's waitFor(fired, ...) below
    // observes "handler done" while the watcher thread is still finishing trailing cleanup (the
    // mutex unlock), a genuine ThreadSanitizer-flagged data race (2026-07-14) on this test's
    // stack-local orderMutex/order once a later test reuses that stack memory. Scoping the
    // lock_guard so it unlocks BEFORE fired++ makes fired's atomic increment correctly
    // happens-after everything the handler does.
    auto reg1 = PosixSignalRegistration::Create(PosixSignal::Sigwinch, [&](PosixSignalContext&) {
        {
            std::lock_guard<std::mutex> l(orderMutex);
            order.push_back(1);
        }
        fired++;
    });
    auto reg2 = PosixSignalRegistration::Create(PosixSignal::Sigwinch, [&](PosixSignalContext&) {
        {
            std::lock_guard<std::mutex> l(orderMutex);
            order.push_back(2);
        }
        fired++;
    });

    kill(getpid(), SIGWINCH);
    ASSERT_TRUE(waitFor(fired, 2, 2000));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    ASSERT_EQ(order.size(), 2u);
    EXPECT_EQ(order[0], 2);
    EXPECT_EQ(order[1], 1);
}

TEST(PosixSignalTests, Dispose_UnregistersHandler) {
    std::atomic<int> fired{0};
    auto reg = PosixSignalRegistration::Create(PosixSignal::Sigwinch, [&](PosixSignalContext&) { fired++; });
    reg.Dispose();
    kill(getpid(), SIGWINCH);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(fired.load(), 0);
}

TEST(PosixSignalTests, Destructor_UnregistersHandler) {
    std::atomic<int> fired{0};
    {
        auto reg = PosixSignalRegistration::Create(PosixSignal::Sigwinch, [&](PosixSignalContext&) { fired++; });
    }
    kill(getpid(), SIGWINCH);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(fired.load(), 0);
}

// Registers SIGTERM and cancels its default disposition -- if Cancel() didn't actually suppress
// the OS default action (process termination), this test process would be killed before it could
// report a result, which is itself a meaningful (if blunt) failure signal.
TEST(PosixSignalTests, Cancel_SuppressesDefaultDisposition) {
    std::atomic<int> fired{0};
    auto reg = PosixSignalRegistration::Create(PosixSignal::Sigterm, [&](PosixSignalContext& ctx) {
        fired++;
        ctx.setCancelProperty(true);
    });
    kill(getpid(), SIGTERM);
    ASSERT_TRUE(waitFor(fired, 1, 2000));
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    SUCCEED(); // still alive
}

TEST(PosixSignalContextTests, Constructor_SetsSignalProperty) {
    PosixSignalContext ctx(PosixSignal::Sighup);
    EXPECT_EQ(ctx.getSignalProperty(), PosixSignal::Sighup);
    EXPECT_FALSE(ctx.getCancelProperty());
}

TEST(PosixSignalContextTests, SetCancelProperty_RoundTrips) {
    PosixSignalContext ctx(PosixSignal::Sigint);
    ctx.setCancelProperty(true);
    EXPECT_TRUE(ctx.getCancelProperty());
}

// ---------------------------------------------------------------------------------------
// Ticket #1974 / SR-AUD-172 (cause R-B of docs/SystemRuntimeNamespaceReviewPlan.md).
//
// Before the repair the self-pipe was created with a plain pipe(), so BOTH ends were
// blocking. onNativeSignal() writes one byte per delivery from inside a raw signal
// handler; once the watcher stops draining and the finite pipe buffer fills, that write
// blocks *inside the handler*, on whichever thread the OS chose for delivery. Measured in
// build-probe/1972_probe2_before.log: the reproduction child passed the pipe capacity and
// then died of SIGALRM.
//
// The shared state lives at namespace scope rather than on the test's stack on purpose: a
// handler copied into dispatchSignal()'s snapshot can still be invoked after Dispose()
// returns, so a handler capturing stack locals is a lifetime hazard independent of what
// this test measures (recorded separately as ticket #1986).
// ---------------------------------------------------------------------------------------
namespace {
    std::atomic<bool> floodWatcherParked{false};
    std::atomic<bool> floodWatcherRelease{false};
    std::atomic<bool> floodFinished{false};
    std::atomic<long> floodDelivered{0};

    // Linux's default pipe capacity is 65536 bytes and one byte is written per delivery, so
    // this is comfortably past the point at which a blocking write end would wedge.
    constexpr long kFloodDeliveries = 200000;
}

TEST(PosixSignalTests, SignalFlood_PastPipeCapacity_DoesNotBlockTheDeliveringThread) {
    floodWatcherParked.store(false);
    floodWatcherRelease.store(false);
    floodFinished.store(false);
    floodDelivered.store(0);

    auto reg = PosixSignalRegistration::Create(PosixSignal::Sigwinch, [](PosixSignalContext& ctx) {
        ctx.setCancelProperty(true);
        // Park the watcher inside the callback so nothing drains the self-pipe. Only the
        // first invocation parks; later ones return at once so teardown cannot deadlock.
        if (!floodWatcherParked.exchange(true)) {
            while (!floodWatcherRelease.load())
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    kill(getpid(), SIGWINCH);
    {
        auto start = std::chrono::steady_clock::now();
        while (!floodWatcherParked.load()) {
            ASSERT_LT(std::chrono::duration_cast<std::chrono::seconds>(
                          std::chrono::steady_clock::now() - start).count(), 5)
                << "the watcher never entered the callback, so this test would prove nothing "
                   "about the self-pipe -- see review plan §4.4";
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    // raise() delivers to the CALLING thread, so a regression blocks this worker rather than
    // the test's main thread. That is what lets the assertion below fail instead of wedging
    // the whole executable.
    std::thread flood([] {
        for (long i = 0; i < kFloodDeliveries; ++i) {
            raise(SIGWINCH);
            floodDelivered.fetch_add(1);
        }
        floodFinished.store(true);
    });

    bool finished = false;
    {
        auto start = std::chrono::steady_clock::now();
        while (!(finished = floodFinished.load())) {
            if (std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::steady_clock::now() - start).count() > 20) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    // Releasing the watcher drains the pipe, which unblocks a regressed write, so the worker
    // is always joinable and this test never leaves a stuck thread behind.
    floodWatcherRelease.store(true);
    flood.join();

    EXPECT_TRUE(finished)
        << "the delivering thread blocked inside the raw signal handler after "
        << floodDelivered.load() << " of " << kFloodDeliveries
        << " deliveries: the self-pipe write end is blocking again (SR-AUD-172)";
    EXPECT_EQ(floodDelivered.load(), kFloodDeliveries);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

TEST(PosixSignalTests, SignalDelivery_StillWorksAfterAFlood) {
    // The dropped bytes must cost nothing: pending_ is a per-signal flag already set before
    // the write, so coalescing is the designed behaviour and delivery must still be observed.
    std::atomic<int> fired{0};
    auto reg = PosixSignalRegistration::Create(PosixSignal::Sigwinch, [&](PosixSignalContext& ctx) {
        ctx.setCancelProperty(true);
        fired++;
    });
    for (int i = 0; i < 1000; ++i) raise(SIGWINCH);
    EXPECT_TRUE(waitFor(fired, 1, 5000));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}
