// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <chrono>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/TimeSpan.hpp"

namespace System::Diagnostics {

    using SharpRuntime::longcs;

    /**
     * @brief Provides a set of methods and properties that you can use to
     * accurately measure elapsed time.
     * 
     * Partial C++ counterpart of .NET System.Diagnostics.Stopwatch.
     * 
     * @note Status: Implemented
     */
    class Stopwatch {
    private:
        // Must be monotonic: real .NET's GetTimestamp() is backed by clock_gettime(CLOCK_MONOTONIC)
        // on Unix (Interop.Sys.GetTimestamp), specifically so elapsed-time math is immune to
        // wall-clock adjustments. std::chrono::high_resolution_clock is implementation-defined and,
        // on this project's toolchain (libstdc++/GCC), is literally an alias for system_clock (the
        // wall clock) -- not monotonic. Matches the convention already used by PeriodicTimer,
        // WaitHandle, SpinWait, and Thread elsewhere in this codebase.
        using Clock     = std::chrono::steady_clock;
        using TimePoint = Clock::time_point;

        TimePoint start_;
        longcs    elapsed_ns_ = 0;
        bool      running_    = false;

    public:
        /** @brief Constructs a stopped Stopwatch with zero elapsed time. */
        Stopwatch() = default;

        /** @brief Starts, or resumes, measuring elapsed time for an interval. */
        void Start() {
            if (!running_) {
                start_   = Clock::now();
                running_ = true;
            }
        }

        /** @brief Stops measuring elapsed time for an interval. */
        void Stop() {
            if (running_) {
                // Defined accumulation, for the reason GetElapsedTime states below. This site
                // is NOT publicly reachable -- elapsed_ns_ is private with no setter and would
                // need roughly 292 years of measured interval to reach INT64_MAX -- so this is
                // hardening rather than a reproduced defect, and no probe case claims otherwise.
                elapsed_ns_ = addNanoseconds(
                    elapsed_ns_, std::chrono::duration_cast<std::chrono::nanoseconds>(
                                     Clock::now() - start_).count());
                running_ = false;
            }
        }

        /** @brief Stops time interval measurement and resets elapsed time to zero. */
        void Reset() {
            running_    = false;
            elapsed_ns_ = 0;
        }

        /**
         * @brief Stops time interval measurement, resets elapsed time to zero,
         * and starts measuring elapsed time.
         */
        void Restart() {
            Reset();
            Start();
        }

        /** @return true if the stopwatch timer is running. */
        [[nodiscard]] bool getIsRunningProperty() const { return running_; }

        /** @return Total elapsed time in milliseconds. */
        [[nodiscard]] longcs getElapsedMillisecondsProperty() const {
            return currentNs() / 1'000'000LL;
        }

        /** @return Total elapsed time in .NET ticks (100-nanosecond intervals). */
        [[nodiscard]] longcs getElapsedTicksProperty() const {
            return currentNs() / 100LL; // .NET ticks = 100 ns
        }

        /** @return Total elapsed time as a TimeSpan. */
        [[nodiscard]] System::TimeSpan getElapsedProperty() const {
            return System::TimeSpan::FromTicks(getElapsedTicksProperty());
        }

        /**
         * @brief Creates and starts a new Stopwatch.
         * @return A running Stopwatch instance.
         */
        [[nodiscard]] static Stopwatch StartNew() {
            Stopwatch sw;
            sw.Start();
            return sw;
        }

        /**
         * @brief Gets the frequency of the timer as the number of ticks per second.
         * Returns 10,000,000 to match the .NET tick resolution (100 ns per tick).
         */
        static constexpr longcs Frequency = 10'000'000LL;

        /** @brief Indicates whether the Stopwatch timer is based on a high-resolution performance counter. */
        static constexpr bool IsHighResolution = true;

        /** @brief Returns the current high-frequency timestamp in Frequency units (100-ns ticks since epoch). */
        [[nodiscard]] static longcs GetTimestamp() {
            return std::chrono::duration_cast<std::chrono::nanoseconds>(
                       Clock::now().time_since_epoch()).count() / 100LL;
        }

        /**
         * @brief Gets the elapsed time since @p startingTimestamp, a value previously retrieved
         * via GetTimestamp().
         */
        [[nodiscard]] static System::TimeSpan GetElapsedTime(longcs startingTimestamp) {
            return GetElapsedTime(startingTimestamp, GetTimestamp());
        }

        /**
         * @brief Gets the elapsed time between two timestamps previously retrieved via
         * GetTimestamp(). Both @p startingTimestamp and @p endingTimestamp are already expressed
         * in Frequency (100-ns tick) units on this port, so no unit conversion is needed --
         * real .NET's own s_tickFrequency scaling factor reduces to exactly 1.0 here since
         * Frequency == TimeSpan.TicksPerSecond.
         */
        [[nodiscard]] static System::TimeSpan GetElapsedTime(longcs startingTimestamp, longcs endingTimestamp) {
            // CCF-004 class A -- defined wrap (docs/DefinedArithmeticBoundaryPlan.md section 4.1,
            // applied here as an occurrence; that family stays closed 8/8 and gains no member).
            // Real .NET evaluates `endingTimestamp - startingTimestamp` in C#'s *unchecked*
            // integral model, where two's-complement wrap is defined and is the intended result.
            // Signed overflow is undefined in C++, and UBSan confirmed it at this line for
            // (INT64_MIN, INT64_MAX), (INT64_MAX, INT64_MIN), (-1, INT64_MAX) and the
            // one-argument door, so the same wrap is produced in the unsigned domain and
            // converted back. Every representable difference is byte-identical to before, and the
            // four wrapping shapes keep the exact values they had (SR-AUD-131, ticket #2218).
            return System::TimeSpan::FromTicks(subtractTimestamps(endingTimestamp, startingTimestamp));
        }

        /** @brief Returns the elapsed time formatted the same way as Elapsed.ToString(). */
        [[nodiscard]] std::string ToString() const {
            return getElapsedProperty().ToString();
        }

    private:
        // The one shared spelling of CCF-004's class A transformation for this type: perform the
        // operation in the unsigned counterpart, where wrap is defined, and convert back (the
        // conversion is itself well defined since C++20). Deliberately NOT a new shared arithmetic
        // helper across the repository -- docs/DefinedArithmeticBoundaryPlan.md section 10
        // exclusion 2 forbids that -- only a local, file-scoped spelling used three times here.
        [[nodiscard]] static constexpr longcs subtractTimestamps(longcs a, longcs b) noexcept {
            return static_cast<longcs>(static_cast<SharpRuntime::ulongcs>(a) -
                                       static_cast<SharpRuntime::ulongcs>(b));
        }

        [[nodiscard]] static constexpr longcs addNanoseconds(longcs a, longcs b) noexcept {
            return static_cast<longcs>(static_cast<SharpRuntime::ulongcs>(a) +
                                       static_cast<SharpRuntime::ulongcs>(b));
        }

        [[nodiscard]] longcs currentNs() const {
            longcs ns = elapsed_ns_;
            if (running_)
                // Same hardening as Stop(); see the note there.
                ns = addNanoseconds(ns, std::chrono::duration_cast<std::chrono::nanoseconds>(
                                            Clock::now() - start_).count());
            return ns;
        }
    };

} // namespace System::Diagnostics
