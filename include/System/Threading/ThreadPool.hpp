// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>
#include <future>
#include <thread>
#if defined(__EMSCRIPTEN__)
#  include "System/PlatformNotSupportedException.hpp"
#endif

namespace System::Threading {

    /** Provides a pool of threads that can be used to execute tasks, post work items, and other operations. */
    class ThreadPool {
    public:
        /** Prevents instantiation — all members are static. */
        ThreadPool() = delete;

        /** Queues the callback for execution on a detached thread; returns true on success. */
        static bool QueueUserWorkItem(std::function<void()> callBack) {
#if defined(__EMSCRIPTEN__)
            (void)callBack;
            throw System::PlatformNotSupportedException("ThreadPool::QueueUserWorkItem requires pthreads (not available in Emscripten single-threaded build)");
#else
            std::thread(std::move(callBack)).detach();
            return true;
#endif
        }

        /** Queues the callback with a state argument for execution on a detached thread; returns true on success. */
        static bool QueueUserWorkItem(std::function<void(void*)> callBack, void* state) {
#if defined(__EMSCRIPTEN__)
            (void)callBack; (void)state;
            throw System::PlatformNotSupportedException("ThreadPool::QueueUserWorkItem requires pthreads (not available in Emscripten single-threaded build)");
#else
            std::thread([callBack, state]{ callBack(state); }).detach();
            return true;
#endif
        }

        /** Retrieves the minimum number of threads the pool maintains. */
        static void GetMinThreads(int& workerThreads, int& completionPortThreads) {
            workerThreads = 1;
            completionPortThreads = 1;
        }

        /** Retrieves the maximum number of concurrent threads. */
        static void GetMaxThreads(int& workerThreads, int& completionPortThreads) {
#if defined(__EMSCRIPTEN__)
            workerThreads = 1;
            completionPortThreads = 1;
#else
            workerThreads = static_cast<int>(std::thread::hardware_concurrency()) * 2;
            completionPortThreads = workerThreads;
#endif
        }

        /** Sets the minimum number of threads the pool maintains; always returns true in this stub. */
        static bool SetMinThreads(int /*workerThreads*/, int /*completionPortThreads*/) { return true; }
        /** Sets the maximum number of concurrent threads; always returns true in this stub. */
        static bool SetMaxThreads(int /*workerThreads*/, int /*completionPortThreads*/) { return true; }
    };

} // namespace System::Threading
