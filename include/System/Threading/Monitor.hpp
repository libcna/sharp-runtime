// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <mutex>

namespace System::Threading {

    /**
     * @brief Provides a mechanism that synchronizes access to objects.
     *
     * Static-only stub wrapping std::mutex concepts.
     * Partial C++ counterpart of .NET System.Threading.Monitor.
     *
     * @note Status: Stub — Enter/Exit/TryEnter are no-ops (full impl requires per-object mutex map)
     */
    class Monitor {
    public:
        /** Prevents instantiation — all members are static. */
        Monitor() = delete;

        /** Acquires an exclusive lock on obj (no-op stub). */
        static void Enter(void*) {}
        /** Releases an exclusive lock on obj (no-op stub). */
        static void Exit(void*)  {}
        /** Attempts to acquire an exclusive lock on obj; always returns true in this stub. */
        static bool TryEnter(void*) { return true; }

        /** Acquires a lock on obj and sets lockTaken to true. */
        static void Enter(void*, bool& lockTaken) { lockTaken = true; }
        /** Attempts to acquire a lock within the specified timeout; always returns true in this stub. */
        static bool TryEnter(void*, int) { return true; }

        /** Releases the lock on obj and blocks until reacquired (no-op stub). */
        static void Wait(void*)   {}
        /** Notifies a thread in the waiting queue to proceed (no-op stub). */
        static void Pulse(void*)  {}
        /** Notifies all threads in the waiting queue to proceed (no-op stub). */
        static void PulseAll(void*) {}
    };

} // namespace System::Threading
