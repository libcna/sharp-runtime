// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <mutex>
#include <string>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
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
    public:
        /** Constructs an unowned Mutex. */
        Mutex() = default;
        /** Constructs a Mutex, optionally owned by the calling thread immediately. */
        explicit Mutex(bool initiallyOwned) { if (initiallyOwned) mutex_.lock(); }
        /** Constructs a named Mutex (naming is not supported; the name parameter is ignored). */
        Mutex(bool initiallyOwned, const std::string& /*name*/) { if (initiallyOwned) mutex_.lock(); }

        /** Acquires the mutex, blocking until it is available. */
        bool WaitOne() override { mutex_.lock(); return true; }
        /** Blocks until the mutex is available or millisecondsTimeout elapses; returns true on success. */
        bool WaitOne(intcs millisecondsTimeout) override {
            return mutex_.try_lock_for(std::chrono::milliseconds(millisecondsTimeout));
        }
        /** Releases the mutex. Throws SynchronizationLockException-equivalent behavior is not enforced. */
        void ReleaseMutex() { mutex_.unlock(); }
        /** Closes the mutex handle. */
        void Close() {}
    };

} // namespace System::Threading
