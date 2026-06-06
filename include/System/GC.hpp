// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstdint>

namespace System {

    class GC {
    public:
        GC() = delete;

        static void Collect() {}
        static void Collect(int generation) { (void)generation; }

        static long long GetTotalMemory(bool forceFullCollection) {
            (void)forceFullCollection;
            return 0LL;
        }

        static long long GetTotalAllocatedBytes(bool precise = false) {
            (void)precise;
            return 0LL;
        }

        template<typename T>
        static void KeepAlive(const T&) {}

        static void SuppressFinalize(void*) {}
        static void ReRegisterForFinalize(void*) {}

        static int MaxGeneration() { return 2; }
        static int GetGeneration(void*) { return 0; }

        static void WaitForPendingFinalizers() {}

        static long long GetGCMemoryInfo_TotalAvailableMemoryBytes() { return 0LL; }
    };

} // namespace System
