// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <chrono>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/TimeSpan.hpp"

namespace System::Diagnostics {

    using SharpRuntime::longcs;

    /// @brief Provides a set of methods and properties that you can use to
    /// accurately measure elapsed time.
    ///
    /// Partial C++ counterpart of .NET System.Diagnostics.Stopwatch.
    ///
    /// @note Status: Implemented
    class Stopwatch {
    private:
        using Clock     = std::chrono::high_resolution_clock;
        using TimePoint = Clock::time_point;

        TimePoint start_;
        longcs    elapsed_ns_ = 0;
        bool      running_    = false;

    public:
        /// @brief Constructs a stopped Stopwatch with zero elapsed time.
        Stopwatch() = default;

        /// @brief Starts, or resumes, measuring elapsed time for an interval.
        void Start() {
            if (!running_) {
                start_   = Clock::now();
                running_ = true;
            }
        }

        /// @brief Stops measuring elapsed time for an interval.
        void Stop() {
            if (running_) {
                elapsed_ns_ += std::chrono::duration_cast<std::chrono::nanoseconds>(
                                   Clock::now() - start_).count();
                running_ = false;
            }
        }

        /// @brief Stops time interval measurement and resets elapsed time to zero.
        void Reset() {
            running_    = false;
            elapsed_ns_ = 0;
        }

        /// @brief Stops time interval measurement, resets elapsed time to zero,
        /// and starts measuring elapsed time.
        void Restart() {
            Reset();
            Start();
        }

        /// @return true if the stopwatch timer is running.
        [[nodiscard]] bool getIsRunningProperty() const { return running_; }

        /// @return Total elapsed time in milliseconds.
        [[nodiscard]] longcs getElapsedMillisecondsProperty() const {
            return currentNs() / 1'000'000LL;
        }

        /// @return Total elapsed time in .NET ticks (100-nanosecond intervals).
        [[nodiscard]] longcs getElapsedTicksProperty() const {
            return currentNs() / 100LL; // .NET ticks = 100 ns
        }

        /// @return Total elapsed time as a TimeSpan.
        [[nodiscard]] System::TimeSpan getElapsedProperty() const {
            return System::TimeSpan::FromTicks(getElapsedTicksProperty());
        }

        /// @brief Creates and starts a new Stopwatch.
        /// @return A running Stopwatch instance.
        [[nodiscard]] static Stopwatch StartNew() {
            Stopwatch sw;
            sw.Start();
            return sw;
        }

    private:
        [[nodiscard]] longcs currentNs() const {
            longcs ns = elapsed_ns_;
            if (running_)
                ns += std::chrono::duration_cast<std::chrono::nanoseconds>(
                          Clock::now() - start_).count();
            return ns;
        }
    };

} // namespace System::Diagnostics
