// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <atomic>
#include <chrono>
#include <thread>
#include "System/IDisposable.hpp"
#include "System/TimeSpan.hpp"

namespace System::Threading {

    /** A timer that fires at a fixed interval; WaitForNextTick blocks until the next tick or disposal. */
    class PeriodicTimer : public System::IDisposable {
        std::chrono::milliseconds period_;
        std::chrono::steady_clock::time_point next_;
        std::atomic<bool> disposed_{false};

    public:
        /** Constructs a PeriodicTimer with the specified period. */
        explicit PeriodicTimer(const System::TimeSpan& period)
            : period_(static_cast<long long>(period.getTotalMillisecondsProperty())),
              next_(std::chrono::steady_clock::now() + period_) {}

        /** Blocks until the next scheduled tick; returns false if the timer has been disposed. */
        bool WaitForNextTick() {
            if (disposed_.load()) return false;
            auto now = std::chrono::steady_clock::now();
            if (next_ > now)
                std::this_thread::sleep_until(next_);
            next_ += period_;
            return !disposed_.load();
        }

        /** Stops the timer so that WaitForNextTick returns false. */
        void Dispose() override {
            disposed_.store(true);
        }
    };

} // namespace System::Threading
