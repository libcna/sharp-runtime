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

    // Named semaphore is not truly cross-process in this implementation.
    class Semaphore : public WaitHandle {
        int count_;
        int maxCount_;
        std::mutex mtx_;
        std::condition_variable cv_;

    public:
        Semaphore(int initialCount, int maximumCount)
            : count_(initialCount), maxCount_(maximumCount) {}
        Semaphore(int initialCount, int maximumCount, const std::string& /*name*/)
            : count_(initialCount), maxCount_(maximumCount) {}

        bool WaitOne() override {
            std::unique_lock<std::mutex> lock(mtx_);
            cv_.wait(lock, [this]{ return count_ > 0; });
            --count_;
            return true;
        }

        bool WaitOne(int milliseconds) override {
            std::unique_lock<std::mutex> lock(mtx_);
            bool ok = cv_.wait_for(lock, std::chrono::milliseconds(milliseconds),
                [this]{ return count_ > 0; });
            if (ok) --count_;
            return ok;
        }

        int Release() { return Release(1); }

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
