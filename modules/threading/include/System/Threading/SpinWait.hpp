// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>
#include <thread>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"

namespace System::Threading {

    /** Provides support for spin-based waiting. */
    class SpinWait {
        SharpRuntime::intcs count_ = 0;
        static constexpr SharpRuntime::intcs YieldThreshold = 10;

    public:
        /** Returns the number of times SpinOnce has been called on this instance. */
        [[nodiscard]] SharpRuntime::intcs getCountProperty() const noexcept { return count_; }
        /** Returns true when the next call to SpinOnce will yield the processor. */
        [[nodiscard]] bool getNextSpinWillYieldProperty() const noexcept { return count_ >= YieldThreshold; }

        /** Performs a single spin; yields the processor when the spin count exceeds the threshold. */
        void SpinOnce() {
            if (count_ >= YieldThreshold)
                std::this_thread::yield();
            ++count_;
        }

        /** Performs a single spin, ignoring sleep1Threshold. */
        void SpinOnce(SharpRuntime::intcs /*sleep1Threshold*/) { SpinOnce(); }

        /** Resets the spin counter to zero. */
        void Reset() noexcept { count_ = 0; }

        /**
         * @brief Spins in a loop until condition returns true.
         *
         * @throws System::ArgumentNullException if @p condition is an empty std::function.
         *
         * .NET's parameterless-timeout overload delegates to the timed one with
         * `Timeout.Infinite`, which runs `ArgumentNullException.ThrowIfNull(condition)`
         * before the first spin. This port's two overloads own separate loops, so each
         * carries the check. An empty condition used to reach
         * `std::bad_function_call` on the first iteration -- an exception outside the
         * System::Exception hierarchy that ported `catch (const Exception&)` code cannot
         * see (SR-AUD-213, callable half). Ticket #1951 / CCF-011.
         */
        static void SpinUntil(std::function<bool()> condition) {
            if (!condition) throw System::ArgumentNullException("condition");
            SpinWait sw;
            while (!condition()) sw.SpinOnce();
        }

        /**
         * @brief Spins until condition returns true or the timeout elapses; returns true on success.
         *
         * Verified against SpinWait.cs: -1 (Timeout.Infinite) waits indefinitely (the 0-arg
         * SpinUntil(condition) overload delegates to this one with Timeout.Infinite). A
         * deadline computed as now()+milliseconds(-1) would already be in the past, so -1
         * must be special-cased to skip the deadline check entirely.
         *
         * @throws System::ArgumentOutOfRangeException if @p millisecondsTimeout is less than -1.
         * @throws System::ArgumentNullException if @p condition is an empty std::function.
         *
         * The two checks run in .NET's order --
         * `ArgumentOutOfRangeException.ThrowIfLessThan(millisecondsTimeout, -1)` then
         * `ArgumentNullException.ThrowIfNull(condition)` -- so a call that is invalid in both
         * ways reports the timeout. A `-2` timeout used to return `false`, which is
         * indistinguishable from a legitimate expiry (SR-AUD-213, timeout half, ticket
         * #1954); the callable half is ticket #1951. SR-AUD-213 closes only with both.
         */
        static bool SpinUntil(std::function<bool()> condition, SharpRuntime::intcs millisecondsTimeout) {
            System::ArgumentOutOfRangeException::ThrowIfLessThan(
                millisecondsTimeout, static_cast<SharpRuntime::intcs>(-1), "millisecondsTimeout");
            if (!condition) throw System::ArgumentNullException("condition");
            if (millisecondsTimeout == -1) {
                SpinWait sw;
                while (!condition()) sw.SpinOnce();
                return true;
            }
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
