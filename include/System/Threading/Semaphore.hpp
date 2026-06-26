// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <condition_variable>
#include <mutex>
#include <string>
#include "System/Threading/WaitHandle.hpp"
#include "System/Threading/SemaphoreFullException.hpp"

namespace System::Threading {

    /** A counting semaphore that limits the number of threads accessing a resource concurrently. */
    class Semaphore : public WaitHandle {
        int count_;
        int maxCount_;
        std::mutex mtx_;
        std::condition_variable cv_;

    public:
        /** Constructs a Semaphore with the given initial and maximum counts. */
        Semaphore(int initialCount, int maximumCount)
            : count_(initialCount), maxCount_(maximumCount) {}
        /** Constructs a named Semaphore (naming is not truly cross-process; the name is ignored). */
        Semaphore(int initialCount, int maximumCount, const std::string& /*name*/)
            : count_(initialCount), maxCount_(maximumCount) {}

        /** Blocks until the semaphore count is greater than zero, then decrements it. */
        bool WaitOne() override {
            std::unique_lock<std::mutex> lock(mtx_);
            cv_.wait(lock, [this]{ return count_ > 0; });
            --count_;
            return true;
        }

        /** Blocks until the count is greater than zero or the timeout elapses; returns true on success. */
        bool WaitOne(int milliseconds) override {
            std::unique_lock<std::mutex> lock(mtx_);
            bool ok = cv_.wait_for(lock, std::chrono::milliseconds(milliseconds),
                [this]{ return count_ > 0; });
            if (ok) --count_;
            return ok;
        }

        /** Releases the semaphore once and returns the count before the release. */
        int Release() { return Release(1); }

        /** Releases the semaphore releaseCount times and returns the count before the release. */
        int Release(int releaseCount) {
            std::unique_lock<std::mutex> lock(mtx_);
            if (count_ + releaseCount > maxCount_)
                throw SemaphoreFullException();
            int prev = count_;
            count_ += releaseCount;
            cv_.notify_all();
            return prev;
        }
    };

} // namespace System::Threading
