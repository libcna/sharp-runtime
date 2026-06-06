// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <atomic>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::Threading {

    using SharpRuntime::intcs;
    using SharpRuntime::longcs;

    /**
     * @brief Provides atomic operations for variables shared by multiple threads.
     *
     * Wraps C++ std::atomic operations.
     * Partial C++ counterpart of .NET System.Threading.Interlocked.
     *
     * @note Status: Implemented
     */
    class Interlocked {
    public:
        Interlocked() = delete;

        static intcs  Increment(intcs&  location) { return __atomic_add_fetch(&location, 1,  __ATOMIC_SEQ_CST); }
        static longcs Increment(longcs& location) { return __atomic_add_fetch(&location, 1LL, __ATOMIC_SEQ_CST); }
        static intcs  Decrement(intcs&  location) { return __atomic_sub_fetch(&location, 1,  __ATOMIC_SEQ_CST); }
        static longcs Decrement(longcs& location) { return __atomic_sub_fetch(&location, 1LL, __ATOMIC_SEQ_CST); }
        static intcs  Add(intcs&  location, intcs  value) { return __atomic_add_fetch(&location, value,  __ATOMIC_SEQ_CST); }
        static longcs Add(longcs& location, longcs value) { return __atomic_add_fetch(&location, value, __ATOMIC_SEQ_CST); }
        static intcs  Exchange(intcs&  location, intcs  value) { intcs  old; __atomic_exchange(&location, &value, &old, __ATOMIC_SEQ_CST); return old; }
        static longcs Exchange(longcs& location, longcs value) { longcs old; __atomic_exchange(&location, &value, &old, __ATOMIC_SEQ_CST); return old; }
        static intcs  CompareExchange(intcs&  location, intcs  value, intcs  comparand) {
            __atomic_compare_exchange(&location, &comparand, &value, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); return comparand;
        }
        static longcs CompareExchange(longcs& location, longcs value, longcs comparand) {
            __atomic_compare_exchange(&location, &comparand, &value, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); return comparand;
        }
        static intcs  Read(const intcs&  location) { return __atomic_load_n(&location, __ATOMIC_SEQ_CST); }
        static longcs Read(const longcs& location) { return __atomic_load_n(&location, __ATOMIC_SEQ_CST); }
    };

} // namespace System::Threading
