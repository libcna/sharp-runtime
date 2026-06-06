// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <condition_variable>
#include <mutex>
#include <stdexcept>

namespace System::Threading {

    class CountdownEvent {
        int initialCount_;
        int currentCount_;
        mutable std::mutex mutex_;
        std::condition_variable cv_;

    public:
        explicit CountdownEvent(int initialCount) : initialCount_(initialCount), currentCount_(initialCount) {
            if (initialCount < 0) throw std::invalid_argument("initialCount must be >= 0.");
        }

        [[nodiscard]] int  getCurrentCountProperty()   const { std::unique_lock lock(mutex_); return currentCount_; }
        [[nodiscard]] int  getInitialCountProperty()   const { return initialCount_; }
        [[nodiscard]] bool getIsSetProperty()          const { std::unique_lock lock(mutex_); return currentCount_ == 0; }

        bool Signal(int signalCount = 1) {
            std::unique_lock lock(mutex_);
            if (currentCount_ == 0) throw std::invalid_argument("CountdownEvent has already been set.");
            currentCount_ -= signalCount;
            if (currentCount_ < 0) currentCount_ = 0;
            bool set = (currentCount_ == 0);
            if (set) cv_.notify_all();
            return set;
        }

        void AddCount(int signalCount = 1) {
            std::unique_lock lock(mutex_);
            if (currentCount_ == 0) throw std::invalid_argument("CountdownEvent is already set.");
            currentCount_ += signalCount;
        }

        void Reset(int count = -1) {
            std::unique_lock lock(mutex_);
            if (count < 0) currentCount_ = initialCount_;
            else { initialCount_ = count; currentCount_ = count; }
        }

        void Wait() {
            std::unique_lock lock(mutex_);
            cv_.wait(lock, [this]{ return currentCount_ == 0; });
        }

        bool Wait(int millisecondsTimeout) {
            std::unique_lock lock(mutex_);
            return cv_.wait_for(lock, std::chrono::milliseconds(millisecondsTimeout),
                                [this]{ return currentCount_ == 0; });
        }

        void Dispose() {}
    };

} // namespace System::Threading
