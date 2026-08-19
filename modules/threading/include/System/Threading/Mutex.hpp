// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <atomic>
#include <mutex>
#include <string>
#include <thread>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ApplicationException.hpp"
#include "System/ObjectDisposedException.hpp"
#include "System/Threading/WaitHandle.hpp"

namespace System::Threading {

    using SharpRuntime::intcs;

    /**
     * @brief A synchronization primitive that can be used for inter-thread and inter-process synchronization.
     *
     * Wraps std::recursive_timed_mutex (recursive, matching .NET's re-entrant Mutex semantics for the
     * owning thread). Partial C++ counterpart of .NET System.Threading.Mutex.
     *
     * @note Status: Partial — inter-process (named) mutex and abandoned-mutex detection are not supported;
     * the name parameter is accepted but ignored, and this Mutex only synchronizes threads within the
     * current process.
     */
    class Mutex : public WaitHandle {
        std::recursive_timed_mutex mutex_;
        std::atomic<std::thread::id> owner_{};
        std::atomic<int> depth_{0};
        // Ticket #1956 / cause T-G (SR-AUD-208). Close() was an EMPTY BODY, so a closed mutex
        // stayed fully usable: measured, Close() then WaitOne(0) returned success. .NET's
        // WaitHandle.Close() is `=> Dispose()` (WaitHandle.cs:87), Dispose(bool) closes the
        // SafeWaitHandle, and every wait path then throws ObjectDisposedException
        // (WaitHandle.cs:118). The header here already CLAIMED Close "closes the mutex handle".
        std::atomic<bool> closed_{false};

        void ThrowIfClosed() const {
            if (closed_.load(std::memory_order_acquire))
                throw System::ObjectDisposedException("The handle has been closed.");
        }

        void onAcquired() {
            if (depth_.fetch_add(1, std::memory_order_relaxed) == 0)
                owner_.store(std::this_thread::get_id(), std::memory_order_relaxed);
        }
        void onReleasing() {
            if (depth_.fetch_sub(1, std::memory_order_relaxed) == 1)
                owner_.store(std::thread::id(), std::memory_order_relaxed);
        }

    public:
        /** Constructs an unowned Mutex. */
        Mutex() = default;
        /** Constructs a Mutex, optionally owned by the calling thread immediately. */
        explicit Mutex(bool initiallyOwned) { if (initiallyOwned) { mutex_.lock(); onAcquired(); } }
        /** Constructs a named Mutex (naming is not supported; the name parameter is ignored). */
        Mutex(bool initiallyOwned, const std::string& /*name*/) { if (initiallyOwned) { mutex_.lock(); onAcquired(); } }

        /** Acquires the mutex, blocking until it is available. */
        bool WaitOne() override { ThrowIfClosed(); mutex_.lock(); onAcquired(); return true; }
        /**
         * @brief Blocks until the mutex is available or millisecondsTimeout elapses; returns true on success.
         * @throws System::ArgumentOutOfRangeException if @p millisecondsTimeout is less than -1.
         */
        bool WaitOne(intcs millisecondsTimeout) override {
            ThrowIfClosed();
            ValidateTimeout(millisecondsTimeout);
            // -1 (Timeout.Infinite) waits indefinitely; std::chrono's try_lock_for treats a
            // negative duration as already-expired, so it must be special-cased rather than
            // passed straight through.
            bool acquired;
            if (millisecondsTimeout == -1) {
                mutex_.lock();
                acquired = true;
            } else {
                acquired = mutex_.try_lock_for(std::chrono::milliseconds(millisecondsTimeout));
            }
            if (acquired) onAcquired();
            return acquired;
        }
        /**
         * @brief Releases the mutex.
         * @throws System::ApplicationException if the calling thread does not own the mutex.
         */
        void ReleaseMutex() {
            ThrowIfClosed();
            if (owner_.load(std::memory_order_relaxed) != std::this_thread::get_id())
                throw System::ApplicationException("Object synchronization method was called from an unsynchronized block of code.");
            onReleasing();
            mutex_.unlock();
        }
        /**
         * @brief Closes the mutex handle. Every later WaitOne or ReleaseMutex throws
         *        System::ObjectDisposedException.
         *
         * Overrides `Dispose()` rather than shadowing `Close()`, so the inherited
         * `WaitHandle::Close()` -- which is `{ Dispose(); }` -- reaches it. That is .NET's own
         * arrangement: `public virtual void Close() => Dispose();` (WaitHandle.cs:87). The
         * spelling `m.Close()` is unaffected.
         *
         * Idempotent, as .NET's is.
         */
        void Dispose() override { closed_.store(true, std::memory_order_release); }
    };

} // namespace System::Threading
