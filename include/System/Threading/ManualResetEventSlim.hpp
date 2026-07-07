// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <atomic>
#include <condition_variable>
#include <mutex>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/IDisposable.hpp"

namespace System::Threading {

    using SharpRuntime::intcs;

    /** A lighter-weight ManualResetEvent that avoids OS handles for short waits. */
    class ManualResetEventSlim : public System::IDisposable {
        std::atomic<bool> set_{false};
        std::mutex mtx_;
        std::condition_variable cv_;

    public:
        /** Constructs a ManualResetEventSlim with the specified initial state. */
        explicit ManualResetEventSlim(bool initialState = false) : set_(initialState) {}

        /** Sets the event, releasing all waiting threads. */
        void Set() {
            set_.store(true, std::memory_order_release);
            cv_.notify_all();
        }

        /** Resets the event to the non-signalled state. */
        void Reset() {
            set_.store(false, std::memory_order_release);
        }

        /** Blocks until the event is set. */
        void Wait() {
            if (set_.load(std::memory_order_acquire)) return;
            std::unique_lock<std::mutex> lock(mtx_);
            cv_.wait(lock, [this]{ return set_.load(std::memory_order_acquire); });
        }

        /** Blocks until the event is set or the timeout elapses; returns true if set. */
        bool Wait(intcs millisecondsTimeout) {
            if (set_.load(std::memory_order_acquire)) return true;
            std::unique_lock<std::mutex> lock(mtx_);
            return cv_.wait_for(lock, std::chrono::milliseconds(millisecondsTimeout),
                [this]{ return set_.load(std::memory_order_acquire); });
        }

        /** Returns true if the event is currently in the set state. */
        [[nodiscard]] bool getIsSetProperty() const noexcept {
            return set_.load(std::memory_order_acquire);
        }

        /** Releases resources (no-op for this implementation). */
        void Dispose() override {}
    };

} // namespace System::Threading
