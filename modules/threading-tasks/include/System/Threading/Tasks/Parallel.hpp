// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <algorithm>
#include <atomic>
#include <functional>
#include <future>
#include <limits>
#include <memory>
#include <optional>
#include <thread>
#include <vector>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/AggregateException.hpp"
#include "System/ArgumentException.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
#  include "System/PlatformNotSupportedException.hpp"
#endif

namespace System::Threading::Tasks {

    using SharpRuntime::intcs;
    using SharpRuntime::longcs;

    /** @brief Stores options that configure the operation of methods on the Parallel class. */
    struct ParallelOptions {
        /**
         * Maximum number of concurrent iterations; -1 (the default) means unlimited.
         *
         * @note **Valid values are -1 and every value greater than or equal to 1.** 0 and every
         * value less than -1 are rejected with `System::ArgumentOutOfRangeException`
         * (ticket #1966, SR-AUD-232).
         *
         * @note **Where the rejection happens differs from .NET, deliberately.** .NET validates
         * in the `ParallelOptions.MaxDegreeOfParallelism` *setter*, so an invalid value can never
         * be stored. This is a public mutable data member with nowhere to put a check, so the
         * value is validated at the entry of every `Parallel` method that reads it, before any
         * iteration is dispatched. The exception type and parameter name are .NET's; only the
         * point of detection moves. Converting this field to a
         * `getMaxDegreeOfParallelismProperty()`/`setMaxDegreeOfParallelismProperty()` pair would
         * be a public source break for every consumer that writes `opts.MaxDegreeOfParallelism =
         * …`, and is deliberately **not** done here — the identical shape problem is why
         * `BoundedChannelOptions::FullMode` is approval-gated as ticket #1969.
         *
         * @note -1 keeps its documented "unlimited" meaning; this runtime realises it as
         * `std::thread::hardware_concurrency()`, which is what .NET's own default does. That is a
         * *worker* count chosen at runtime and is explicitly **not** a CLAUDE.md
         * build-resource-policy violation, which concerns build-job counts
         * (`docs/ThreadingTasksChannelsReviewPlan.md` §3.1 item 3).
         */
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
     * This is a safe simplification specifically because Parallel::For/ForEach dispatch iterations
     * in strict ascending index/source order (see Parallel below) — by the time any iteration can
     * observe and act on a Stop/Break request, every lower-indexed iteration has already been
     * dispatched, so Break()'s ".NET guarantee that lower iterations still run" holds automatically
     * without needing separate bookkeeping for it.
     */
    class ParallelLoopState {
        std::shared_ptr<std::atomic<bool>> stopped_ = std::make_shared<std::atomic<bool>>(false);
        std::shared_ptr<std::atomic<bool>> exceptional_ = std::make_shared<std::atomic<bool>>(false);
        // Verified against ParallelLoopState.cs's internal Break<TInt>(iteration, pflags): real
        // .NET tracks the lowest iteration index that ever called Break(), exposed via
        // ParallelLoopResult.LowestBreakIteration. This port's Break() previously only set
        // stopped_ and never touched this value at all -- getLowestBreakIterationProperty() on
        // the returned ParallelLoopResult always returned nullopt even when Break() genuinely ran,
        // a dead/unpopulated property with no test coverage beyond its own default value. Fixed by
        // tracking the minimum via a shared atomic, with currentIteration_ set by the Parallel
        // dispatcher (via the friend declaration below) immediately before invoking each
        // iteration's body -- mirroring real .NET's own per-iteration state object design.
        std::shared_ptr<std::atomic<longcs>> lowestBreakIteration_ =
            std::make_shared<std::atomic<longcs>>(std::numeric_limits<longcs>::max());
        longcs currentIteration_ = 0;

        friend class Parallel;
        void setCurrentIterationForBreakTracking(longcs i) { currentIteration_ = i; }

    public:
        /** Requests that the loop stop scheduling further iterations immediately. */
        void Stop() { stopped_->store(true); }
        /**
         * Requests that the loop stop scheduling iterations beyond the current one, and records
         * this iteration's index if it is lower than any previously recorded Break() index (see
         * ParallelLoopResult::getLowestBreakIterationProperty()).
         */
        void Break() {
            stopped_->store(true);
            longcs current = lowestBreakIteration_->load();
            while (currentIteration_ < current) {
                if (lowestBreakIteration_->compare_exchange_weak(current, currentIteration_)) break;
                // compare_exchange_weak refreshes `current` with the latest value on failure;
                // loop re-checks whether currentIteration_ is still lower before retrying.
            }
        }
        /** Marks the loop as having encountered an exception in some iteration. */
        void SetExceptional() { exceptional_->store(true); }
        /** Returns true if the current iteration should exit (Stop/Break was requested by any iteration). */
        [[nodiscard]] bool getShouldExitCurrentIterationProperty() const { return stopped_->load(); }
        /** Returns true if Stop() or Break() has been called by any iteration. */
        [[nodiscard]] bool getIsStoppedProperty() const { return stopped_->load(); }
        /** Returns true if any iteration has thrown an unhandled exception. */
        [[nodiscard]] bool getIsExceptionalProperty() const { return exceptional_->load(); }

        /** Returns the lowest iteration index from which Break() was called, or nullopt if none. */
        [[nodiscard]] std::optional<longcs> getLowestBreakIterationProperty() const {
            longcs v = lowestBreakIteration_->load();
            if (v == std::numeric_limits<longcs>::max()) return std::nullopt;
            return v;
        }
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

        // Verified against the sibling For(..., ParallelOptions, ...) overload just below,
        // which already batches std::async launches by this value. This port's other three
        // loop-shaped overloads (For with ParallelLoopState, and both ForEach variants) had
        // no batching at all -- they launched one std::async per iteration/item unconditionally,
        // unlike real .NET's TPL (which always partitions work across a bounded thread pool
        // regardless of which overload is called). For a large source collection (e.g.
        // ForEach(vectorOf50000Items, body)), this attempted to spawn up to 50,000 real OS
        // threads simultaneously -- libstdc++'s std::async does not pool threads. Past the OS
        // thread-creation limit, std::async itself throws std::system_error *at the call site*,
        // which was not wrapped in any try/catch (only f.get() inside
        // waitAllCollectingExceptions was guarded) -- that exception propagated unhandled
        // instead of the documented AggregateException contract, with any not-yet-collected
        // futures then block-joining on stack unwind.
        static intcs DefaultMaxDegreeOfParallelism() {
            intcs maxDeg = static_cast<intcs>(std::thread::hardware_concurrency());
            if (maxDeg < 1) maxDeg = 1;
            return maxDeg;
        }

        // Ticket #1965 (SR-AUD-231, cause TC-A). Every loop-shaped entry point below decides the
        // emptiness of its body at the public boundary, before any iteration is dispatched,
        // exactly as .NET's Parallel.For/ForEach do with
        // `ArgumentNullException.ThrowIfNull(body)`. Before this check the empty std::function
        // was launched once per iteration and each invocation raised std::bad_function_call,
        // which this class then collected into an AggregateException -- a *delayed*, iteration-
        // count-dependent report of an argument error: an empty range or an empty source ran no
        // iteration at all and returned a normally completed ParallelLoopResult.
        template<typename TBody>
        static void requireNonEmptyBody(const TBody& body) {
            if (body == nullptr) throw System::ArgumentNullException("body");
        }

        // Ticket #1966 (SR-AUD-232, cause TC-B/1). Before this check every
        // MaxDegreeOfParallelism <= 0 was silently rewritten to hardware_concurrency(), so 0 and
        // -2 ran a completed loop where .NET throws. Two consequences, both measured
        // (build-probe/1966_probe1_parallel_degree.cpp): the caller's configuration error was
        // lost, and the substitution raised the effective degree to the machine's core count --
        // the opposite of what a degree cap is for.
        //
        // ORDERING: this runs BEFORE requireNonEmptyBody. In .NET the invalid degree is rejected
        // by the ParallelOptions setter, which necessarily executes before Parallel.For is
        // called and therefore before .NET's own `body` null check; putting the checks the other
        // way round would report the body error for a call .NET answers with the degree error.
        // Same shape as docs/ThreadingNamespaceReviewPlan.md 17.3's constraint on #1954.
        static void requireValidMaxDegreeOfParallelism(intcs maxDegree) {
            if (maxDegree == 0 || maxDegree < -1)
                throw System::ArgumentOutOfRangeException("MaxDegreeOfParallelism");
        }

        // Resolves a validated MaxDegreeOfParallelism to the batch bound this runtime uses.
        // Only -1 ("unlimited") is substituted; every value >= 1 is honoured exactly.
        static intcs resolveMaxDegreeOfParallelism(intcs maxDegree) {
            return maxDegree == -1 ? DefaultMaxDegreeOfParallelism() : maxDegree;
        }

    public:
        /**
         * Executes a for loop from fromInclusive to toExclusive in parallel.
         * @throws System::ArgumentNullException if @p body is empty (parameter name `body`).
         */
        static ParallelLoopResult For(intcs fromInclusive, intcs toExclusive, std::function<void(intcs)> body) {
            return For(fromInclusive, toExclusive, ParallelOptions{}, std::move(body));
        }

        /**
         * Executes a for loop in parallel, respecting MaxDegreeOfParallelism in @p opts.
         *
         * @throws System::ArgumentOutOfRangeException if `opts.MaxDegreeOfParallelism` is 0 or
         * less than -1 (parameter name `MaxDegreeOfParallelism`) — checked first, because .NET
         * rejects those values in the options setter, which runs before this call. No iteration
         * is dispatched when it throws.
         * @throws System::ArgumentNullException if @p body is empty (parameter name `body`).
         */
        static ParallelLoopResult For(intcs fromInclusive, intcs toExclusive, const ParallelOptions& opts,
                                       std::function<void(intcs)> body) {
            requireValidMaxDegreeOfParallelism(opts.MaxDegreeOfParallelism);
            requireNonEmptyBody(body);
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
            (void)fromInclusive; (void)toExclusive; (void)opts; (void)body;
            throw System::PlatformNotSupportedException("Parallel::For requires pthreads (not available in Emscripten single-threaded build)");
#else
            const intcs maxDeg = resolveMaxDegreeOfParallelism(opts.MaxDegreeOfParallelism);

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
         * @throws System::ArgumentNullException if @p body is empty (parameter name `body`).
         */
        static ParallelLoopResult For(intcs fromInclusive, intcs toExclusive,
                                       std::function<void(intcs, ParallelLoopState&)> body) {
            requireNonEmptyBody(body);
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
            (void)fromInclusive; (void)toExclusive; (void)body;
            throw System::PlatformNotSupportedException("Parallel::For requires pthreads (not available in Emscripten single-threaded build)");
#else
            intcs maxDeg = DefaultMaxDegreeOfParallelism();
            ParallelLoopState state;
            std::vector<std::future<void>> futures;
            std::vector<std::exception_ptr> exceptions;
            for (intcs i = fromInclusive; i < toExclusive; ++i) {
                if (state.getShouldExitCurrentIterationProperty()) break;
                state.setCurrentIterationForBreakTracking(i);
                futures.push_back(std::async(std::launch::async, [body, i, state]() mutable { body(i, state); }));
                if (static_cast<intcs>(futures.size()) >= maxDeg) {
                    waitAllCollectingExceptions(futures, exceptions);
                    futures.clear();
                }
            }
            waitAllCollectingExceptions(futures, exceptions);
            if (!exceptions.empty()) throw System::AggregateException(std::move(exceptions));
            ParallelLoopResult result;
            result.isCompleted_ = !state.getIsStoppedProperty();
            result.lowestBreakIteration_ = state.getLowestBreakIterationProperty();
            return result;
#endif
        }

        /**
         * Executes a foreach loop over source in parallel.
         * @throws System::ArgumentNullException if @p body is empty (parameter name `body`).
         */
        template<typename TSource>
        static ParallelLoopResult ForEach(const std::vector<TSource>& source, std::function<void(TSource)> body) {
            requireNonEmptyBody(body);
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
            (void)source; (void)body;
            throw System::PlatformNotSupportedException("Parallel::ForEach requires pthreads (not available in Emscripten single-threaded build)");
#else
            intcs maxDeg = DefaultMaxDegreeOfParallelism();
            std::vector<std::future<void>> futures;
            std::vector<std::exception_ptr> exceptions;
            // Capture item by value to avoid dangling reference after loop iteration.
            for (TSource item : source) {
                futures.push_back(std::async(std::launch::async, [body, item]{ body(item); }));
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
         * Executes a foreach loop over source in parallel, passing each iteration a ParallelLoopState.
         * @throws System::ArgumentNullException if @p body is empty (parameter name `body`).
         */
        template<typename TSource>
        static ParallelLoopResult ForEach(const std::vector<TSource>& source,
                                           std::function<void(TSource, ParallelLoopState&)> body) {
            requireNonEmptyBody(body);
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
            (void)source; (void)body;
            throw System::PlatformNotSupportedException("Parallel::ForEach requires pthreads (not available in Emscripten single-threaded build)");
#else
            intcs maxDeg = DefaultMaxDegreeOfParallelism();
            ParallelLoopState state;
            std::vector<std::future<void>> futures;
            std::vector<std::exception_ptr> exceptions;
            // Real .NET tracks LowestBreakIteration for ForEach as the element's position within
            // the (order-preserving) source enumerable, not a value from TSource itself.
            longcs sourceIndex = 0;
            for (TSource item : source) {
                if (state.getShouldExitCurrentIterationProperty()) break;
                state.setCurrentIterationForBreakTracking(sourceIndex++);
                futures.push_back(std::async(std::launch::async, [body, item, state]() mutable { body(item, state); }));
                if (static_cast<intcs>(futures.size()) >= maxDeg) {
                    waitAllCollectingExceptions(futures, exceptions);
                    futures.clear();
                }
            }
            waitAllCollectingExceptions(futures, exceptions);
            if (!exceptions.empty()) throw System::AggregateException(std::move(exceptions));
            ParallelLoopResult result;
            result.isCompleted_ = !state.getIsStoppedProperty();
            result.lowestBreakIteration_ = state.getLowestBreakIterationProperty();
            return result;
#endif
        }

        /**
         * Executes all actions in parallel, blocking until all have completed.
         *
         * @throws System::ArgumentException — **not** ArgumentNullException — if any element of
         * @p actions is empty. This is the one Parallel entry whose .NET counterpart does not
         * report a null delegate as a null *argument*: `Parallel.Invoke(params Action[] actions)`
         * copies the array and rejects a null element with
         * `new ArgumentException(SR.Parallel_Invoke_ActionNull)`, i.e. an ArgumentException with
         * no parameter name, because the null is an *element* of the argument rather than the
         * argument itself. The message text below is .NET's own. Recorded as ticket #1965's
         * counterpart to `docs/ThreadingNamespaceReviewPlan.md` §17.1: a member of the CCF-011
         * family must take .NET's answer for its own shape, not the family's usual spelling.
         *
         * Every element is validated before any action is launched, so a rejected call starts no
         * work at all — where previously the valid actions in a mixed list ran to completion and
         * only then was an AggregateException raised for the empty one.
         */
        static void Invoke(std::initializer_list<std::function<void()>> actions) {
            for (const auto& a : actions) {
                if (a == nullptr) throw System::ArgumentException("One of the actions was null.");
            }
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
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
