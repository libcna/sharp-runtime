// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <algorithm>
#include <atomic>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <thread>
#include <vector>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/AggregateException.hpp"
#if defined(__EMSCRIPTEN__)
#  include "System/PlatformNotSupportedException.hpp"
#endif

namespace System::Threading::Tasks {

    using SharpRuntime::intcs;
    using SharpRuntime::longcs;

    /** @brief Stores options that configure the operation of methods on the Parallel class. */
    struct ParallelOptions {
        /** Maximum number of concurrent iterations; -1 (the default) means unlimited. */
        intcs MaxDegreeOfParallelism = -1;
    };

    /**
     * @brief Enables iterations of parallel loops to interact with other iterations.
     *
     * C++ counterpart of .NET System.Threading.Tasks.ParallelLoopState. Simplification: unlike
     * .NET, this runtime does not distinguish Break() (stop scheduling iterations after the lowest
     * one requested) from Stop() (stop scheduling immediately) — both simply prevent further
     * iterations from being launched; iterations already running when Stop/Break is called still
     * run to completion, matching .NET's own guarantee that in-flight iterations are not aborted.
     */
    class ParallelLoopState {
        std::shared_ptr<std::atomic<bool>> stopped_ = std::make_shared<std::atomic<bool>>(false);
        std::shared_ptr<std::atomic<bool>> exceptional_ = std::make_shared<std::atomic<bool>>(false);

    public:
        /** Requests that the loop stop scheduling further iterations immediately. */
        void Stop() { stopped_->store(true); }
        /** Requests that the loop stop scheduling iterations after the current one. */
        void Break() { stopped_->store(true); }
        /** Marks the loop as having encountered an exception in some iteration. */
        void SetExceptional() { exceptional_->store(true); }
        /** Returns true if the current iteration should exit (Stop/Break was requested by any iteration). */
        [[nodiscard]] bool getShouldExitCurrentIterationProperty() const { return stopped_->load(); }
        /** Returns true if Stop() or Break() has been called by any iteration. */
        [[nodiscard]] bool getIsStoppedProperty() const { return stopped_->load(); }
        /** Returns true if any iteration has thrown an unhandled exception. */
        [[nodiscard]] bool getIsExceptionalProperty() const { return exceptional_->load(); }
    };

    /** @brief Reports on the all or portion of a loop that was completed. */
    struct ParallelLoopResult {
        bool isCompleted_ = false;
        std::optional<longcs> lowestBreakIteration_;

        /** Returns true if the loop ran to completion, without a Stop() or Break() request. */
        [[nodiscard]] bool getIsCompletedProperty() const { return isCompleted_; }
        /** Returns the index of the lowest iteration from which Break() was called, or nullopt if none. */
        [[nodiscard]] std::optional<longcs> getLowestBreakIterationProperty() const { return lowestBreakIteration_; }
    };

    /** @brief Provides support for parallel loops and regions. */
    class Parallel {
    private:
        // Verified against Parallel.cs: when one or more iterations/actions throw, the loop
        // does not propagate only the first exception it happens to observe -- it collects every
        // exception raised and throws them together as a single AggregateException. This port
        // previously called f.get() per future with no try/catch, so the first exception
        // encountered escaped immediately (unwrapped) and every other future's exception was
        // silently discarded when its std::future was destroyed. This waits for and collects
        // from every future in the batch, regardless of exceptions, so exceptions.empty() is a
        // reliable "no errors occurred" check even after a mid-batch throw.
        static void waitAllCollectingExceptions(std::vector<std::future<void>>& futures,
                                                  std::vector<std::exception_ptr>& exceptions) {
            for (auto& f : futures) {
                try {
                    f.get();
                } catch (...) {
                    exceptions.push_back(std::current_exception());
                }
            }
        }

    public:
        /** Executes a for loop from fromInclusive to toExclusive in parallel. */
        static ParallelLoopResult For(intcs fromInclusive, intcs toExclusive, std::function<void(intcs)> body) {
            return For(fromInclusive, toExclusive, ParallelOptions{}, std::move(body));
        }

        /** Executes a for loop in parallel, respecting MaxDegreeOfParallelism in @p opts. */
        static ParallelLoopResult For(intcs fromInclusive, intcs toExclusive, const ParallelOptions& opts,
                                       std::function<void(intcs)> body) {
#if defined(__EMSCRIPTEN__)
            (void)fromInclusive; (void)toExclusive; (void)opts; (void)body;
            throw System::PlatformNotSupportedException("Parallel::For requires pthreads (not available in Emscripten single-threaded build)");
#else
            intcs maxDeg = opts.MaxDegreeOfParallelism;
            if (maxDeg <= 0)
                maxDeg = static_cast<intcs>(std::thread::hardware_concurrency());
            if (maxDeg < 1) maxDeg = 1;

            std::vector<std::future<void>> futures;
            std::vector<std::exception_ptr> exceptions;
            for (intcs i = fromInclusive; i < toExclusive; ++i) {
                futures.push_back(std::async(std::launch::async, [body, i]{ body(i); }));
                if (static_cast<intcs>(futures.size()) >= maxDeg) {
                    waitAllCollectingExceptions(futures, exceptions);
                    futures.clear();
                }
            }
            waitAllCollectingExceptions(futures, exceptions);
            if (!exceptions.empty()) throw System::AggregateException(std::move(exceptions));
            ParallelLoopResult result;
            result.isCompleted_ = true;
            return result;
#endif
        }

        /**
         * Executes a for loop from fromInclusive to toExclusive in parallel, passing each iteration a
         * ParallelLoopState so the loop body can call Stop()/Break() to request early termination.
         * Once requested, no further iterations are scheduled, but iterations already in flight run
         * to completion.
         */
        static ParallelLoopResult For(intcs fromInclusive, intcs toExclusive,
                                       std::function<void(intcs, ParallelLoopState&)> body) {
#if defined(__EMSCRIPTEN__)
            (void)fromInclusive; (void)toExclusive; (void)body;
            throw System::PlatformNotSupportedException("Parallel::For requires pthreads (not available in Emscripten single-threaded build)");
#else
            ParallelLoopState state;
            std::vector<std::future<void>> futures;
            std::vector<std::exception_ptr> exceptions;
            for (intcs i = fromInclusive; i < toExclusive; ++i) {
                if (state.getShouldExitCurrentIterationProperty()) break;
                futures.push_back(std::async(std::launch::async, [body, i, state]() mutable { body(i, state); }));
            }
            waitAllCollectingExceptions(futures, exceptions);
            if (!exceptions.empty()) throw System::AggregateException(std::move(exceptions));
            ParallelLoopResult result;
            result.isCompleted_ = !state.getIsStoppedProperty();
            return result;
#endif
        }

        /** Executes a foreach loop over source in parallel. */
        template<typename TSource>
        static ParallelLoopResult ForEach(const std::vector<TSource>& source, std::function<void(TSource)> body) {
#if defined(__EMSCRIPTEN__)
            (void)source; (void)body;
            throw System::PlatformNotSupportedException("Parallel::ForEach requires pthreads (not available in Emscripten single-threaded build)");
#else
            std::vector<std::future<void>> futures;
            // Capture item by value to avoid dangling reference after loop iteration.
            for (TSource item : source) {
                futures.push_back(std::async(std::launch::async, [body, item]{ body(item); }));
            }
            std::vector<std::exception_ptr> exceptions;
            waitAllCollectingExceptions(futures, exceptions);
            if (!exceptions.empty()) throw System::AggregateException(std::move(exceptions));
            ParallelLoopResult result;
            result.isCompleted_ = true;
            return result;
#endif
        }

        /** Executes a foreach loop over source in parallel, passing each iteration a ParallelLoopState. */
        template<typename TSource>
        static ParallelLoopResult ForEach(const std::vector<TSource>& source,
                                           std::function<void(TSource, ParallelLoopState&)> body) {
#if defined(__EMSCRIPTEN__)
            (void)source; (void)body;
            throw System::PlatformNotSupportedException("Parallel::ForEach requires pthreads (not available in Emscripten single-threaded build)");
#else
            ParallelLoopState state;
            std::vector<std::future<void>> futures;
            for (TSource item : source) {
                if (state.getShouldExitCurrentIterationProperty()) break;
                futures.push_back(std::async(std::launch::async, [body, item, state]() mutable { body(item, state); }));
            }
            std::vector<std::exception_ptr> exceptions;
            waitAllCollectingExceptions(futures, exceptions);
            if (!exceptions.empty()) throw System::AggregateException(std::move(exceptions));
            ParallelLoopResult result;
            result.isCompleted_ = !state.getIsStoppedProperty();
            return result;
#endif
        }

        /** Executes all actions in parallel, blocking until all have completed. */
        static void Invoke(std::initializer_list<std::function<void()>> actions) {
#if defined(__EMSCRIPTEN__)
            (void)actions;
            throw System::PlatformNotSupportedException("Parallel::Invoke requires pthreads (not available in Emscripten single-threaded build)");
#else
            std::vector<std::future<void>> futures;
            for (auto& a : actions)
                futures.push_back(std::async(std::launch::async, a));
            std::vector<std::exception_ptr> exceptions;
            waitAllCollectingExceptions(futures, exceptions);
            if (!exceptions.empty()) throw System::AggregateException(std::move(exceptions));
#endif
        }
    };

} // namespace System::Threading::Tasks
