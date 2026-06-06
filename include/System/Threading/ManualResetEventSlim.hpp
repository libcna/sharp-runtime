// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <atomic>
#include <condition_variable>
#include <mutex>
#include "System/IDisposable.hpp"

namespace System::Threading {

    // Lighter-weight version of ManualResetEvent that avoids OS handles for short waits.
    class ManualResetEventSlim : public System::IDisposable {
        std::atomic<bool> set_{false};
        std::mutex mtx_;
        std::condition_variable cv_;

    public:
        explicit ManualResetEventSlim(bool initialState = false) : set_(initialState) {}

        void Set() {
            set_.store(true, std::memory_order_release);
            cv_.notify_all();
        }

        void Reset() {
            set_.store(false, std::memory_order_release);
        }

        void Wait() {
            if (set_.load(std::memory_order_acquire)) return;
            std::unique_lock<std::mutex> lock(mtx_);
            cv_.wait(lock, [this]{ return set_.load(std::memory_order_acquire); });
        }

        bool Wait(int millisecondsTimeout) {
            if (set_.load(std::memory_order_acquire)) return true;
            std::unique_lock<std::mutex> lock(mtx_);
            return cv_.wait_for(lock, std::chrono::milliseconds(millisecondsTimeout),
                [this]{ return set_.load(std::memory_order_acquire); });
        }

        [[nodiscard]] bool getIsSetProperty() const noexcept {
            return set_.load(std::memory_order_acquire);
        }

        void Dispose() override {}
    };

} // namespace System::Threading
