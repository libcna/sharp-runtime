// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>
#include <future>
#include <thread>

namespace System::Threading {

    class ThreadPool {
    public:
        ThreadPool() = delete;

        // Queues a work item on a detached thread (approximate — no real pool management).
        static bool QueueUserWorkItem(std::function<void()> callBack) {
            std::thread(std::move(callBack)).detach();
            return true;
        }

        static bool QueueUserWorkItem(std::function<void(void*)> callBack, void* state) {
            std::thread([callBack, state]{ callBack(state); }).detach();
            return true;
        }

        // Approximate processor count limits.
        static void GetMinThreads(int& workerThreads, int& completionPortThreads) {
            workerThreads = 1;
            completionPortThreads = 1;
        }

        static void GetMaxThreads(int& workerThreads, int& completionPortThreads) {
            workerThreads = static_cast<int>(std::thread::hardware_concurrency()) * 2;
            completionPortThreads = workerThreads;
        }

        static bool SetMinThreads(int /*workerThreads*/, int /*completionPortThreads*/) { return true; }
        static bool SetMaxThreads(int /*workerThreads*/, int /*completionPortThreads*/) { return true; }
    };

} // namespace System::Threading
