// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <condition_variable>
#include <mutex>
#include <string>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Threading/WaitHandle.hpp"
#include "System/Threading/SemaphoreFullException.hpp"

namespace System::Threading {

    using SharpRuntime::intcs;

    /** A counting semaphore that limits the number of threads accessing a resource concurrently. */
    class Semaphore : public WaitHandle {
        intcs count_;
        intcs maxCount_;
        std::mutex mtx_;
        std::condition_variable cv_;

        static void ValidateCounts(intcs initialCount, intcs maximumCount) {
            if (maximumCount < 1)
                throw System::ArgumentOutOfRangeException("maximumCount");
            if (initialCount < 0 || initialCount > maximumCount)
                throw System::ArgumentOutOfRangeException("initialCount");
        }

    public:
        /** Constructs a Semaphore with the given initial and maximum counts. */
        Semaphore(intcs initialCount, intcs maximumCount)
            : count_(initialCount), maxCount_(maximumCount) {
            ValidateCounts(initialCount, maximumCount);
        }
        /** Constructs a named Semaphore (naming is not truly cross-process; the name is ignored). */
        Semaphore(intcs initialCount, intcs maximumCount, const std::string& /*name*/)
            : count_(initialCount), maxCount_(maximumCount) {
            ValidateCounts(initialCount, maximumCount);
        }

        /** Blocks until the semaphore count is greater than zero, then decrements it. */
        bool WaitOne() override {
            std::unique_lock<std::mutex> lock(mtx_);
            cv_.wait(lock, [this]{ return count_ > 0; });
            --count_;
            return true;
        }

        /** Blocks until the count is greater than zero or the timeout elapses; returns true on success. */
        bool WaitOne(intcs milliseconds) override {
            std::unique_lock<std::mutex> lock(mtx_);
            bool ok = cv_.wait_for(lock, std::chrono::milliseconds(milliseconds),
                [this]{ return count_ > 0; });
            if (ok) --count_;
            return ok;
        }

        /** Releases the semaphore once and returns the count before the release. */
        intcs Release() { return Release(1); }

        /** Releases the semaphore releaseCount times and returns the count before the release. */
        intcs Release(intcs releaseCount) {
            if (releaseCount < 1)
                throw System::ArgumentOutOfRangeException("releaseCount");
            std::unique_lock<std::mutex> lock(mtx_);
            if (count_ + releaseCount > maxCount_)
                throw SemaphoreFullException();
            intcs prev = count_;
            count_ += releaseCount;
            cv_.notify_all();
            return prev;
        }
    };

} // namespace System::Threading
