// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <atomic>

namespace System::Threading {

    /** A lightweight mutual-exclusion lock optimised for short wait times. */
    class SpinLock {
        std::atomic_flag flag_ = ATOMIC_FLAG_INIT;

    public:
        /** Constructs a SpinLock; the enableThreadOwnerTracking parameter is ignored. */
        explicit SpinLock(bool /*enableThreadOwnerTracking*/ = false) {}

        /** Acquires the lock, spinning until it is available; sets lockTaken to true on success. */
        void Enter(bool& lockTaken) {
            while (flag_.test_and_set(std::memory_order_acquire)) {
                // spin
            }
            lockTaken = true;
        }

        /** Attempts to acquire the lock without spinning; sets lockTaken and returns true on success. */
        bool TryEnter(bool& lockTaken) {
            lockTaken = !flag_.test_and_set(std::memory_order_acquire);
            return lockTaken;
        }

        /** Releases the lock. */
        void Exit(bool /*useMemoryBarrier*/ = true) {
            flag_.clear(std::memory_order_release);
        }

        /** Returns an approximation of whether the lock is currently held (always false in this stub). */
        [[nodiscard]] bool getIsHeldProperty() const noexcept {
            // Can't query without separate flag; approximate
            return false;
        }
    };

} // namespace System::Threading
