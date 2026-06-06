// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <atomic>
#include <condition_variable>
#include <mutex>
#include "System/Threading/WaitHandle.hpp"
#include "System/Threading/EventResetMode.hpp"

namespace System::Threading {

    class EventWaitHandle : public WaitHandle {
        EventResetMode mode_;
        std::atomic<bool> set_{false};
        std::mutex mtx_;
        std::condition_variable cv_;

    public:
        EventWaitHandle(bool initialState, EventResetMode mode)
            : mode_(mode), set_(initialState) {}

        void Set() {
            set_.store(true, std::memory_order_release);
            if (mode_ == EventResetMode::ManualReset)
                cv_.notify_all();
            else
                cv_.notify_one();
        }

        void Reset() { set_.store(false, std::memory_order_release); }

        bool WaitOne() override {
            std::unique_lock<std::mutex> lock(mtx_);
            cv_.wait(lock, [this]{ return set_.load(std::memory_order_acquire); });
            if (mode_ == EventResetMode::AutoReset)
                set_.store(false, std::memory_order_release);
            return true;
        }

        bool WaitOne(int milliseconds) override {
            std::unique_lock<std::mutex> lock(mtx_);
            bool ok = cv_.wait_for(lock, std::chrono::milliseconds(milliseconds),
                [this]{ return set_.load(std::memory_order_acquire); });
            if (ok && mode_ == EventResetMode::AutoReset)
                set_.store(false, std::memory_order_release);
            return ok;
        }
    };

} // namespace System::Threading
