// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <atomic>
#include <chrono>
#include <functional>
#include <thread>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::Threading {

    using SharpRuntime::intcs;

    /**
     * @brief Provides a mechanism for executing a method at specified intervals on a thread-pool thread.
     *
     * Partial C++ counterpart of .NET System.Threading.Timer.
     * Uses a dedicated std::thread instead of a thread pool.
     *
     * @note Status: Partial — no thread-pool; uses std::thread.
     */
    class Timer {
        std::function<void(void*)> callback_;
        void*                      state_    = nullptr;
        intcs                      dueTime_  = 0;
        intcs                      period_   = 0;
        std::atomic<bool>          running_  = false;
        std::thread                thread_;

    public:
        /**
         * @param callback   Called on each tick. Receives the state object.
         * @param state      Object passed to callback on each invocation.
         * @param dueTime    Delay before first invocation, in milliseconds.
         * @param period     Interval between invocations, in milliseconds. 0 = fire once.
         */
        Timer(std::function<void(void*)> callback, void* state, intcs dueTime, intcs period)
            : callback_(std::move(callback)), state_(state), dueTime_(dueTime), period_(period)
        {
            running_ = true;
            thread_ = std::thread([this]() { run(); });
        }

        ~Timer() { Dispose(); }

        Timer(const Timer&) = delete;
        Timer& operator=(const Timer&) = delete;

        /** @brief Changes the timer's due time and period. Pass -1 to disable. */
        void Change(intcs dueTime, intcs period) {
            dueTime_ = dueTime;
            period_  = period;
        }

        void Dispose() {
            running_ = false;
            if (thread_.joinable()) thread_.detach();
        }

    private:
        void run() {
            if (dueTime_ > 0) std::this_thread::sleep_for(std::chrono::milliseconds(dueTime_));
            while (running_) {
                if (callback_) callback_(state_);
                if (period_ <= 0) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(period_));
            }
        }
    };

} // namespace System::Threading
