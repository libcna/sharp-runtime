// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <atomic>

namespace System::Threading {

    class Volatile {
    public:
        Volatile() = delete;

        template<typename T>
        static T Read(const T& location) {
            return __atomic_load_n(&location, __ATOMIC_ACQUIRE);
        }

        template<typename T>
        static void Write(T& location, T value) {
            __atomic_store_n(&location, value, __ATOMIC_RELEASE);
        }
    };

} // namespace System::Threading
