// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <atomic>
#include <condition_variable>
#include <mutex>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/IDisposable.hpp"
#include "System/ObjectDisposedException.hpp"
#include "System/Threading/WaitHandle.hpp"

namespace System::Threading {

    using SharpRuntime::intcs;

    /** A lighter-weight ManualResetEvent that avoids OS handles for short waits. */
    class ManualResetEventSlim : public System::IDisposable {
        std::atomic<bool> set_{false};
        std::mutex mtx_;
        std::condition_variable cv_;
        // Ticket #1955 / cause T-A of docs/ThreadingNamespaceReviewPlan.md. This was an
        // ordinary `bool`, written by Dispose() and read by the guard below with no
        // synchronisation between them. Mixing synchronised and unsynchronised access to the
        // same object is a data race and therefore undefined behaviour, and ThreadSanitizer
        // confirmed it both at audit time and again in
        // build-probe/1955_probe1_shared_state_races.cpp. std::atomic<bool> is 1 byte and
        // 1-byte aligned on every supported target -- measured before and after in
        // build-probe/1955_probe1_layout_{before,after}.log -- so the flag's type change is
        // layout-neutral and the header stays consumer-compatible.
        std::atomic<bool> disposed_{false};

        void ThrowIfDisposed() const {
            if (disposed_.load(std::memory_order_acquire))
                throw System::ObjectDisposedException("ManualResetEventSlim");
        }

    public:
        /** Constructs a ManualResetEventSlim with the specified initial state. */
        explicit ManualResetEventSlim(bool initialState = false) : set_(initialState) {}

        /** Sets the event, releasing all waiting threads. */
        void Set() {
            set_.store(true, std::memory_order_release);
            cv_.notify_all();
        }

        /**
         * @brief Resets the event to the non-signalled state.
         * @throws System::ObjectDisposedException if this instance has been disposed.
         */
        void Reset() {
            ThrowIfDisposed();
            set_.store(false, std::memory_order_release);
        }

        /**
         * @brief Blocks until the event is set.
         * @throws System::ObjectDisposedException if this instance has been disposed.
         */
        void Wait() {
            ThrowIfDisposed();
            if (set_.load(std::memory_order_acquire)) return;
            std::unique_lock<std::mutex> lock(mtx_);
            cv_.wait(lock, [this]{ return set_.load(std::memory_order_acquire); });
        }

        /**
         * @brief Blocks until the event is set or the timeout elapses; returns true if set.
         * @throws System::ArgumentOutOfRangeException if @p millisecondsTimeout is less than -1.
         * @throws System::ObjectDisposedException if this instance has been disposed.
         */
        bool Wait(intcs millisecondsTimeout) {
            WaitHandle::ValidateTimeout(millisecondsTimeout);
            ThrowIfDisposed();
            if (set_.load(std::memory_order_acquire)) return true;
            std::unique_lock<std::mutex> lock(mtx_);
            // -1 (Timeout.Infinite) waits indefinitely; std::chrono's wait_for treats a
            // negative duration as already-expired, so it must be special-cased.
            if (millisecondsTimeout == -1) {
                cv_.wait(lock, [this]{ return set_.load(std::memory_order_acquire); });
                return true;
            }
            return cv_.wait_for(lock, std::chrono::milliseconds(millisecondsTimeout),
                [this]{ return set_.load(std::memory_order_acquire); });
        }

        /** Returns true if the event is currently in the set state. */
        [[nodiscard]] bool getIsSetProperty() const noexcept {
            return set_.load(std::memory_order_acquire);
        }

        /** Releases resources for this event. */
        void Dispose() override { disposed_.store(true, std::memory_order_release); }
    };

} // namespace System::Threading
