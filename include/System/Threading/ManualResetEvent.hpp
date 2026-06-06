// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <mutex>
#include <condition_variable>

namespace System::Threading {

    /**
     * @brief Represents a thread synchronization event that, when signaled, must be reset manually.
     *
     * Wraps std::condition_variable. Partial C++ counterpart of .NET System.Threading.ManualResetEvent.
     *
     * @note Status: Implemented
     */
    class ManualResetEvent {
        std::mutex              mutex_;
        std::condition_variable cv_;
        bool                    signaled_;
    public:
        explicit ManualResetEvent(bool initialState = false) : signaled_(initialState) {}

        /** @brief Sets the event to signaled, releasing all waiting threads. */
        void Set() {
            { std::lock_guard<std::mutex> lk(mutex_); signaled_ = true; }
            cv_.notify_all();
        }

        /** @brief Resets the event to non-signaled. */
        void Reset() {
            std::lock_guard<std::mutex> lk(mutex_);
            signaled_ = false;
        }

        /** @brief Blocks until the event is signaled. */
        void WaitOne() {
            std::unique_lock<std::mutex> lk(mutex_);
            cv_.wait(lk, [this]{ return signaled_; });
        }

        /** @brief Blocks until the event is signaled or timeout elapses. Returns true if signaled. */
        bool WaitOne(int milliseconds) {
            std::unique_lock<std::mutex> lk(mutex_);
            return cv_.wait_for(lk, std::chrono::milliseconds(milliseconds), [this]{ return signaled_; });
        }

        void Close() {}
    };

} // namespace System::Threading
