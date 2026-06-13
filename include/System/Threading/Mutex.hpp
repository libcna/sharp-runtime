// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <mutex>
#include <string>

namespace System::Threading {

    /**
     * @brief A synchronization primitive that can be used for inter-thread and inter-process synchronization.
     *
     * Wraps std::mutex. Partial C++ counterpart of .NET System.Threading.Mutex.
     *
     * @note Status: Partial — inter-process (named) mutex not supported.
     */
    class Mutex {
        std::mutex mutex_;
    public:
        /// Constructs an unowned Mutex.
        Mutex() = default;
        /// Constructs a Mutex, optionally initially owned; the initiallyOwned parameter is ignored.
        explicit Mutex(bool /*initiallyOwned*/) {}
        /// Constructs a named Mutex (naming is not supported; the name parameter is ignored).
        Mutex(bool /*initiallyOwned*/, const std::string& /*name*/) {}

        /// Acquires the mutex, blocking until it is available.
        void WaitOne() { mutex_.lock(); }
        /// Attempts to acquire the mutex without blocking; returns true if acquired.
        bool WaitOne(int /*millisecondsTimeout*/) { return mutex_.try_lock(); }
        /// Releases the mutex.
        void ReleaseMutex() { mutex_.unlock(); }
        /// Closes the mutex handle.
        void Close() {}
    };

} // namespace System::Threading
