// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#if defined(__EMSCRIPTEN__)
#  include "System/PlatformNotSupportedException.hpp"
#endif

namespace System::Threading {

    using SharpRuntime::intcs;

    /** Delegate type for timer callbacks — counterpart of .NET System.Threading.TimerCallback. */
    using TimerCallback = std::function<void(void*)>;

    /**
     * @brief Provides a mechanism for executing a method at specified intervals on a thread-pool thread.
     *
     * Partial C++ counterpart of .NET System.Threading.Timer.
     * Uses a dedicated std::thread instead of a thread pool.
     *
     * @note Status: Partial — no thread-pool; uses std::thread.
     */
    class Timer {
        // Shared state owned jointly by the Timer and the background thread.
        // Using shared_ptr ensures the State outlives the Timer object even after
        // Dispose() detaches the thread — eliminates the dangling-this UB.
        struct State {
            std::atomic<bool>          running{false};
            std::function<void(void*)> callback;
            void*                      arg     = nullptr;
            // dueTime/period are protected by mtx (not atomic): run() needs to atomically
            // check-and-wait on them together via cv, which a pair of independent atomics can't
            // express without a race between the check and the wait.
            std::mutex                 mtx;
            std::condition_variable    cv;
            intcs                      dueTime = 0;
            intcs                      period  = 0;
        };

        std::shared_ptr<State> state_;
        std::thread            thread_;

    public:
        /**
         * @brief Initializes the timer with the given callback, state, initial delay, and repeat period (in milliseconds).
         * @throws System::ArgumentOutOfRangeException if @p dueTime or @p period is less than -1.
         */
        Timer(std::function<void(void*)> callback, void* state, intcs dueTime, intcs period)
            : state_(std::make_shared<State>())
        {
            System::ArgumentOutOfRangeException::ThrowIfLessThan(dueTime, static_cast<intcs>(-1), "dueTime");
            System::ArgumentOutOfRangeException::ThrowIfLessThan(period, static_cast<intcs>(-1), "period");
#if defined(__EMSCRIPTEN__)
            (void)callback; (void)state; (void)dueTime; (void)period;
            throw System::PlatformNotSupportedException("Timer requires pthreads (not available in Emscripten single-threaded build)");
#else
            state_->callback = std::move(callback);
            state_->arg      = state;
            state_->dueTime  = dueTime;
            state_->period   = period;
            state_->running  = true;
            thread_ = std::thread([s = state_]() { run(s); });
#endif
        }

        /** Destroys the Timer and stops the background thread. */
        ~Timer() { Dispose(); }

        /** Copying is not allowed. */
        Timer(const Timer&) = delete;
        /** Copy assignment is not allowed. */
        Timer& operator=(const Timer&) = delete;

        /**
         * @brief Changes the timer's due time and period. Pass -1 to disable.
         * @throws System::ArgumentOutOfRangeException if @p dueTime or @p period is less than -1.
         */
        void Change(intcs dueTime, intcs period) {
            System::ArgumentOutOfRangeException::ThrowIfLessThan(dueTime, static_cast<intcs>(-1), "dueTime");
            System::ArgumentOutOfRangeException::ThrowIfLessThan(period, static_cast<intcs>(-1), "period");
            {
                std::lock_guard<std::mutex> lock(state_->mtx);
                state_->dueTime = dueTime;
                state_->period  = period;
            }
            state_->cv.notify_all();
        }

        /** Stops the timer and releases the background thread. */
        void Dispose() {
            if (state_) {
                state_->running = false;
                state_->cv.notify_all();
            }
            if (thread_.joinable()) thread_.detach();
        }

    private:
        // Verified against TimerQueueTimer.Change()/TimerQueue.UpdateTimer: a timer whose
        // dueTime is Timeout.Infinite (-1) is never scheduled to fire -- not just when never
        // started, but also when Change(-1, ...) pauses an already-active timer
        // (TimerQueueTimer.Change() removes the timer from the queue whenever the new dueTime
        // is UnsignedInfinite). This port previously only special-cased "dueTime > 0" before
        // the very first wait, so -1 fell through and fired on the first loop iteration just
        // like 0 does, and had no way to represent "paused" once past the first fire.
        static void run(std::shared_ptr<State> s) {
            std::unique_lock<std::mutex> lock(s->mtx);
            while (s->running) {
                while (s->running && s->dueTime == -1) {
                    s->cv.wait(lock);
                }
                if (!s->running) break;
                intcs due = s->dueTime;
                lock.unlock();
                if (due > 0) std::this_thread::sleep_for(std::chrono::milliseconds(due));
                lock.lock();
                if (!s->running || s->dueTime == -1) continue; // paused/disposed while sleeping
                if (s->callback) {
                    lock.unlock();
                    s->callback(s->arg);
                    lock.lock();
                }
                if (s->period <= 0) {
                    // Single-shot: disable further firing until a future Change() re-arms it
                    // (rather than exiting the loop/thread entirely, so a later Change() call
                    // can still bring this timer back to life, matching real .NET).
                    s->dueTime = -1;
                } else {
                    s->dueTime = s->period; // schedule the next cycle using the period
                }
            }
        }
    };

} // namespace System::Threading
