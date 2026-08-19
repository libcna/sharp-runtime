// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <mutex>
#include <atomic>
#include <condition_variable>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ObjectDisposedException.hpp"
#include "System/Threading/WaitHandle.hpp"

namespace System::Threading {

    using SharpRuntime::intcs;

    /**
     * Represents a thread synchronization event that, when signaled, must be reset manually.
     * 
     * Wraps std::condition_variable. Partial C++ counterpart of .NET System.Threading.ManualResetEvent.
     * 
     * @note Status: Implemented
     */
    class ManualResetEvent {
        std::mutex              mutex_;
        std::condition_variable cv_;
        bool                    signaled_;
        // Ticket #1956 / cause T-G (SR-AUD-208). Close() was an EMPTY BODY, so a closed handle
        // stayed fully usable: measured, Close() followed by WaitOne(0) returned success. .NET's
        // WaitHandle.Close() is `=> Dispose()`, Dispose(bool) is `_waitHandle?.Close()`, and every
        // wait path then opens with `ObjectDisposedException.ThrowIf(waitHandle is null, this)`
        // (WaitHandle.cs:87-98, 118). The header here already CLAIMED Close "closes the handle",
        // so the documentation and the behaviour disagreed.
        //
        // std::atomic<bool> is 1 byte and 1-byte aligned, and it lands in padding these types
        // already had -- the sizes are unchanged, pinned by a layout test. Landed under SA-5 (a
        // call that succeeds today starts throwing) with SA-3's layout condition discharged.
        std::atomic<bool> closed_{false};

        void ThrowIfClosed() const {
            if (closed_.load(std::memory_order_acquire))
                throw System::ObjectDisposedException("The handle has been closed.");
        }
    public:
        /** @param initialState If true, the event starts in the signaled state. */
        explicit ManualResetEvent(bool initialState = false) : signaled_(initialState) {}

        /** Sets the event to signaled, releasing all waiting threads. */
        void Set() {
            ThrowIfClosed();
            { std::lock_guard<std::mutex> lk(mutex_); signaled_ = true; }
            cv_.notify_all();
        }

        /** Resets the event to non-signaled. */
        void Reset() {
            ThrowIfClosed();
            std::lock_guard<std::mutex> lk(mutex_);
            signaled_ = false;
        }

        /** Blocks until the event is signaled. */
        void WaitOne() {
            ThrowIfClosed();
            std::unique_lock<std::mutex> lk(mutex_);
            cv_.wait(lk, [this]{ return signaled_; });
        }

        /**
         * Blocks until the event is signaled or the timeout elapses.
         * @param milliseconds Maximum time to wait.
         * @return True if the event was signaled before the timeout.
         * @throws System::ArgumentOutOfRangeException if @p milliseconds is less than -1.
         */
        bool WaitOne(intcs milliseconds) {
            ThrowIfClosed();
            WaitHandle::ValidateTimeout(milliseconds);
            std::unique_lock<std::mutex> lk(mutex_);
            // -1 (Timeout.Infinite) waits indefinitely; std::chrono's wait_for treats a
            // negative duration as already-expired, so it must be special-cased.
            if (milliseconds == -1) {
                cv_.wait(lk, [this]{ return signaled_; });
                return true;
            }
            return cv_.wait_for(lk, std::chrono::milliseconds(milliseconds), [this]{ return signaled_; });
        }

        /**
         * @brief Closes the handle. Every later Set, Reset or WaitOne throws
         *        System::ObjectDisposedException.
         *
         * Idempotent, as .NET's is: Close() is `=> Dispose()` and disposing twice is defined.
         */
        void Close() { closed_.store(true, std::memory_order_release); }
    };

} // namespace System::Threading
