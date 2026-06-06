// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <thread>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::Threading {

    class SpinWait {
        SharpRuntime::intcs count_ = 0;
        static constexpr SharpRuntime::intcs YieldThreshold = 10;

    public:
        [[nodiscard]] SharpRuntime::intcs getCountProperty() const noexcept { return count_; }
        [[nodiscard]] bool getNextSpinWillYieldProperty() const noexcept { return count_ >= YieldThreshold; }

        void SpinOnce() {
            if (count_ >= YieldThreshold)
                std::this_thread::yield();
            ++count_;
        }

        void SpinOnce(SharpRuntime::intcs /*sleep1Threshold*/) { SpinOnce(); }

        void Reset() noexcept { count_ = 0; }

        static void SpinUntil(std::function<bool()> condition) {
            SpinWait sw;
            while (!condition()) sw.SpinOnce();
        }

        static bool SpinUntil(std::function<bool()> condition, int millisecondsTimeout) {
            auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(millisecondsTimeout);
            SpinWait sw;
            while (!condition()) {
                if (std::chrono::steady_clock::now() >= deadline) return false;
                sw.SpinOnce();
            }
            return true;
        }
    };

} // namespace System::Threading
