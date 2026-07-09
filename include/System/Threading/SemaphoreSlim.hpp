// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <algorithm>
#include <mutex>
#include <condition_variable>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Threading/SemaphoreFullException.hpp"
#include "System/Threading/WaitHandle.hpp"

namespace System::Threading {

    using SharpRuntime::intcs;

    /**
     * @brief A lightweight alternative to Semaphore that limits the number of threads that can access a resource.
     *
     * Wraps std::condition_variable. Partial C++ counterpart of .NET System.Threading.SemaphoreSlim.
     *
     * @note Status: Implemented
     */
    class SemaphoreSlim {
        std::mutex              mutex_;
        std::condition_variable cv_;
        intcs                   count_;
        intcs                   maxCount_;
    public:
        /** Constructs a SemaphoreSlim with the given initial and maximum counts. */
        explicit SemaphoreSlim(intcs initialCount, intcs maxCount = 0x7fffffff)
            : count_(initialCount), maxCount_(maxCount) {
            if (maxCount < 1)
                throw System::ArgumentOutOfRangeException("maxCount");
            if (initialCount < 0 || initialCount > maxCount)
                throw System::ArgumentOutOfRangeException("initialCount");
        }

        /** Returns the current count of the semaphore. */
        [[nodiscard]] intcs getCurrentCountProperty() const { return count_; }

        /** @brief Blocks until the semaphore can be entered. */
        void Wait() {
            std::unique_lock<std::mutex> lk(mutex_);
            cv_.wait(lk, [this]{ return count_ > 0; });
            --count_;
        }

        /**
         * @brief Tries to enter the semaphore within a timeout. Returns true on success.
         * @throws System::ArgumentOutOfRangeException if @p milliseconds is less than -1.
         */
        bool Wait(intcs milliseconds) {
            WaitHandle::ValidateTimeout(milliseconds);
            std::unique_lock<std::mutex> lk(mutex_);
            bool ok = cv_.wait_for(lk, std::chrono::milliseconds(milliseconds), [this]{ return count_ > 0; });
            if (ok) --count_;
            return ok;
        }

        /** @brief Releases the semaphore once. */
        intcs Release() { return Release(1); }

        /** @brief Releases the semaphore a specified number of times. Returns previous count. */
        intcs Release(intcs releaseCount) {
            if (releaseCount < 1)
                throw System::ArgumentOutOfRangeException("releaseCount");
            std::lock_guard<std::mutex> lk(mutex_);
            if (count_ + releaseCount > maxCount_)
                throw System::Threading::SemaphoreFullException();
            intcs prev = count_;
            count_ += releaseCount;
            cv_.notify_all();
            return prev;
        }

        /** Releases resources used by the SemaphoreSlim. */
        void Dispose() {}
    };

} // namespace System::Threading
