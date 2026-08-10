// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <chrono>
#include <cstddef>
#include <string>
#include <thread>
#include <vector>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentException.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/IDisposable.hpp"

namespace System::Threading {

    using SharpRuntime::intcs;

    /** Abstract base class for OS synchronisation handles. */
    class WaitHandle : public System::IDisposable {
    public:
        /** Return value from a timed wait that expired before the handle was signalled. */
        static constexpr intcs WaitTimeout = 258;   // WAIT_TIMEOUT on Windows
        /** Sentinel for an invalid native handle. */
        static constexpr intcs InvalidHandle = -1;

        /**
         * @brief Validates a millisecondsTimeout argument for WaitOne-style methods.
         *
         * C++ counterpart of the bounds check in .NET WaitHandle.WaitOne(int), shared by
         * every timed wait method in this hierarchy (and by the non-WaitHandle-derived
         * Mutex/Semaphore/SemaphoreSlim/ManualResetEventSlim wait methods).
         * @throws System::ArgumentOutOfRangeException if @p millisecondsTimeout is less than -1.
         */
        static void ValidateTimeout(intcs millisecondsTimeout) {
            System::ArgumentOutOfRangeException::ThrowIfLessThan(millisecondsTimeout, static_cast<intcs>(-1), "millisecondsTimeout");
        }

    private:
        // Argument validation shared by the four static multi-wait entry points.
        //
        // Transcribed from WaitHandle.cs's WaitMultiple, which validates in this order:
        // the array reference, then an empty array (ArgumentException,
        // SR.Argument_EmptyWaithandleArray, paramName "waitHandles"), then the
        // MaxWaitHandles ceiling, then the timeout, then each element for null
        // (ArgumentNullException with paramName "waitHandles[i]" and
        // SR.ArgumentNull_ArrayElement). The array-reference check has no counterpart here
        // -- these overloads take `const std::vector<WaitHandle*>&`, which cannot be null --
        // and the MaxWaitHandles ceiling is deliberately NOT reproduced: it exists because
        // Win32 WaitForMultipleObjects accepts at most 64 handles, and this port waits
        // sequentially with no such limit, so rejecting a 65-handle collection would refuse
        // input that works. SR-AUD-183's report notes the missing ceiling as an
        // undocumented portability boundary rather than a defect; it is documented here.
        //
        // Ticket #1952 / SR-AUD-183, cause T-C of docs/ThreadingNamespaceReviewPlan.md.
        static void requireNonEmptyHandles(const std::vector<WaitHandle*>& waitHandles) {
            if (waitHandles.empty())
                throw System::ArgumentException("Waithandle array may not be empty.", "waitHandles");
        }

        static void requireNoNullHandles(const std::vector<WaitHandle*>& waitHandles) {
            for (std::size_t i = 0; i < waitHandles.size(); ++i) {
                if (waitHandles[i] == nullptr)
                    throw System::ArgumentNullException(
                        "waitHandles[" + std::to_string(i) + "]",
                        "At least one element in the specified array was null.");
            }
        }

    public:

        /** Destroys the WaitHandle. */
        virtual ~WaitHandle() = default;

        /** Blocks the current thread until the handle is signalled. */
        virtual bool WaitOne() = 0;
        /** Blocks the current thread until the handle is signalled or millisecondsTimeout elapses. */
        virtual bool WaitOne(intcs millisecondsTimeout) = 0;

        /** Releases resources held by this WaitHandle. */
        void Dispose() override {}

        /** Closes the WaitHandle by calling Dispose. */
        void Close() { Dispose(); }

        /**
         * @brief Waits for all the elements of @p waitHandles to receive a signal.
         *
         * @throws System::ArgumentException if @p waitHandles is empty.
         * @throws System::ArgumentNullException if any element of @p waitHandles is null.
         *
         * @note Deliberate simplification: waits on each handle sequentially rather than
         * atomically multiplexing on the underlying OS objects (WaitHandle exposes no native
         * handle to multiplex on in this port). This is observably equivalent for the common
         * case of independent handles, but does not detect abandoned-mutex ordering the way
         * a true WaitForMultipleObjects call would. That adaptation covers *valid* input
         * only; invalid input is rejected exactly as .NET rejects it (#1952/SR-AUD-183).
         * .NET's `WaitAll(WaitHandle[])` delegates to the timed overload with
         * `Timeout.Infinite`, so no timeout can be invalid here and only the two collection
         * checks apply.
         */
        static bool WaitAll(const std::vector<WaitHandle*>& waitHandles) {
            requireNonEmptyHandles(waitHandles);
            requireNoNullHandles(waitHandles);
            for (WaitHandle* h : waitHandles)
                h->WaitOne();
            return true;
        }

        /**
         * @brief Waits for all the elements of @p waitHandles to receive a signal, or until millisecondsTimeout elapses.
         *
         * @throws System::ArgumentException if @p waitHandles is empty.
         * @throws System::ArgumentOutOfRangeException if @p millisecondsTimeout is less than -1.
         * @throws System::ArgumentNullException if any element of @p waitHandles is null.
         *
         * The three checks run in `WaitMultiple`'s order: empty collection, then timeout,
         * then null elements. A `-2` timeout used to return **true** for an empty
         * collection and **false** for a non-empty one -- two different wrong answers for
         * the same invalid argument -- because the negative deadline was already in the past
         * (#1952/SR-AUD-183).
         *
         * -1 (Timeout.Infinite) waits indefinitely for each handle in turn. A deadline
         * computed as now()+milliseconds(-1) would already be in the past, causing every
         * handle to time out immediately, so -1 must be special-cased.
         */
        static bool WaitAll(const std::vector<WaitHandle*>& waitHandles, intcs millisecondsTimeout) {
            requireNonEmptyHandles(waitHandles);
            ValidateTimeout(millisecondsTimeout);
            requireNoNullHandles(waitHandles);
            if (millisecondsTimeout == -1) {
                for (WaitHandle* h : waitHandles)
                    h->WaitOne();
                return true;
            }
            auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(millisecondsTimeout);
            for (WaitHandle* h : waitHandles) {
                auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
                if (remaining.count() < 0 || !h->WaitOne(static_cast<intcs>(remaining.count())))
                    return false;
            }
            return true;
        }

        /**
         * @brief Waits for any of the elements of @p waitHandles to receive a signal and returns its index.
         *
         * @throws System::ArgumentException if @p waitHandles is empty.
         * @throws System::ArgumentNullException if any element of @p waitHandles is null.
         *
         * The empty check is what makes this overload terminate at all. Its polling loop has
         * no exit condition other than a handle becoming signalled, so a collection with no
         * pollable handle -- empty, or holding only nulls, which were silently skipped --
         * slept one millisecond and retried **forever** (#1952/SR-AUD-183). Rejecting both
         * shapes at entry is the whole fix for the hang; there is no input a caller can now
         * supply that leaves this method looping with nothing to observe.
         *
         * @note Deliberate simplification: polls each handle with a short non-blocking wait in a
         * round-robin loop rather than an atomic OS-level multiplex; see WaitAll for the same caveat.
         */
        static intcs WaitAny(const std::vector<WaitHandle*>& waitHandles) {
            requireNonEmptyHandles(waitHandles);
            requireNoNullHandles(waitHandles);
            for (;;) {
                for (std::size_t i = 0; i < waitHandles.size(); ++i) {
                    if (waitHandles[i]->WaitOne(0))
                        return static_cast<intcs>(i);
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }

        /**
         * @brief Waits for any of the elements of @p waitHandles to receive a signal, or until millisecondsTimeout elapses.
         *
         * @throws System::ArgumentException if @p waitHandles is empty.
         * @throws System::ArgumentOutOfRangeException if @p millisecondsTimeout is less than -1.
         * @throws System::ArgumentNullException if any element of @p waitHandles is null.
         *
         * The infinite-timeout branch was a second unbounded loop for the same reason as the
         * no-timeout overload, and the finite branch returned WaitTimeout (258) for an empty
         * or null-only collection -- a *timeout* result for a call that could never have
         * succeeded. Both are now rejected at entry, in `WaitMultiple`'s order: empty
         * collection, then timeout, then null elements.
         *
         * -1 (Timeout.Infinite) polls indefinitely with no deadline. A deadline computed as
         * now()+milliseconds(-1) would already be in the past, causing an immediate
         * WaitTimeout, so -1 must be special-cased.
         */
        static intcs WaitAny(const std::vector<WaitHandle*>& waitHandles, intcs millisecondsTimeout) {
            requireNonEmptyHandles(waitHandles);
            ValidateTimeout(millisecondsTimeout);
            requireNoNullHandles(waitHandles);
            if (millisecondsTimeout == -1) {
                for (;;) {
                    for (std::size_t i = 0; i < waitHandles.size(); ++i) {
                        if (waitHandles[i]->WaitOne(0))
                            return static_cast<intcs>(i);
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }
            auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(millisecondsTimeout);
            for (;;) {
                for (std::size_t i = 0; i < waitHandles.size(); ++i) {
                    if (waitHandles[i]->WaitOne(0))
                        return static_cast<intcs>(i);
                }
                if (std::chrono::steady_clock::now() >= deadline)
                    return WaitTimeout;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    };

} // namespace System::Threading
