// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstdint>
#include <functional>
#include <future>
#include <vector>
#if defined(__EMSCRIPTEN__)
#  include "System/PlatformNotSupportedException.hpp"
#endif

namespace System::Threading::Tasks {

    /// Options to configure the behaviour of a parallel loop.
    struct ParallelOptions {
        /// Maximum number of concurrent iterations; -1 means unlimited.
        int MaxDegreeOfParallelism = -1; // -1 = unlimited
    };

    /// Provides status and control for the current parallel loop iteration.
    struct ParallelLoopState {
        bool shouldStop_  = false;
        bool isExceptional_ = false;

        /// Requests that the loop stop after the current iteration completes.
        void Stop()  { shouldStop_ = true; }
        /// Requests that the loop break after the current iteration completes.
        void Break() { shouldStop_ = true; }
        /// Returns true if the current iteration should exit.
        [[nodiscard]] bool getShouldExitCurrentIterationProperty() const { return shouldStop_; }
        /// Returns true if any iteration threw an exception.
        [[nodiscard]] bool getIsExceptionalProperty()              const { return isExceptional_; }
    };

    /// Reports whether a parallel loop ran to completion.
    struct ParallelLoopResult {
        bool isCompleted_  = false;
        bool lowestBreakIteration_ = false;
        /// Returns true if the loop ran to completion without being stopped or broken.
        [[nodiscard]] bool getIsCompletedProperty() const { return isCompleted_; }
    };

    /// Provides support for parallel loops and regions.
    class Parallel {
    public:
        /// Executes a for loop from fromInclusive to toExclusive in parallel.
        static ParallelLoopResult For(int fromInclusive, int toExclusive, std::function<void(int)> body) {
#if defined(__EMSCRIPTEN__)
            (void)fromInclusive; (void)toExclusive; (void)body;
            throw System::PlatformNotSupportedException("Parallel::For requires pthreads (not available in Emscripten single-threaded build)");
#else
            std::vector<std::future<void>> futures;
            for (int i = fromInclusive; i < toExclusive; ++i) {
                int idx = i;
                futures.push_back(std::async(std::launch::async, [body, idx]{ body(idx); }));
            }
            for (auto& f : futures) f.get();
            ParallelLoopResult result;
            result.isCompleted_ = true;
            return result;
#endif
        }

        /// Executes a for loop in parallel, respecting the provided ParallelOptions.
        static ParallelLoopResult For(int fromInclusive, int toExclusive, const ParallelOptions& /*opts*/,
                                       std::function<void(int)> body) {
            return For(fromInclusive, toExclusive, std::move(body));
        }

        /// Executes a foreach loop over source in parallel.
        template<typename TSource>
        static ParallelLoopResult ForEach(const std::vector<TSource>& source, std::function<void(TSource)> body) {
#if defined(__EMSCRIPTEN__)
            (void)source; (void)body;
            throw System::PlatformNotSupportedException("Parallel::ForEach requires pthreads (not available in Emscripten single-threaded build)");
#else
            std::vector<std::future<void>> futures;
            for (auto& item : source) {
                futures.push_back(std::async(std::launch::async, [body, &item]{ body(item); }));
            }
            for (auto& f : futures) f.get();
            ParallelLoopResult result;
            result.isCompleted_ = true;
            return result;
#endif
        }

        /// Executes all actions in parallel, blocking until all have completed.
        static void Invoke(std::initializer_list<std::function<void()>> actions) {
#if defined(__EMSCRIPTEN__)
            (void)actions;
            throw System::PlatformNotSupportedException("Parallel::Invoke requires pthreads (not available in Emscripten single-threaded build)");
#else
            std::vector<std::future<void>> futures;
            for (auto& a : actions)
                futures.push_back(std::async(std::launch::async, a));
            for (auto& f : futures) f.get();
#endif
        }
    };

} // namespace System::Threading::Tasks
