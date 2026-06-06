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
        Monitor() = delete;

        static void Enter(void*) {}
        static void Exit(void*)  {}
        static bool TryEnter(void*) { return true; }

        static void Enter(void*, bool& lockTaken) { lockTaken = true; }
        static bool TryEnter(void*, int) { return true; }

        static void Wait(void*)   {}
        static void Pulse(void*)  {}
        static void PulseAll(void*) {}
    };

} // namespace System::Threading
