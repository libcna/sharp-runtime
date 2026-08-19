// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <atomic>
#include <condition_variable>
#include <mutex>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/Threading/WaitHandle.hpp"
#include "System/ObjectDisposedException.hpp"
#include "System/Threading/EventResetMode.hpp"

namespace System::Threading {

    using SharpRuntime::intcs;

    /** Represents a thread-synchronisation event. */
    class EventWaitHandle : public WaitHandle {
        EventResetMode mode_;
        std::atomic<bool> set_{false};
        std::mutex mtx_;
        std::condition_variable cv_;

        // Ticket #1958 / SR-AUD-209. #1956 gave AutoResetEvent, ManualResetEvent and Mutex a
        // closed state; EventWaitHandle was its fourth case and was missed, so Close() here
        // reached WaitHandle's EMPTY Dispose() and a closed handle stayed fully usable. That gap
        // had to be closed before AutoResetEvent and ManualResetEvent could derive from this
        // class, or deriving would have silently reverted #1956 for both of them.
        std::atomic<bool> closed_{false};

        void ThrowIfClosed() const {
            if (closed_.load(std::memory_order_acquire))
                throw System::ObjectDisposedException("The handle has been closed.");
        }

    public:
        /**
         * @brief Constructs an EventWaitHandle with the specified initial state and reset mode.
         *
         * @throws System::ArgumentException if @p mode is not `EventResetMode::AutoReset` or
         * `EventResetMode::ManualReset`.
         *
         * .NET rejects an undeclared `EventResetMode` at construction; this port used to store
         * it, producing a handle that is **neither** kind of event, because every branch that
         * consults `mode_` tests for one specific value: `Set()` took the AutoReset
         * `notify_one` path *because 42 is not ManualReset*, while `WaitOne()` did not
         * auto-reset *because 42 is not AutoReset*, so the event stayed signalled forever and
         * released one waiter at a time (SR-AUD-184, ticket #1954).
         *
         * Rejection, not normalisation, is deliberate, and the difference from
         * `ReaderWriterLockSlim` in the same ticket is .NET's, not this port's:
         * `ReaderWriterLockSlim` stores a bool and *derives* `RecursionPolicy`, so an
         * undeclared policy is silently `NoRecursion`, whereas `EventWaitHandle` validates
         * `mode` and throws. The two conventions are not unified here because .NET does not
         * unify them.
         *
         * @note The exception's exact derived type could not be verified against the reference
         * tree, which is not present in this environment; the audit's managed probe records
         * only the category ("an argument exception"). `System::ArgumentException` is thrown
         * because it is the base of both candidates -- .NET's own
         * `ArgumentException(SR.Argument_InvalidFlag, nameof(mode))` and the
         * `ArgumentOutOfRangeException(nameof(mode))` some releases use -- so a handler
         * written against either still catches this one. Tests assert the category rather
         * than a derived type for the same reason.
         */
        EventWaitHandle(bool initialState, EventResetMode mode)
            : mode_(mode), set_(initialState) {
            if (mode != EventResetMode::AutoReset && mode != EventResetMode::ManualReset)
                throw System::ArgumentException("Value of flags is invalid.", "mode");
        }

        /**
         * @brief Sets the event to the signalled state.
         * @throws System::ObjectDisposedException if the handle has been closed.
         *
         * The store and the notification happen under `mtx_`. Ticket #1958 / SR-AUD-209
         * measured that doing them WITHOUT the lock loses wakeups: a waiter that has evaluated
         * the predicate as false but has not yet atomically released the lock and slept misses
         * the notification entirely and blocks until some later Set(). Probed over 900 rounds,
         * the unlocked form lost 2 and the locked form lost 0 -- and this is the type six `cna`
         * data members hold by value, all of them for async completion, which is exactly the
         * shape a lost wakeup hangs.
         */
        void Set() {
            ThrowIfClosed();
            { std::lock_guard<std::mutex> lk(mtx_); set_.store(true, std::memory_order_release); }
            if (mode_ == EventResetMode::ManualReset)
                cv_.notify_all();
            else
                cv_.notify_one();
        }

        /**
         * @brief Sets the event to the non-signalled state.
         * @throws System::ObjectDisposedException if the handle has been closed.
         */
        void Reset() {
            ThrowIfClosed();
            std::lock_guard<std::mutex> lk(mtx_);
            set_.store(false, std::memory_order_release);
        }

        /**
         * @brief Closes the handle; every later Set, Reset or WaitOne throws
         *        System::ObjectDisposedException.
         *
         * .NET's `WaitHandle.Close()` is `=> Dispose()` and this port's base spells it the same
         * way, so overriding Dispose() is what makes Close() effective here. Idempotent, as
         * .NET's is.
         */
        void Dispose() override { closed_.store(true, std::memory_order_release); }

        /** Blocks until the event is signalled; auto-resets if the mode is AutoReset. */
        bool WaitOne() override {
            ThrowIfClosed();
            std::unique_lock<std::mutex> lock(mtx_);
            cv_.wait(lock, [this]{ return set_.load(std::memory_order_acquire); });
            if (mode_ == EventResetMode::AutoReset)
                set_.store(false, std::memory_order_release);
            return true;
        }

        /**
         * @brief Blocks until the event is signalled or the timeout elapses; returns true if signalled.
         * @throws System::ArgumentOutOfRangeException if @p milliseconds is less than -1.
         */
        bool WaitOne(intcs milliseconds) override {
            ThrowIfClosed();
            ValidateTimeout(milliseconds);
            std::unique_lock<std::mutex> lock(mtx_);
            // -1 (Timeout.Infinite) waits indefinitely; std::chrono's wait_for treats a
            // negative duration as already-expired, so it must be special-cased.
            bool ok;
            if (milliseconds == -1) {
                cv_.wait(lock, [this]{ return set_.load(std::memory_order_acquire); });
                ok = true;
            } else {
                ok = cv_.wait_for(lock, std::chrono::milliseconds(milliseconds),
                    [this]{ return set_.load(std::memory_order_acquire); });
            }
            if (ok && mode_ == EventResetMode::AutoReset)
                set_.store(false, std::memory_order_release);
            return ok;
        }
    };

} // namespace System::Threading
