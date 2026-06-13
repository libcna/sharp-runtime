// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstdint>

namespace System {

    /// Controls the garbage collector (no-op stubs in this C++ port).
    class GC {
    public:
        /// Deleted constructor; all members are static.
        GC() = delete;

        /// Forces a garbage collection (no-op in this port).
        static void Collect() {}
        /// Forces a garbage collection of the specified generation (no-op in this port).
        static void Collect(int generation) { (void)generation; }

        /// Returns the total memory currently thought to be allocated (always 0 in this port).
        static long long GetTotalMemory(bool forceFullCollection) {
            (void)forceFullCollection;
            return 0LL;
        }

        /// Returns the total bytes allocated by managed code since the process started (always 0 in this port).
        static long long GetTotalAllocatedBytes(bool precise = false) {
            (void)precise;
            return 0LL;
        }

        /// Keeps the specified object reachable for GC purposes (no-op in this port).
        template<typename T>
        static void KeepAlive(const T&) {}

        /// Requests that the runtime not call the finalizer for the specified object (no-op in this port).
        static void SuppressFinalize(void*) {}
        /// Requests that the runtime call the finalizer for the specified object (no-op in this port).
        static void ReRegisterForFinalize(void*) {}

        /// Returns the maximum number of generations the GC supports (returns 2).
        static int MaxGeneration() { return 2; }
        /// Returns the current GC generation of the specified object (always 0 in this port).
        static int GetGeneration(void*) { return 0; }

        /// Suspends the current thread until the thread processing the queue of finalizers has emptied it (no-op).
        static void WaitForPendingFinalizers() {}

        /// Returns the total available memory bytes as reported by GCMemoryInfo (always 0 in this port).
        static long long GetGCMemoryInfo_TotalAvailableMemoryBytes() { return 0LL; }
    };

} // namespace System
