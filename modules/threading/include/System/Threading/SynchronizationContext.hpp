// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>
#include "System/NullReferenceException.hpp"
#include "System/Threading/ThreadPool.hpp"

namespace System::Threading {

    /** A delegate type for sending or posting callbacks to a SynchronizationContext. */
    using SendOrPostCallback = std::function<void(void*)>;

    /**
     * @brief Provides the basic functionality required to propagate a synchronisation context
     * in various synchronisation models.
     *
     * C++ counterpart of .NET System.Threading.SynchronizationContext.
     *
     * @note Verified against SynchronizationContext.cs: (1) Post() queues the callback to the
     * thread pool (asynchronous), unlike Send() which runs it synchronously on the calling
     * thread -- this port's Post() previously ran the callback inline, identically to Send(),
     * defeating the entire purpose of the distinction. (2) Current/SetSynchronizationContext
     * read/write a per-thread field (real .NET: Thread.CurrentThread._synchronizationContext);
     * this port's getCurrentProperty() previously always returned nullptr regardless of what had been
     * set via SetSynchronizationContext(), which was itself a complete no-op -- the pair never
     * round-tripped at all.
     */
    class SynchronizationContext {
    public:
        /** Destroys the SynchronizationContext. */
        virtual ~SynchronizationContext() = default;

        /**
         * @brief Dispatches an asynchronous message to the context (queued to the thread
         * pool by default).
         *
         * An empty @p d is dropped rather than queued. .NET's base `Post` hands the
         * delegate to `ThreadPool.QueueUserWorkItem` and so *returns normally* to the
         * caller for a null delegate too; the difference is confined to a background
         * thread that has no caller to report to. This port matches the observable the
         * caller has -- deliberately, and it is not what SR-AUD-222 is about (that finding
         * names `Send` only, and explicitly records `Post(null, null)` as already matching).
         */
        virtual void Post(SendOrPostCallback d, void* state) {
            if (d) ThreadPool::QueueUserWorkItem(d, state);
        }

        /**
         * @brief Dispatches a synchronous message to the context (runs immediately on the
         * calling thread).
         *
         * @throws System::NullReferenceException if @p d is an empty std::function.
         *
         * Like LazyInitializer::EnsureInitialized, this is a CCF-011 site whose .NET answer
         * is not `ArgumentNullException`: `SynchronizationContext.Send` is declared as
         * `public virtual void Send(SendOrPostCallback d, object? state) => d(state);`
         * with no validation, so a null delegate faults with `NullReferenceException` on
         * the calling thread. This port used to test the callable and *return normally*,
         * turning a managed fault into a silent no-op that no caller could detect
         * (SR-AUD-222). Ticket #1951 / CCF-011; the exception type is .NET's own, not a
         * substitution. An overriding context that wants different behaviour still gets it
         * -- this is the base implementation only, exactly as in .NET.
         */
        virtual void Send(SendOrPostCallback d, void* state) {
            if (!d) throw System::NullReferenceException();
            d(state);
        }

        /** Returns the synchronization context set for the current thread via SetSynchronizationContext(), or nullptr if none. */
        static SynchronizationContext* getCurrentProperty() { return CurrentSlot(); }

        /** Sets the synchronization context for the current thread. */
        static void SetSynchronizationContext(SynchronizationContext* syncContext) { CurrentSlot() = syncContext; }

    private:
        static SynchronizationContext*& CurrentSlot() {
            thread_local SynchronizationContext* current = nullptr;
            return current;
        }
    };

} // namespace System::Threading
