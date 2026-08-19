// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <thread>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/ObjectDisposedException.hpp"
#include "System/Threading/BarrierPostPhaseException.hpp"

namespace System::Threading {

    using SharpRuntime::intcs;
    using SharpRuntime::longcs;

    /**
     * @brief Enables multiple tasks to cooperatively work on an algorithm in parallel through
     * multiple phases.
     *
     * @note Two behaviors verified against Barrier.cs's FinishPhase()/SignalAndWait():
     * (1) real .NET rejects (InvalidOperationException) a call to SignalAndWait/
     * AddParticipant(s)/RemoveParticipant(s) made from within the post-phase action on the same
     * thread, rather than allowing it to reenter; (2) every participant thread -- not just the
     * one that happened to trigger the phase transition -- observes and rethrows a
     * BarrierPostPhaseException if the post-phase action faulted. This port previously had
     * neither: FinishPhase() ran the action while still holding `mutex_` with no reentrancy
     * check, so a reentrant call from within the action self-deadlocked on the non-recursive
     * std::mutex instead of throwing; and only the triggering thread's FinishPhase() call ever
     * saw the caught exception, so every other participant silently resumed as if the phase had
     * completed successfully.
     */
    class Barrier {
        // Ticket #1955 / SR-AUD-212, cause T-A. getParticipantCountProperty() read this
        // outside mutex_ while AddParticipant()/RemoveParticipant() wrote it under the lock --
        // TSan-confirmed undefined behaviour.
        //
        // The repair is an atomic field rather than a locking property, for two reasons.
        // First, .NET does the same: Barrier.ParticipantCount reads the packed
        // `_currentTotalCount` field directly, without taking the barrier's lock. Second, and
        // decisively, taking mutex_ in this property would create a NEW self-deadlock: the
        // post-phase action is invoked by FinishPhase() while it still holds mutex_, so a
        // legal `barrier.getParticipantCountProperty()` inside that action would block on the
        // lock its own caller holds. That is exactly SR-AUD-210 (cause T-E/2, approval-gated
        // ticket #1957) -- which getCurrentPhaseNumberProperty() already suffers -- and #1955
        // must not add a second instance of the defect a later ticket exists to remove.
        //
        // Writers keep writing under mutex_, so the compound invariants with remainingCount_
        // are unaffected. sizeof/alignof(std::atomic<intcs>) == sizeof/alignof(intcs) == 4,
        // so the field is layout-neutral (build-probe/1955_probe1_layout_{before,after}.log).
        std::atomic<intcs> participantCount_;
        intcs remainingCount_;
        // Ticket #1957 / SR-AUD-210, cause T-E/2. This was a plain `longcs` read under mutex_,
        // and FinishPhase() runs the post-phase action while HOLDING mutex_, so a legal
        // `barrier.getCurrentPhaseNumberProperty()` inside that action deadlocked on a
        // non-recursive std::mutex -- exactly the hazard #1955 recorded here when it fixed the
        // sibling property, and it named this one as the remaining case.
        //
        // .NET's own CurrentPhaseNumber is `Volatile.Read(ref _currentPhase)` (Barrier.cs:184-188)
        // -- a lock-free read of a plain field -- so the reference already answers this: the
        // property must not take the barrier's lock. std::atomic<longcs> is 8 bytes and 8-aligned,
        // the same as longcs, so the change is layout-neutral (sizeof(Barrier) is 160 before and
        // after, build-probe/1957_probe2_barrier.cpp) -- the same argument #1955 made for
        // participantCount_ and disposed_.
        //
        // Writers keep writing under mutex_, so every compound invariant with remainingCount_ is
        // unaffected.
        std::atomic<longcs> phaseCount_{0};
        std::function<void(Barrier&)> postPhaseAction_;
        mutable std::mutex mutex_;
        std::condition_variable cv_;
        std::exception_ptr lastPostPhaseException_;
        std::atomic<std::thread::id> actionCallerId_;
        // Ticket #1955 / cause T-A of docs/ThreadingNamespaceReviewPlan.md. This was an
        // ordinary `bool`, written by Dispose() and read by the guard below with no
        // synchronisation between them -- a data race, i.e. undefined behaviour, confirmed by
        // ThreadSanitizer at audit time and again in
        // build-probe/1955_probe1_shared_state_races.cpp. std::atomic<bool> is 1 byte and
        // 1-byte aligned on every supported target, so the change is layout-neutral; measured
        // in build-probe/1955_probe1_layout_{before,after}.log.
        std::atomic<bool> disposed_{false};

        void ThrowIfCalledFromPostPhaseAction() const {
            if (actionCallerId_.load() == std::this_thread::get_id())
                throw System::InvalidOperationException(
                    "This method may not be called from within the postPhaseAction.");
        }

        // Verified against Barrier.cs: SignalAndWait/AddParticipants/RemoveParticipants all
        // call ObjectDisposedException.ThrowIf(_disposed, this) as their first check. This port
        // previously had no disposed_ flag at all -- Dispose() was a true no-op and every method
        // remained fully usable after disposal.
        void ThrowIfDisposed() const {
            if (disposed_.load(std::memory_order_acquire))
                throw System::ObjectDisposedException("Barrier");
        }

    public:
        /** Constructs a Barrier with the specified number of participants and an optional post-phase action. */
        explicit Barrier(intcs participantCount, std::function<void(Barrier&)> postPhaseAction = nullptr)
            : participantCount_(participantCount), remainingCount_(participantCount),
              postPhaseAction_(std::move(postPhaseAction)) {
            if (participantCount < 0 || participantCount > 32767)
                throw System::ArgumentOutOfRangeException("participantCount");
        }

        /** Returns the total number of participants. */
        /**
         * @brief Returns the total number of participants.
         *
         * Read without taking the barrier's lock, matching .NET's own unlocked read and
         * keeping the property callable from inside a post-phase action (#1955, SR-AUD-212).
         */
        [[nodiscard]] intcs  getParticipantCountProperty() const {
            return participantCount_.load(std::memory_order_acquire);
        }
        /**
         * @brief Returns the current phase number.
         *
         * Read without taking the barrier's lock, matching .NET's
         * `Volatile.Read(ref _currentPhase)` (`Barrier.cs:184-188`) and keeping the property
         * callable from inside a post-phase action (#1957, SR-AUD-210). Taking `mutex_` here
         * deadlocked, because `FinishPhase()` invokes that action while holding it.
         *
         * @note **Inside the post-phase action this returns the phase that is ENDING, not the
         * one about to begin.** That is .NET's value, not an accident of ordering: .NET
         * increments in `SetResetEvents`, which its `FinishPhase` calls from the `finally`
         * *after* the action has run (`Barrier.cs:781-816, 834-836`).
         */
        [[nodiscard]] longcs getCurrentPhaseNumberProperty() const {
            return phaseCount_.load(std::memory_order_acquire);
        }

        /**
         * @brief Signals that a participant has reached the barrier and blocks until all participants have arrived.
         * @throws System::ObjectDisposedException if this instance has been disposed.
         * @throws System::InvalidOperationException if called from within the post-phase action.
         * @throws BarrierPostPhaseException if the post-phase action threw during this phase.
         */
        void SignalAndWait() {
            ThrowIfDisposed();
            ThrowIfCalledFromPostPhaseAction();
            std::unique_lock lock(mutex_);
            if (participantCount_ == 0)
                throw System::InvalidOperationException("The barrier has no registered participants.");
            --remainingCount_;
            if (remainingCount_ == 0) {
                FinishPhase(lock); // throws directly if the post-phase action faulted
            } else {
                longcs myPhase = phaseCount_;
                cv_.wait(lock, [this, myPhase]{ return phaseCount_ > myPhase; });
                if (lastPostPhaseException_)
                    throw BarrierPostPhaseException(lastPostPhaseException_);
            }
        }

        /**
         * @brief Notifies the barrier that there will be one additional participant; returns new participant count.
         * @throws System::ObjectDisposedException if this instance has been disposed.
         * @throws System::InvalidOperationException if called from within the post-phase action.
         */
        intcs AddParticipant() {
            ThrowIfDisposed();
            ThrowIfCalledFromPostPhaseAction();
            std::unique_lock lock(mutex_);
            if (participantCount_ >= 32767)
                throw System::InvalidOperationException("Adding the specified number of participants would cause the Barrier's participants count to exceed the maximum allowed.");
            ++participantCount_;
            ++remainingCount_;
            return participantCount_;
        }

        /**
         * @brief Notifies the barrier that there will be one fewer participant.
         * @throws System::ObjectDisposedException if this instance has been disposed.
         * @throws System::InvalidOperationException if called from within the post-phase action.
         */
        void RemoveParticipant() {
            ThrowIfDisposed();
            ThrowIfCalledFromPostPhaseAction();
            std::unique_lock lock(mutex_);
            if (participantCount_ == 0)
                throw System::ArgumentOutOfRangeException("participantCount");
            if (remainingCount_ == 0)
                throw System::InvalidOperationException("The number of participants to remove would result in a negative number of remaining participants for this phase.");
            --participantCount_;
            if (--remainingCount_ == 0) {
                FinishPhase(lock);
            }
        }

        /**
         * @brief Releases resources used by the Barrier.
         * @throws System::InvalidOperationException if called from within the post-phase action.
         * @note Verified against Barrier.cs's Dispose()/Dispose(bool): real .NET checks the same
         * post-phase-action reentrancy guard as SignalAndWait/AddParticipants/RemoveParticipants
         * before disposing, and Dispose(bool) is idempotent. Calling this repeatedly is safe.
         */
        void Dispose() {
            ThrowIfCalledFromPostPhaseAction();
            disposed_.store(true, std::memory_order_release);
        }

    private:
        /**
         * @brief Advances to the next phase, invoking the post-phase action.
         * @throws BarrierPostPhaseException on the calling (triggering) thread if the action
         * threw; other participants blocked in SignalAndWait() observe the same failure via
         * lastPostPhaseException_ once they wake.
         */
        void FinishPhase(std::unique_lock<std::mutex>& lock) {
            // The participant count is reset BEFORE the action, matching .NET, which zeroes the
            // arrival count in SetCurrentTotal and only then calls FinishPhase
            // (Barrier.cs:454-458).
            remainingCount_ = participantCount_;
            if (postPhaseAction_) {
                actionCallerId_.store(std::this_thread::get_id());
                try {
                    postPhaseAction_(*this);
                    lastPostPhaseException_ = nullptr;
                } catch (...) {
                    lastPostPhaseException_ = std::current_exception();
                }
                actionCallerId_.store(std::thread::id());
            } else {
                lastPostPhaseException_ = nullptr;
            }

            // #1957 MOVED THIS, and the move is what makes the now-reachable property answer
            // what .NET answers. .NET increments the phase in SetResetEvents, which FinishPhase
            // calls from its `finally` -- AFTER the action, and on the throwing path too
            // (Barrier.cs:804-812, 834-836). So inside the action the phase is the one ENDING.
            //
            // The increment used to run first, which was unobservable only because the property
            // that would have seen it deadlocked. Fixing the deadlock without moving it would
            // have shipped a newly reachable wrong answer instead of a hang.
            //
            // Nothing outside the action can see the difference: mutex_ is held for this whole
            // function, so no waiter can run until it is released.
            ++phaseCount_;
            cv_.notify_all();
            (void)lock;
            if (lastPostPhaseException_)
                throw BarrierPostPhaseException(lastPostPhaseException_);
        }
    };

} // namespace System::Threading
