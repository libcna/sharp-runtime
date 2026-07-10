// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
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
            std::atomic<intcs>         dueTime{0};
            std::atomic<intcs>         period{0};
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

        /** @brief Changes the timer's due time and period. Pass -1 to disable. */
        void Change(intcs dueTime, intcs period) {
            state_->dueTime = dueTime;
            state_->period  = period;
        }

        /** Stops the timer and releases the background thread. */
        void Dispose() {
            if (state_) state_->running = false;
            if (thread_.joinable()) thread_.detach();
        }

    private:
        static void run(std::shared_ptr<State> s) {
            if (s->dueTime > 0)
                std::this_thread::sleep_for(std::chrono::milliseconds(s->dueTime.load()));
            while (s->running) {
                if (s->callback) s->callback(s->arg);
                if (s->period <= 0) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(s->period.load()));
            }
        }
    };

} // namespace System::Threading
