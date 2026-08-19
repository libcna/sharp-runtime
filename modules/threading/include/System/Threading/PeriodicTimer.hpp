// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/IDisposable.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/Threading/Timeout.hpp"
#include "System/TimeSpan.hpp"

namespace System::Threading {

    /** A timer that fires at a fixed interval; WaitForNextTick blocks until the next tick or disposal. */
    class PeriodicTimer : public System::IDisposable {
        // Matches Timer.cs's Timer.MaxSupportedTimeout (0xfffffffe == uint.MaxValue - 1).
        static constexpr long long MaxSupportedTimeoutMs = 0xFFFFFFFEULL;

        bool infinite_;
        std::chrono::milliseconds period_;
        std::chrono::steady_clock::time_point next_;
        std::mutex mtx_;
        std::condition_variable cv_;
        std::atomic<bool> disposed_{false};

        // Ticket #1957 / SR-AUD-201. .NET's State carries `private bool _activeWait`
        // (PeriodicTimer.cs:192) for exactly this, and its WaitForNextTickAsync opens by
        // testing it and throwing (PeriodicTimer.cs:199-203) under the comment
        // "WaitForNextTickAsync should only be used by one consumer at a time. Failing to do
        // so is an error."
        //
        // Without it, two concurrent waiters both returned true for ONE tick -- the audit's
        // probe measured `concurrent=1,1` -- so a caller that accidentally shared a timer got
        // twice the intended work rate with no diagnostic at all.
        bool activeWait_ = false;

    public:
        /**
         * @brief Constructs a PeriodicTimer with the specified period.
         *
         * @throws System::ArgumentOutOfRangeException unless @p period is
         * Timeout::InfiniteTimeSpan or its millisecond count, **truncated towards zero**,
         * lies in [1, uint.MaxValue - 1].
         *
         * @note A **fractional** period is accepted and truncated, not rejected. Verified
         * against `PeriodicTimer.TryGetMilliseconds` (PeriodicTimer.cs:110-121):
         *
         *     long ms = (long)value.TotalMilliseconds;
         *     if ((ms >= 1 && ms <= Timer.MaxSupportedTimeout) || value == Timeout.InfiniteTimeSpan)
         *
         * The cast is truncating and happens **before** the range test, so .NET schedules
         * `TimeSpan::FromMilliseconds(1.5)` as 1 ms. SR-AUD-200 concluded that such a period
         * "must be rejected"; ticket #1954 declined to implement that on the reading that .NET
         * truncates, and #1963 has now confirmed the reading from the reference. Rejecting
         * would be a narrowing **away** from .NET, refusing input that `TimeSpan::FromTicks`
         * arithmetic produces easily.
         *
         * @note The truncation is also why this port compares the **truncated** count rather
         * than the double: testing `ms <= MaxSupportedTimeoutMs` on the double rejected a
         * period whose fractional part pushed it past the ceiling but whose truncated value
         * did not -- `MaxSupportedTimeoutMs + 0.5` threw here and is accepted by .NET.
         *
         * @note This port previously performed no validation at all -- TimeSpan::Zero or a
         * negative (non-Infinite) period silently constructed a timer whose next_ was already
         * <= now(), so WaitForNextTick() would busy-loop at 100% CPU (sleep_until a point
         * already in the past returns immediately) instead of throwing a clear error.
         */
        explicit PeriodicTimer(const System::TimeSpan& period)
            : infinite_(period.getTicksProperty() == System::Threading::Timeout::InfiniteTimeSpan) {
            // TimeSpan's ticks are a 64-bit count, so TotalMilliseconds cannot exceed ~9.2e14
            // and the cast is always in range for long long.
            const long long ms = static_cast<long long>(period.getTotalMillisecondsProperty());
            if (!infinite_ && !(ms >= 1 && ms <= MaxSupportedTimeoutMs))
                throw System::ArgumentOutOfRangeException("period");
            period_ = std::chrono::milliseconds(ms);
            next_ = std::chrono::steady_clock::now() + period_;
        }

        /**
         * @brief Blocks until the next scheduled tick; returns false if the timer has been disposed.
         * @note A timer constructed with Timeout::InfiniteTimeSpan never ticks on its own --
         * this blocks until Dispose() is called, matching real .NET (a Timeout.InfiniteTimeSpan
         * PeriodicTimer never schedules a tick, but Dispose() still unblocks a pending wait via
         * State.Signal(stopping: true)).
         */
        bool WaitForNextTick() {
            std::unique_lock<std::mutex> lock(mtx_);

            // FIRST, before every other test, because that is where .NET puts it: the
            // _activeWait check precedes both the cancellation short-circuit and the
            // already-signalled fast path (PeriodicTimer.cs:197-213). So a second consumer
            // arriving while the first waits is refused even if the timer has since been
            // disposed -- it is the CONCURRENT USE that is the error, not the timer's state.
            //
            // NO TEST FORCES THIS ORDERING, and that is recorded rather than hidden: moving the
            // check below the disposed test changes the answer only when a second consumer
            // arrives after Dispose() but before the parked first consumer reacquires the mutex
            // and clears the flag. Dispose() releases the mutex before notify_all, so which of
            // the two acquires it next is unspecified -- a test for it would be flaky, and a
            // gate that is intermittently green is not evidence (#2352, #2166). The ordering is
            // here because it is .NET's.
            if (activeWait_) {
                throw System::InvalidOperationException(
                    "WaitForNextTick should only be used by one consumer at a time.");
            }

            if (disposed_.load()) return false;

            // Cleared on EVERY exit -- the ordinary returns, the disposed return, and any
            // exception -- so a caller that abandons a wait does not lock the timer out
            // permanently. .NET clears it in the completion path (PeriodicTimer.cs:296).
            activeWait_ = true;
            struct ActiveWaitGuard {
                bool& flag;
                ~ActiveWaitGuard() { flag = false; }
            } guard{activeWait_};

            if (infinite_) {
                cv_.wait(lock, [this] { return disposed_.load(); });
                return false;
            }
            cv_.wait_until(lock, next_, [this] { return disposed_.load(); });
            if (disposed_.load()) return false;
            next_ += period_;
            return true;
        }

        /** Stops the timer so that WaitForNextTick returns false, unblocking any in-progress wait. */
        void Dispose() override {
            {
                std::lock_guard<std::mutex> lock(mtx_);
                disposed_.store(true);
            }
            cv_.notify_all();
        }
    };

} // namespace System::Threading
