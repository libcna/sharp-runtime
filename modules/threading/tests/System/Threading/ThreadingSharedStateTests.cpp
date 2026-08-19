// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Permanent regressions for ticket #1955 -- cause T-A of docs/ThreadingNamespaceReviewPlan.md,
// "shared mutable state is observed outside its own mutex" (SR-AUD-203 race half, SR-AUD-207
// across three types, SR-AUD-212, SR-AUD-216, SR-AUD-218).
//
// A data race cannot be asserted from inside an ordinary test binary: the failure is
// undefined behaviour, not a wrong value, and the detector is ThreadSanitizer. What this file
// pins is everything around the repair that *can* be asserted deterministically:
//
//   * the LAYOUT GATE -- sizeof/alignof of all six affected types, which #1955's acceptance
//     criteria make the condition for landing without user approval. If a future edit grows
//     one of these types, this file fails before the header reaches a consumer;
//   * that each repaired reader still returns the right answer, uncontended and contended;
//   * that the two counters repaired with an atomic field rather than a locking property keep
//     working from the one place a locking property would have deadlocked.
//
// The race evidence itself is build-probe/1955_probe1_shared_state_races.cpp, whose TSan log
// went from 13 reports across seven scenarios to zero
// (build-probe/1955_probe1_tsan_{before,after}.log).
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <thread>
#include <vector>

#include "System/Exception.hpp"
#include "System/ObjectDisposedException.hpp"
#include "System/Threading/Barrier.hpp"
#include "System/Threading/CountdownEvent.hpp"
#include "System/Threading/LazyInitializer.hpp"
#include "System/Threading/ManualResetEventSlim.hpp"
#include "System/Threading/ReaderWriterLockSlim.hpp"
#include "System/Threading/SemaphoreSlim.hpp"
#include "System/Threading/ThreadLocal.hpp"

using namespace System::Threading;

// ===========================================================================
// The layout gate
// ===========================================================================

// Measured on the verified Linux/GCC baseline before and after #1955 and found identical
// (build-probe/1955_probe1_layout_{before,after}.log). Every one of these headers carries its
// implementation inline, so a growth here is a consumer-visible ABI change -- which is why the
// numbers are pinned and every change to one must be deliberate.
//
// UPDATED 2026-08-19 by ticket #2389, and the update is the point rather than an inconvenience:
// this gate is what caught the growth. ReaderWriterLockSlim went 120 -> 128 when #2389 added
// waitingReaders_ and waitingUpgraders_ for Dispose's waiter check. (#1957/SR-AUD-204's earlier
// waitingWriters_ was layout-NEUTRAL -- it landed in padding the type already had, and this gate
// stayed green through it, which is the evidence that 128 is a real change and not drift.)
// It lands under docs/StandingApprovals.md SA-3: private data members, sizeof pinned here and in
// ReaderWriterWriterPreferenceTests.Decl1957_TheWaiterCountsLayout, migration note recording the
// full-consumer-rebuild requirement, and the full gate.
TEST(ThreadingSharedStateTests, RepairedTypes_LayoutUnchanged) {
    EXPECT_EQ(sizeof(ReaderWriterLockSlim), 128u);   // #2389: was 120
    EXPECT_EQ(sizeof(SemaphoreSlim), 104u);
    EXPECT_EQ(sizeof(ManualResetEventSlim), 112u);
    EXPECT_EQ(sizeof(CountdownEvent), 104u);
    EXPECT_EQ(sizeof(Barrier), 160u);
    EXPECT_EQ(sizeof(ThreadLocal<int>), 56u);

    EXPECT_EQ(alignof(ReaderWriterLockSlim), 8u);
    EXPECT_EQ(alignof(SemaphoreSlim), 8u);
    EXPECT_EQ(alignof(ManualResetEventSlim), 8u);
    EXPECT_EQ(alignof(CountdownEvent), 8u);
    EXPECT_EQ(alignof(Barrier), 8u);
    EXPECT_EQ(alignof(ThreadLocal<int>), 8u);
}

// The two primitives the layout-neutrality claim rests on. If either stops holding on some
// future target, "the flag change is layout-neutral" stops holding with it.
TEST(ThreadingSharedStateTests, AtomicPrimitives_MatchTheirPlainCounterparts) {
    static_assert(sizeof(std::atomic<bool>) == sizeof(bool));
    static_assert(alignof(std::atomic<bool>) == alignof(bool));
    static_assert(sizeof(std::atomic<SharpRuntime::intcs>) == sizeof(SharpRuntime::intcs));
    static_assert(alignof(std::atomic<SharpRuntime::intcs>) == alignof(SharpRuntime::intcs));
    EXPECT_TRUE(std::atomic<bool>{}.is_lock_free());
    EXPECT_TRUE(std::atomic<SharpRuntime::intcs>{}.is_lock_free());
}

// ===========================================================================
// Each repaired reader still answers correctly
// ===========================================================================

TEST(ThreadingSharedStateTests, SemaphoreSlim_CurrentCountStillTracksWaitAndRelease) {
    SemaphoreSlim sem(2, 4);
    EXPECT_EQ(sem.getCurrentCountProperty(), 2);
    EXPECT_TRUE(sem.Wait(0));
    EXPECT_EQ(sem.getCurrentCountProperty(), 1);
    EXPECT_TRUE(sem.Wait(0));
    EXPECT_EQ(sem.getCurrentCountProperty(), 0);
    EXPECT_FALSE(sem.Wait(0));
    EXPECT_EQ(sem.getCurrentCountProperty(), 0);
    sem.Release(2);
    EXPECT_EQ(sem.getCurrentCountProperty(), 2);
}

// The count is atomic, but every write still happens under the mutex, so the class invariant
// 0 <= count <= maxCount must survive contention.
TEST(ThreadingSharedStateTests, SemaphoreSlim_CurrentCountStaysInRangeUnderContention) {
    SemaphoreSlim sem(2, 2);
    std::atomic<bool> go{false};
    std::atomic<bool> outOfRange{false};

    std::vector<std::thread> workers;
    for (int t = 0; t < 3; ++t) {
        workers.emplace_back([&] {
            while (!go.load()) {
            }
            for (int i = 0; i < 500; ++i) {
                auto seen = sem.getCurrentCountProperty();
                if (seen < 0 || seen > 2) outOfRange.store(true);
                if (sem.Wait(0)) sem.Release();
            }
        });
    }
    go.store(true);
    for (auto& w : workers) w.join();

    EXPECT_FALSE(outOfRange.load());
    EXPECT_EQ(sem.getCurrentCountProperty(), 2);
}

TEST(ThreadingSharedStateTests, Barrier_ParticipantCountStillTracksAddAndRemove) {
    Barrier barrier(1);
    EXPECT_EQ(barrier.getParticipantCountProperty(), 1);
    EXPECT_EQ(barrier.AddParticipant(), 2);
    EXPECT_EQ(barrier.getParticipantCountProperty(), 2);
    barrier.RemoveParticipant();
    EXPECT_EQ(barrier.getParticipantCountProperty(), 1);
}

// The reason Barrier's participant count became an atomic field rather than a locked read.
// FinishPhase() invokes the post-phase action while still holding mutex_, so a property that
// took that lock would block on the lock its own caller holds -- which is SR-AUD-210, the
// defect approval-gated ticket #1957 exists to remove. #1955 must not add a second instance
// of it. This case would hang rather than fail if that regressed, which for a deadlock is the
// only signal available.
TEST(ThreadingSharedStateTests, Barrier_ParticipantCountReadableFromPostPhaseAction) {
    std::atomic<int> seenInAction{-1};
    Barrier barrier(1, [&seenInAction](Barrier& self) {
        seenInAction.store(static_cast<int>(self.getParticipantCountProperty()));
    });

    barrier.SignalAndWait();
    EXPECT_EQ(seenInAction.load(), 1);
    EXPECT_EQ(barrier.getCurrentPhaseNumberProperty(), 1);
}

// ===========================================================================
// Disposal guards still fire, and still only after Dispose
// ===========================================================================

TEST(ThreadingSharedStateTests, DisposalGuards_StillRejectAfterDispose) {
    {
        SemaphoreSlim sem(1, 1);
        sem.Dispose();
        EXPECT_THROW((void)sem.Wait(0), System::ObjectDisposedException);
    }
    {
        ManualResetEventSlim ev(true);
        ev.Dispose();
        EXPECT_THROW(ev.Reset(), System::ObjectDisposedException);
    }
    {
        CountdownEvent ce(1);
        ce.Dispose();
        EXPECT_THROW((void)ce.Signal(), System::ObjectDisposedException);
    }
    {
        ReaderWriterLockSlim lock;
        lock.Dispose();
        EXPECT_THROW((void)lock.TryEnterReadLock(0), System::ObjectDisposedException);
    }
    {
        ThreadLocal<int> tl;
        tl.Dispose();
        EXPECT_THROW((void)tl.getValueProperty(), System::ObjectDisposedException);
    }
    {
        Barrier barrier(1);
        barrier.Dispose();
        EXPECT_THROW(barrier.SignalAndWait(), System::ObjectDisposedException);
    }
}

TEST(ThreadingSharedStateTests, DisposalGuards_DoNotFireBeforeDispose) {
    SemaphoreSlim sem(1, 1);
    EXPECT_NO_THROW((void)sem.Wait(0));

    ManualResetEventSlim ev(true);
    EXPECT_NO_THROW(ev.Reset());

    CountdownEvent ce(1);
    EXPECT_NO_THROW((void)ce.Signal());

    ReaderWriterLockSlim lock;
    EXPECT_NO_THROW({
        if (lock.TryEnterReadLock(0)) lock.ExitReadLock();
    });

    ThreadLocal<int> tl;
    EXPECT_NO_THROW((void)tl.getValueProperty());

    Barrier barrier(1);
    EXPECT_NO_THROW(barrier.SignalAndWait());
}

// A concurrent reader/disposer pair must reach one of exactly two defined outcomes -- the
// operation succeeds, or it throws ObjectDisposedException -- and never anything else. Under
// TSan this same shape is the race reproduction; here it is the behavioural half.
TEST(ThreadingSharedStateTests, ConcurrentDispose_YieldsOnlyDefinedOutcomes) {
    constexpr int kRounds = 200;
    std::atomic<int> unexpected{0};

    for (int round = 0; round < kRounds; ++round) {
        SemaphoreSlim sem(1, 1);
        std::atomic<bool> go{false};
        std::thread user([&] {
            while (!go.load()) {
            }
            try {
                if (sem.Wait(0)) sem.Release();
            } catch (const System::ObjectDisposedException&) {
                // defined outcome
            } catch (const System::Exception&) {
                unexpected.fetch_add(1);
            }
        });
        std::thread disposer([&] {
            while (!go.load()) {
            }
            sem.Dispose();
        });
        go.store(true);
        user.join();
        disposer.join();
    }

    EXPECT_EQ(unexpected.load(), 0);
}

// ===========================================================================
// SR-AUD-216 -- LazyInitializer's guard and result now load through the same atomic_ref that
// publishes the value.
// ===========================================================================

namespace {

    struct Boxed {
        int value = 5;
    };

}  // namespace

TEST(ThreadingSharedStateTests, LazyInitializer_ConcurrentCallersAgreeOnOneInstance) {
    constexpr int kRounds = 200;
    int agreed = 0;

    for (int round = 0; round < kRounds; ++round) {
        Boxed* target = nullptr;
        std::atomic<bool> go{false};
        std::atomic<Boxed*> a{nullptr};
        std::atomic<Boxed*> b{nullptr};

        auto body = [&](std::atomic<Boxed*>& out) {
            while (!go.load()) {
            }
            out.store(&LazyInitializer::EnsureInitialized<Boxed>(
                target, std::function<Boxed*()>([] { return new Boxed(); })));
        };
        std::thread t1([&] { body(a); });
        std::thread t2([&] { body(b); });
        go.store(true);
        t1.join();
        t2.join();

        // Both callers must observe the single published instance -- .NET's documented
        // contract is that the loser's candidate is discarded, not returned.
        if (a.load() == b.load() && a.load() == target && target != nullptr &&
            target->value == 5) {
            ++agreed;
        }
        delete target;
    }

    EXPECT_EQ(agreed, kRounds);
}

TEST(ThreadingSharedStateTests, LazyInitializer_SingleCallerPathsUnchanged) {
    Boxed* defaulted = nullptr;
    Boxed& d = LazyInitializer::EnsureInitialized<Boxed>(defaulted);
    EXPECT_EQ(&d, defaulted);
    EXPECT_EQ(d.value, 5);
    // already initialised: the factory overload must return the same object without invoking
    // the factory at all
    int factoryCalls = 0;
    Boxed& again = LazyInitializer::EnsureInitialized<Boxed>(
        defaulted, std::function<Boxed*()>([&factoryCalls] {
            ++factoryCalls;
            return new Boxed();
        }));
    EXPECT_EQ(&again, defaulted);
    EXPECT_EQ(factoryCalls, 0);
    delete defaulted;
}
