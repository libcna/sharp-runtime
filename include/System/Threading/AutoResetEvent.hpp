// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <mutex>
#include <condition_variable>

namespace System::Threading {

    /**
     * @brief Represents a thread synchronization event that resets automatically after releasing a single waiting thread.
     *
     * Wraps std::condition_variable. Partial C++ counterpart of .NET System.Threading.AutoResetEvent.
     *
     * @note Status: Implemented
     */
    class AutoResetEvent {
        std::mutex              mutex_;
        std::condition_variable cv_;
        bool                    signaled_;
    public:
        explicit AutoResetEvent(bool initialState = false) : signaled_(initialState) {}

        /** @brief Sets the event to signaled, releasing one waiting thread, then resets automatically. */
        void Set() {
            { std::lock_guard<std::mutex> lk(mutex_); signaled_ = true; }
            cv_.notify_one();
        }

        /** @brief Resets the event to non-signaled. */
        void Reset() {
            std::lock_guard<std::mutex> lk(mutex_);
            signaled_ = false;
        }

        /** @brief Blocks until the event is signaled (then auto-resets). */
        void WaitOne() {
            std::unique_lock<std::mutex> lk(mutex_);
            cv_.wait(lk, [this]{ return signaled_; });
            signaled_ = false;
        }

        /** @brief Blocks until signaled or timeout. Returns true if signaled (then auto-resets). */
        bool WaitOne(int milliseconds) {
            std::unique_lock<std::mutex> lk(mutex_);
            bool ok = cv_.wait_for(lk, std::chrono::milliseconds(milliseconds), [this]{ return signaled_; });
            if (ok) signaled_ = false;
            return ok;
        }

        void Close() {}
    };

} // namespace System::Threading
