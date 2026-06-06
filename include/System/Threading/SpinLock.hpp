// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <atomic>

namespace System::Threading {

    class SpinLock {
        std::atomic_flag flag_ = ATOMIC_FLAG_INIT;

    public:
        explicit SpinLock(bool /*enableThreadOwnerTracking*/ = false) {}

        void Enter(bool& lockTaken) {
            while (flag_.test_and_set(std::memory_order_acquire)) {
                // spin
            }
            lockTaken = true;
        }

        bool TryEnter(bool& lockTaken) {
            lockTaken = !flag_.test_and_set(std::memory_order_acquire);
            return lockTaken;
        }

        void Exit(bool /*useMemoryBarrier*/ = true) {
            flag_.clear(std::memory_order_release);
        }

        [[nodiscard]] bool getIsHeldProperty() const noexcept {
            // Can't query without separate flag; approximate
            return false;
        }
    };

} // namespace System::Threading
