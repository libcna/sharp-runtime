// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <csignal>
#include <mutex>
#include <optional>
#include <thread>
#include <set>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
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

// ---------------------------------------------------------------------------------------
// Ticket #1975 / SR-AUD-169 (cause R-A of docs/SystemRuntimeNamespaceReviewPlan.md).
//
// installIfNeeded() used to pass nullptr as sigaction()'s oldact out-parameter, throwing the
// process's existing disposition away, and uninstallIfUnused() then imposed SIG_DFL -- the OS
// default, not "undo". Measured before the repair (build-probe/1972_probe2_before.log):
// before_create=original -> during_registration original_calls=0 -> after_dispose=SIG_DFL,
// and the sharper case sighup_before_create=SIG_IGN -> sighup_after_dispose=SIG_DFL, where
// SIGHUP's default disposition TERMINATES the process.
//
// These tests use SIGWINCH, whose default is ignore, so a regression cannot kill the suite;
// each one restores the disposition it installed whatever the outcome. What they measure --
// that the exact prior disposition comes back -- is identical for SIGHUP.
//
// They deliberately do NOT assert anything about what happens DURING delivery. Chaining to a
// saved handler is ticket #1979 and is approval-gated; #1975 saves and restores only.
// ---------------------------------------------------------------------------------------
namespace {
    std::atomic<int> priorHandlerCalls{0};

    extern "C" void priorSigwinchHandler(int) { priorHandlerCalls.fetch_add(1); }

    /** Puts SIGWINCH back to the OS default however a test exits. */
    struct SigwinchDispositionGuard {
        ~SigwinchDispositionGuard() {
            struct sigaction dfl{};
            dfl.sa_handler = SIG_DFL;
            sigemptyset(&dfl.sa_mask);
            dfl.sa_flags = 0;
            sigaction(SIGWINCH, &dfl, nullptr);
        }
    };

    struct sigaction currentSigwinchDisposition() {
        struct sigaction cur{};
        sigaction(SIGWINCH, nullptr, &cur);
        return cur;
    }
}

TEST(PosixSignalTests, Dispose_RestoresAPreExistingCustomHandler) {
    SigwinchDispositionGuard guard;

    struct sigaction mine{};
    mine.sa_handler = priorSigwinchHandler;
    sigemptyset(&mine.sa_mask);
    mine.sa_flags = SA_RESTART;
    ASSERT_EQ(sigaction(SIGWINCH, &mine, nullptr), 0);
    ASSERT_EQ(currentSigwinchDisposition().sa_handler, priorSigwinchHandler);

    {
        std::atomic<int> fired{0};
        auto reg = PosixSignalRegistration::Create(PosixSignal::Sigwinch,
                                                    [&fired](PosixSignalContext& ctx) {
                                                        ctx.setCancelProperty(true);
                                                        fired++;
                                                    });
        // While registered the port owns the disposition -- that part is unchanged.
        EXPECT_NE(currentSigwinchDisposition().sa_handler, priorSigwinchHandler);
        kill(getpid(), SIGWINCH);
        EXPECT_TRUE(waitFor(fired, 1, 2000));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(currentSigwinchDisposition().sa_handler, priorSigwinchHandler)
        << "the pre-existing handler was not restored (SR-AUD-169)";

    // And it works again, not merely looks right.
    priorHandlerCalls.store(0);
    kill(getpid(), SIGWINCH);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_GE(priorHandlerCalls.load(), 1);
}

TEST(PosixSignalTests, Dispose_RestoresSigIgnAsSigIgnNotSigDfl) {
    // The case that carries the severity: for a signal whose default terminates, restoring
    // SIG_DFL where SIG_IGN was set arms an unrelated kill.
    SigwinchDispositionGuard guard;

    struct sigaction ign{};
    ign.sa_handler = SIG_IGN;
    sigemptyset(&ign.sa_mask);
    ign.sa_flags = 0;
    ASSERT_EQ(sigaction(SIGWINCH, &ign, nullptr), 0);

    {
        auto reg = PosixSignalRegistration::Create(
            PosixSignal::Sigwinch, [](PosixSignalContext& ctx) { ctx.setCancelProperty(true); });
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(currentSigwinchDisposition().sa_handler, SIG_IGN)
        << "SIG_IGN was replaced by SIG_DFL (SR-AUD-169)";
}

TEST(PosixSignalTests, Dispose_RestoresSigDflWhenSigDflWasWhatWasThere) {
    // The control: restoring must reproduce whatever was found, including the default.
    SigwinchDispositionGuard guard;

    struct sigaction dfl{};
    dfl.sa_handler = SIG_DFL;
    sigemptyset(&dfl.sa_mask);
    dfl.sa_flags = 0;
    ASSERT_EQ(sigaction(SIGWINCH, &dfl, nullptr), 0);

    {
        auto reg = PosixSignalRegistration::Create(
            PosixSignal::Sigwinch, [](PosixSignalContext& ctx) { ctx.setCancelProperty(true); });
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(currentSigwinchDisposition().sa_handler, SIG_DFL);
}

TEST(PosixSignalTests, Dispose_RestoresOnlyAfterTheLastRegistrationForThatSignal) {
    SigwinchDispositionGuard guard;

    struct sigaction mine{};
    mine.sa_handler = priorSigwinchHandler;
    sigemptyset(&mine.sa_mask);
    mine.sa_flags = SA_RESTART;
    ASSERT_EQ(sigaction(SIGWINCH, &mine, nullptr), 0);

    {
        auto outer = PosixSignalRegistration::Create(
            PosixSignal::Sigwinch, [](PosixSignalContext& ctx) { ctx.setCancelProperty(true); });
        {
            auto inner = PosixSignalRegistration::Create(
                PosixSignal::Sigwinch, [](PosixSignalContext& ctx) { ctx.setCancelProperty(true); });
        }
        // The inner Dispose must NOT restore: the outer registration is still live.
        EXPECT_NE(currentSigwinchDisposition().sa_handler, priorSigwinchHandler)
            << "an inner Dispose restored the prior handler while an outer registration was live";
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(currentSigwinchDisposition().sa_handler, priorSigwinchHandler);
}

TEST(PosixSignalTests, Dispose_RestoresSaFlagsAndSaMaskNotJustTheHandler) {
    // The whole struct sigaction is the disposition. Saving only sa_handler would pass the
    // three tests above and still silently drop the caller's flags and mask.
    SigwinchDispositionGuard guard;

    struct sigaction mine{};
    mine.sa_handler = priorSigwinchHandler;
    sigemptyset(&mine.sa_mask);
    sigaddset(&mine.sa_mask, SIGUSR2);
    mine.sa_flags = SA_RESTART | SA_NODEFER;
    ASSERT_EQ(sigaction(SIGWINCH, &mine, nullptr), 0);

    {
        auto reg = PosixSignalRegistration::Create(
            PosixSignal::Sigwinch, [](PosixSignalContext& ctx) { ctx.setCancelProperty(true); });
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    struct sigaction restored = currentSigwinchDisposition();
    EXPECT_EQ(restored.sa_handler, priorSigwinchHandler);
    EXPECT_NE(restored.sa_flags & SA_NODEFER, 0) << "SA_NODEFER was dropped on restore";
    EXPECT_NE(restored.sa_flags & SA_RESTART, 0) << "SA_RESTART was dropped on restore";
    EXPECT_EQ(sigismember(&restored.sa_mask, SIGUSR2), 1) << "sa_mask was dropped on restore";
}

// ---------------------------------------------------------------------------------------
// Ticket #1977 / SR-AUD-170 (cause R-D of docs/SystemRuntimeNamespaceReviewPlan.md).
//
// toNativeSignalNumber() indexed a ten-entry table by -value-1, so everything outside
// [-10,-1] became 0 and Create() threw PlatformNotSupportedException. Measured before the
// repair (build-probe/1972_probe2_before.log), the rejection was wider than the finding
// states: SIGUSR1(10), SIGUSR2(12), SIGPIPE(13), SIGALRM(14) AND SIGWINCH(28) -- the
// positive spelling of a signal the port already supports by name. The change is strictly
// WIDENING; every rejection that was correct is retained and asserted below.
// ---------------------------------------------------------------------------------------

TEST(PosixSignalTests, Create_RawPositiveSignalNumber_IsAcceptedAndDelivers) {
    std::atomic<int> fired{0};
    auto reg = PosixSignalRegistration::Create(static_cast<PosixSignal>(SIGUSR1),
                                                [&fired](PosixSignalContext& ctx) {
                                                    ctx.setCancelProperty(true);
                                                    fired++;
                                                });
    kill(getpid(), SIGUSR1);
    EXPECT_TRUE(waitFor(fired, 1, 2000));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

TEST(PosixSignalTests, Create_RawSpellingOfANamedSignal_SharesTheNamedSpellingsDispatch) {
    // The part SR-AUD-170 does not name: the enum was accepted in exactly one of its two
    // valid spellings. Both must now reach the same bucket, so a delivery caused under one
    // spelling is seen by a handler registered under the other.
    std::atomic<int> viaNamed{0};
    std::atomic<int> viaRaw{0};

    auto named = PosixSignalRegistration::Create(PosixSignal::Sigwinch,
                                                  [&viaNamed](PosixSignalContext& ctx) {
                                                      ctx.setCancelProperty(true);
                                                      viaNamed++;
                                                  });
    auto raw = PosixSignalRegistration::Create(static_cast<PosixSignal>(SIGWINCH),
                                                [&viaRaw](PosixSignalContext& ctx) {
                                                    ctx.setCancelProperty(true);
                                                    viaRaw++;
                                                });

    kill(getpid(), SIGWINCH);
    EXPECT_TRUE(waitFor(viaNamed, 1, 2000));
    EXPECT_TRUE(waitFor(viaRaw, 1, 2000));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

TEST(PosixSignalTests, Create_RawSigkillAndRawSigstop_AreStillRejected) {
    // Reachable by number for the first time. Both must be reported the same way as the
    // named PosixSignal::Sigkill rather than left to sigaction()'s EINVAL.
    EXPECT_THROW(PosixSignalRegistration::Create(static_cast<PosixSignal>(SIGKILL),
                                                  [](PosixSignalContext&) {}),
                 System::PlatformNotSupportedException);
    EXPECT_THROW(PosixSignalRegistration::Create(static_cast<PosixSignal>(SIGSTOP),
                                                  [](PosixSignalContext&) {}),
                 System::PlatformNotSupportedException);
    // ... and the named spelling is unchanged.
    EXPECT_THROW(PosixSignalRegistration::Create(PosixSignal::Sigkill, [](PosixSignalContext&) {}),
                 System::PlatformNotSupportedException);
}

TEST(PosixSignalTests, Create_OutOfRangeValues_AreStillRejected) {
    // Zero, a value past what the pending-flag table can track, and an unnamed negative.
    // Accepting a number this dispatcher cannot record would be worse than rejecting it: the
    // registration would appear to succeed and then never deliver.
    EXPECT_THROW(PosixSignalRegistration::Create(static_cast<PosixSignal>(0),
                                                  [](PosixSignalContext&) {}),
                 System::PlatformNotSupportedException);
    EXPECT_THROW(PosixSignalRegistration::Create(static_cast<PosixSignal>(100000),
                                                  [](PosixSignalContext&) {}),
                 System::PlatformNotSupportedException);
    EXPECT_THROW(PosixSignalRegistration::Create(static_cast<PosixSignal>(-99),
                                                  [](PosixSignalContext&) {}),
                 System::PlatformNotSupportedException);
}

TEST(PosixSignalTests, Create_RawSignal_RestoresTheDispositionOnDispose) {
    // #1975's contract must hold for the newly reachable spelling too, not only for named
    // members: the raw path installs through exactly the same registry.
    struct sigaction saved{};
    ASSERT_EQ(sigaction(SIGUSR2, nullptr, &saved), 0);

    struct sigaction ign{};
    ign.sa_handler = SIG_IGN;
    sigemptyset(&ign.sa_mask);
    ign.sa_flags = 0;
    ASSERT_EQ(sigaction(SIGUSR2, &ign, nullptr), 0);

    {
        auto reg = PosixSignalRegistration::Create(
            static_cast<PosixSignal>(SIGUSR2),
            [](PosixSignalContext& ctx) { ctx.setCancelProperty(true); });
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    struct sigaction after{};
    ASSERT_EQ(sigaction(SIGUSR2, nullptr, &after), 0);
    EXPECT_EQ(after.sa_handler, SIG_IGN);

    sigaction(SIGUSR2, &saved, nullptr);
}

// ---------------------------------------------------------------------------------------
// Ticket #1986 -- a handler could be invoked AFTER its registration's Dispose() returned.
//
// dispatchSignal() copies the matching entries -- including each std::function handler --
// into a local snapshot under registryMutex_, releases the mutex, and only then invokes
// them. Dispose() takes the same mutex and erases the entry, so the erase stops FUTURE
// dispatches but not the pending one: a dispatch that already has its snapshot still runs
// its copy of the handler after Dispose() has come back. A handler capturing caller stack
// state by reference -- the natural way to write one, and what every test above does --
// then touches a dead frame. Measured before the repair as
// `dispose_returned_while_dispatch_parked=1` / `victim_handler_ran_AFTER_dispose_returned=1`
// (build-probe/1986_probe1_before.log) and, under ASan, as a stack-use-after-scope written
// from dispatchSignal (build-probe/1986_probe2_before.log). It is a LIFETIME defect, not a
// timing one; the sleep-based Dispose checks above only make it unlikely.
//
// No SR-AUD identifier was issued: audit numbering stays frozen at 364.
//
// Both tests below keep their shared state at NAMESPACE scope, deliberately. A regression
// must be reported as a failed expectation, not as this suite corrupting its own stack --
// the state a late callback touches has to outlive the test frame for the assertion to be
// trustworthy.
namespace {
    std::atomic<bool> lateParked{false};
    std::atomic<bool> lateRelease{false};
    std::atomic<bool> lateDisposeReturned{false};
    std::atomic<int>  lateVictimRan{0};
    std::atomic<int>  lateVictimRanAfterDispose{0};

    // Waits for `flag`, returning false on timeout instead of spinning forever, so no
    // failure path in these tests can wedge the executable.
    bool waitForFlag(std::atomic<bool>& flag, bool want, int timeoutMs) {
        auto start = std::chrono::steady_clock::now();
        while (flag.load() != want) {
            if (std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start).count() > timeoutMs) return false;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return true;
    }
}

// The ordering assertion is exact, not probabilistic: two registrations share SIGWINCH, the
// one registered LAST runs FIRST, and it parks the watcher inside the callback. The dispatch
// snapshot is therefore already taken and already contains BOTH handlers while the main
// thread disposes the first registration. Whether the victim's handler observes
// lateDisposeReturned is then decided purely by the happens-before the repair establishes.
TEST(PosixSignalTests, Dispose_DoesNotReturnWhileAnInFlightDispatchCanStillRunTheHandler) {
    lateParked.store(false);
    lateRelease.store(false);
    lateDisposeReturned.store(false);
    lateVictimRan.store(0);
    lateVictimRanAfterDispose.store(0);

    // Registered FIRST -> invoked SECOND. This is the registration under test.
    auto victim = PosixSignalRegistration::Create(PosixSignal::Sigwinch, [](PosixSignalContext& ctx) {
        ctx.setCancelProperty(true);
        if (lateDisposeReturned.load()) lateVictimRanAfterDispose.fetch_add(1);
        lateVictimRan.fetch_add(1);
    });

    // Registered SECOND -> invoked FIRST. Parks the dispatch mid-snapshot. Only the first
    // invocation parks, so nothing can deadlock during teardown.
    auto parker = PosixSignalRegistration::Create(PosixSignal::Sigwinch, [](PosixSignalContext& ctx) {
        ctx.setCancelProperty(true);
        if (!lateParked.exchange(true)) waitForFlag(lateRelease, true, 30000);
    });

    kill(getpid(), SIGWINCH);
    ASSERT_TRUE(waitForFlag(lateParked, true, 5000))
        << "the watcher never entered the parking callback, so this test would prove nothing";

    // Dispose on a worker: a correctly waiting Dispose() blocks, and must not block the
    // thread that is going to release the parked watcher.
    std::thread disposer([&] {
        victim.Dispose();
        lateDisposeReturned.store(true);
    });

    // A Dispose() that returns while the dispatch is parked has already broken the contract.
    // Bounded, and only ever produces a false PASS on a very slow machine, never a false FAIL.
    const bool returnedWhileParked = waitForFlag(lateDisposeReturned, true, 500);

    // Released unconditionally and before any assertion below, so a regression cannot leave
    // the watcher thread or the disposer parked.
    lateRelease.store(true);
    disposer.join();
    EXPECT_TRUE(waitForFlag(lateDisposeReturned, true, 5000));

    EXPECT_EQ(lateVictimRanAfterDispose.load(), 0)
        << "the handler ran after its own Dispose() returned: the caller was free to destroy "
           "everything that handler captured while the call was still pending (ticket #1986)";
    EXPECT_FALSE(returnedWhileParked)
        << "Dispose() returned while a dispatch holding a copy of this handler was still parked";
    EXPECT_EQ(lateVictimRan.load(), 1)
        << "the repair must make Dispose() WAIT for the pending invocation, not cancel it";
}

namespace {
    std::atomic<bool> reentrantParked{false};
    std::atomic<int>  reentrantCompleted{0};
    std::optional<PosixSignalRegistration> reentrantSelf;
}

// The wait added above must not be taken when the caller IS the dispatching thread. A handler
// is expressly allowed to dispose itself -- its own token is in flight by definition, so a
// Dispose() that waited unconditionally would be the watcher thread waiting for itself, and
// the snapshot exists precisely so that registryMutex_ is not held across invocation.
//
// If this ever regresses, the watcher thread wedges rather than the main thread: Dispose()
// clears token_ before it waits, so the destructors below stay no-ops and teardown still
// completes. The bounded wait then fails loudly instead of hanging the executable.
TEST(PosixSignalTests, Dispose_CalledFromInsideItsOwnHandler_DoesNotDeadlock) {
    reentrantParked.store(false);
    reentrantCompleted.store(0);

    reentrantSelf.emplace(PosixSignalRegistration::Create(
        PosixSignal::Sigwinch, [](PosixSignalContext& ctx) {
            ctx.setCancelProperty(true);
            reentrantParked.store(true);
            reentrantSelf->Dispose();   // self-dispose from the watcher thread
            reentrantCompleted.fetch_add(1);
        }));

    kill(getpid(), SIGWINCH);

    ASSERT_TRUE(waitForFlag(reentrantParked, true, 5000)) << "the handler never ran";
    auto start = std::chrono::steady_clock::now();
    while (reentrantCompleted.load() == 0 &&
           std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - start).count() < 5000) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    EXPECT_EQ(reentrantCompleted.load(), 1)
        << "Dispose() called from inside the handler did not return: the watcher thread is "
           "waiting for its own in-flight dispatch (ticket #1986)";

    reentrantSelf.reset();
}

// ---------------------------------------------------------------------------------------
// Ticket #1985 -- the self-pipe descriptors were inherited across exec().
//
// ensureWatcherStarted() called ::pipe() with no close-on-exec, so both ends survived an
// exec() in a child process: the child held a WRITE end to a pipe whose watcher thread does
// not exist in it (fork() gives the child no watcher) and a READ end keeping the pipe alive
// for as long as the child ran. Measured before the repair as
// `new_pipe_fds=1 with_cloexec=0` (build-probe/1985_probe1_before.log; only one end shows up
// in a naive scan because pipe() reuses the descriptor number the scan itself had just
// released -- which is exactly why the test below classifies descriptors by TYPE rather than
// by presence).
//
// This is a SEPARATE defect from SR-AUD-172, which is about the write BLOCKING, and it was
// deliberately kept out of #1974 so that a change to exec() inheritance would not land under
// cover of a liveness repair (docs/SystemRuntimeNamespaceReviewPlan.md §15). No SR-AUD
// identifier was issued: numbering stays frozen at 364.
#if defined(__linux__)
namespace {
    // Descriptor numbers that are currently open AND are pipes. Classifying by type, not by
    // presence, is what makes the before/after difference exact: the directory handle the scan
    // itself opens releases a low descriptor number that ::pipe() then immediately reuses, so a
    // plain set-difference of open descriptors misses one of the two ends.
    std::set<int> openPipeFds() {
        std::set<int> out;
        DIR* d = ::opendir("/proc/self/fd");
        if (d == nullptr) return out;
        while (dirent* e = ::readdir(d)) {
            if (e->d_name[0] == '.') continue;
            int fd = std::atoi(e->d_name);
            struct stat st{};
            if (::fstat(fd, &st) == 0 && S_ISFIFO(st.st_mode)) out.insert(fd);
        }
        ::closedir(d);
        return out;
    }

    // Child role: a FRESH process, so this Create() is the one that builds the self-pipe and
    // the before/after difference is unambiguous. Never returns.
    [[noreturn]] void runCloexecChildRoleAndExit() {
        const std::set<int> before = openPipeFds();
        auto reg = PosixSignalRegistration::Create(
            PosixSignal::Sigwinch, [](PosixSignalContext& ctx) { ctx.setCancelProperty(true); });

        std::vector<int> created;
        for (int fd : openPipeFds())
            if (!before.count(fd)) created.push_back(fd);

        // Exactly the two ends of one self-pipe, or the probe proves nothing.
        if (created.size() != 2) _exit(10);
        for (int fd : created) {
            int flags = ::fcntl(fd, F_GETFD, 0);
            if (flags == -1 || !(flags & FD_CLOEXEC)) _exit(11);
        }

        // ...and confirm the consequence rather than only the flag: exec a shell and let it
        // report whether either descriptor is still open in the exec'd image.
        char script[256];
        std::snprintf(script, sizeof(script),
                      "if [ -e /proc/self/fd/%d ] || [ -e /proc/self/fd/%d ]; then exit 12; fi; exit 0",
                      created[0], created[1]);
        pid_t grandchild = ::fork();
        if (grandchild == 0) {
            ::execl("/bin/sh", "sh", "-c", script, static_cast<char*>(nullptr));
            _exit(13);
        }
        if (grandchild < 0) _exit(14);
        int gstatus = 0;
        if (::waitpid(grandchild, &gstatus, 0) != grandchild) _exit(15);
        if (!WIFEXITED(gstatus) || WEXITSTATUS(gstatus) != 0) _exit(WIFEXITED(gstatus) ? WEXITSTATUS(gstatus) : 16);
        _exit(0);
    }
}

// One test, two roles. The child role runs in a fresh image of this executable -- which is the
// only way to observe the descriptors being created, since the self-pipe is built once per
// process and any earlier test in this suite has already built it -- and _exit()s with its
// verdict before GoogleTest reports, so it adds no second case and no skip to the suite.
TEST(PosixSignalTests, SelfPipe_DescriptorsAreCloseOnExec) {
    if (::getenv("SHARP_RUNTIME_CLOEXEC_CHILD") != nullptr) runCloexecChildRoleAndExit();

    char exePath[4096];
    ssize_t n = ::readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
    ASSERT_GT(n, 0) << "cannot locate this executable, so the child role cannot be launched";
    exePath[n] = '\0';

    pid_t child = ::fork();
    ASSERT_GE(child, 0) << "fork() failed";
    if (child == 0) {
        ::setenv("SHARP_RUNTIME_CLOEXEC_CHILD", "1", 1);
        ::execl(exePath, exePath,
                "--gtest_filter=PosixSignalTests.SelfPipe_DescriptorsAreCloseOnExec",
                static_cast<char*>(nullptr));
        _exit(90);
    }

    int status = 0;
    ASSERT_EQ(::waitpid(child, &status, 0), child);
    ASSERT_TRUE(WIFEXITED(status)) << "the child role did not exit normally";
    const int code = WEXITSTATUS(status);
    EXPECT_EQ(code, 0)
        << "self-pipe close-on-exec check failed (child exit " << code << "): "
        << "10=did not observe exactly two new pipe descriptors, 11=FD_CLOEXEC missing, "
        << "12=a descriptor survived exec(), 13/90=exec failed, 14/15/16=fork/wait failed";
}
#endif // __linux__
