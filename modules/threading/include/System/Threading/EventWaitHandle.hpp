// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <atomic>
#include <condition_variable>
#include <mutex>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/Threading/WaitHandle.hpp"
#include "System/Threading/EventResetMode.hpp"

namespace System::Threading {

    using SharpRuntime::intcs;

    /** Represents a thread-synchronisation event. */
    class EventWaitHandle : public WaitHandle {
        EventResetMode mode_;
        std::atomic<bool> set_{false};
        std::mutex mtx_;
        std::condition_variable cv_;

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

        /** Sets the event to the signalled state. */
        void Set() {
            set_.store(true, std::memory_order_release);
            if (mode_ == EventResetMode::ManualReset)
                cv_.notify_all();
            else
                cv_.notify_one();
        }

        /** Sets the event to the non-signalled state. */
        void Reset() { set_.store(false, std::memory_order_release); }

        /** Blocks until the event is signalled; auto-resets if the mode is AutoReset. */
        bool WaitOne() override {
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
