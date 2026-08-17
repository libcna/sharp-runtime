// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include <functional>
#include <future>
#include <mutex>
#include <thread>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/Threading/IThreadPoolWorkItem.hpp"
#include "System/Threading/RegisteredWaitHandle.hpp"
#include "System/Threading/WaitCallback.hpp"
#include "System/Threading/WaitHandle.hpp"
#include "System/Threading/WaitOrTimerCallback.hpp"
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
#  include "System/PlatformNotSupportedException.hpp"
#endif

namespace System::Threading {

    using SharpRuntime::intcs;

    /** Provides a pool of threads that can be used to execute tasks, post work items, and other operations. */
    class ThreadPool {
        // Process-global thread-pool configuration, added by ticket #1971 (SR-AUD-189) so
        // SetMinThreads/SetMaxThreads actually store what they accept. Held in function-local
        // statics rather than in data members because ThreadPool is a static-only class
        // (`ThreadPool() = delete`) with no instances and therefore no object layout to change.
        struct Configuration {
            intcs minWorker = 1;
            intcs minCompletionPort = 1;
            intcs maxWorker = 1;
            intcs maxCompletionPort = 1;
        };

        static std::mutex& configurationMutex() {
            static std::mutex instance;
            return instance;
        }

        // Must be called with configurationMutex() held.
        static Configuration& mutableConfiguration() {
            static Configuration instance = [] {
                Configuration defaults;
#if !defined(__EMSCRIPTEN__) || defined(__EMSCRIPTEN_PTHREADS__)
                // The pre-#1971 default reported by GetMaxThreads, preserved exactly.
                intcs cores = static_cast<intcs>(std::thread::hardware_concurrency());
                if (cores < 1) cores = 1;
                defaults.maxWorker = cores * 2;
                defaults.maxCompletionPort = defaults.maxWorker;
#endif
                return defaults;
            }();
            return instance;
        }

        static Configuration readConfiguration() {
            std::lock_guard<std::mutex> lock(configurationMutex());
            return mutableConfiguration();
        }

    public:
        /** Prevents instantiation — all members are static. */
        ThreadPool() = delete;

        /** Queues the callback for execution on a detached thread; returns true on success. */
        static bool QueueUserWorkItem(std::function<void()> callBack) {
            if (!callBack)
                throw System::ArgumentNullException("callBack");
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
            throw System::PlatformNotSupportedException("ThreadPool::QueueUserWorkItem requires pthreads (not available in Emscripten single-threaded build)");
#else
            std::thread(std::move(callBack)).detach();
            return true;
#endif
        }

        /** Queues the callback with a state argument for execution on a detached thread; returns true on success. */
        static bool QueueUserWorkItem(std::function<void(void*)> callBack, void* state) {
            if (!callBack)
                throw System::ArgumentNullException("callBack");
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
            (void)state;
            throw System::PlatformNotSupportedException("ThreadPool::QueueUserWorkItem requires pthreads (not available in Emscripten single-threaded build)");
#else
            std::thread([callBack, state]{ callBack(state); }).detach();
            return true;
#endif
        }

        /**
         * @brief Retrieves the minimum number of threads the pool maintains.
         *
         * Reports whatever a successful SetMinThreads last stored, or the default of 1/1
         * (ticket #1971, SR-AUD-189). Before that ticket this always answered 1/1 regardless of
         * what had been set.
         */
        static void GetMinThreads(intcs& workerThreads, intcs& completionPortThreads) {
            const Configuration snapshot = readConfiguration();
            workerThreads = snapshot.minWorker;
            completionPortThreads = snapshot.minCompletionPort;
        }

        /**
         * @brief Retrieves the maximum number of concurrent threads.
         *
         * Reports whatever a successful SetMaxThreads last stored, or this runtime's default:
         * twice `std::thread::hardware_concurrency()` natively, and 1/1 on a single-threaded
         * Emscripten build. (That is a *worker* count reported by a thread-pool API, not a
         * build-job count, so CLAUDE.md's build-resource policy does not apply to it.)
         */
        static void GetMaxThreads(intcs& workerThreads, intcs& completionPortThreads) {
            const Configuration snapshot = readConfiguration();
            workerThreads = snapshot.maxWorker;
            completionPortThreads = snapshot.maxCompletionPort;
        }

        /**
         * @brief Sets the minimum number of threads the pool maintains.
         *
         * @return false without storing anything if either value is negative or exceeds the
         * corresponding current maximum; true after storing otherwise.
         *
         * Ticket #1971 (SR-AUD-189). Both setters used to return `true` unconditionally and store
         * nothing, so a caller that used the result to decide whether its concurrency policy had
         * taken effect was told "yes" every time — including for `SetMinThreads(-1, -1)`.
         *
         * Provenance of each rule, since `/rv/tmp/runtime/src/libraries/` is not available in this
         * environment and none of it may be presented as freshly read from source:
         * - **negative ⇒ false**, and **a valid pair is stored and observable through
         *   GetMinThreads**: measured by the audit's managed probe, which recorded
         *   `set_min=True after=7,9` and `invalid_min=False`.
         * - **a minimum above the corresponding maximum ⇒ false**: .NET's documented
         *   minimum/maximum consistency rule.
         * - **zero is accepted**: not measured and not documented either way; a minimum of zero
         *   is meaningful (the pool starts with no threads), so it is accepted rather than
         *   inventing a rejection .NET may not have.
         *
         * **Deliberately not adopted:** .NET additionally refuses a *maximum* below the machine's
         * processor count. It could not be verified here, and adopting it would make this port's
         * observable result depend on the core count of whatever machine runs it. Same reasoning
         * as ticket #1952's non-adoption of `MaxWaitHandles`.
         *
         * @note The stored configuration is process-global mutable state and each setter is a
         * read-modify-write across both pairs, so all four entry points share one lock.
         */
        static bool SetMinThreads(intcs workerThreads, intcs completionPortThreads) {
            std::lock_guard<std::mutex> lock(configurationMutex());
            Configuration& configuration = mutableConfiguration();
            if (workerThreads < 0 || completionPortThreads < 0) return false;
            if (workerThreads > configuration.maxWorker ||
                completionPortThreads > configuration.maxCompletionPort) {
                return false;
            }
            configuration.minWorker = workerThreads;
            configuration.minCompletionPort = completionPortThreads;
            return true;
        }

        /**
         * @brief Sets the maximum number of concurrent threads.
         *
         * @return false without storing anything if either value is less than 1 or below the
         * corresponding current minimum; true after storing otherwise. See SetMinThreads for the
         * provenance of each rule and for what was deliberately not adopted.
         */
        static bool SetMaxThreads(intcs workerThreads, intcs completionPortThreads) {
            std::lock_guard<std::mutex> lock(configurationMutex());
            Configuration& configuration = mutableConfiguration();
            if (workerThreads < 1 || completionPortThreads < 1) return false;
            if (workerThreads < configuration.minWorker ||
                completionPortThreads < configuration.minCompletionPort) {
                return false;
            }
            configuration.maxWorker = workerThreads;
            configuration.maxCompletionPort = completionPortThreads;
            return true;
        }

        /**
         * @brief Queues an IThreadPoolWorkItem's Execute() for execution on a detached thread.
         *
         * **Takes a `std::shared_ptr` since ticket #1959** (SR-AUD-187, CCF-019). This used to
         * capture a borrowed raw pointer in a DETACHED lambda and call `Execute()` on it with no
         * ownership, join or retention of any kind: the ASan probe queued a heap item, waited
         * until `Execute` had entered, deleted it, and let `Execute` touch a member --
         * heap-use-after-free. A detached thread has no owner to wait for it, so a boundary in
         * some destructor (the answer #2134 gave `Socket`) is not available here; the queue must
         * hold a share of the item instead, which is exactly what .NET's GC reference does.
         *
         * @param callBack The work item. Kept alive until `Execute()` returns.
         * @param preferLocal Ignored by this port, as it is by .NET outside its own scheduler.
         * @return true on success.
         * @throws System::ArgumentNullException if @p callBack is null.
         * @note **Source break, ticket #1959.** This took a raw `IThreadPoolWorkItem*` before.
         *       See `docs/Migration-BorrowedCallbackOwnership.md`.
         */
        static bool UnsafeQueueUserWorkItem(std::shared_ptr<IThreadPoolWorkItem> callBack,
                                             bool /*preferLocal*/) {
            if (!callBack)
                throw System::ArgumentNullException("callBack");
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
            throw System::PlatformNotSupportedException("ThreadPool::UnsafeQueueUserWorkItem requires pthreads (not available in Emscripten single-threaded build)");
#else
            // The captured shared_ptr IS the liveness boundary: the detached thread holds a share
            // for as long as the body runs, so the item cannot be destroyed under it however
            // early the caller drops its own reference.
            std::thread([callBack = std::move(callBack)]{ callBack->Execute(); }).detach();
            return true;
#endif
        }

        /**
         * @brief Registers a wait on waitObject; callBack is invoked when it is signalled or the timeout elapses.
         *
         * @throws System::ArgumentOutOfRangeException if @p millisecondsTimeOutInterval is less than -1.
         * @throws System::ArgumentNullException if @p waitObject or @p callBack is null/empty.
         *
         * The three checks reproduce .NET's own split and order: the public
         * `RegisterWaitForSingleObject(WaitHandle, WaitOrTimerCallback, object?, int, bool)`
         * overload runs `ArgumentOutOfRangeException.ThrowIfLessThan(millisecondsTimeOutInterval, -1)`
         * and then delegates to a private overload that opens with
         * `ArgumentNullException.ThrowIfNull(waitObject)` and
         * `ArgumentNullException.ThrowIfNull(callBack)`. The two null checks live in
         * RegisteredWaitHandle's constructor here for the same reason -- it is the private
         * step that captures the arguments, and putting them there means the background
         * thread cannot be created before they run (ticket #1953 / SR-AUD-188).
         *
         * @note Deliberate simplification: implemented via a dedicated background thread that polls
         * waitObject rather than true OS-level thread-pool wait registration.
         */
        static RegisteredWaitHandle RegisterWaitForSingleObject(
            WaitHandle* waitObject, WaitOrTimerCallback callBack, void* state,
            intcs millisecondsTimeOutInterval, bool executeOnlyOnce) {
            System::ArgumentOutOfRangeException::ThrowIfLessThan(
                millisecondsTimeOutInterval, static_cast<intcs>(-1), "millisecondsTimeOutInterval");
            return RegisteredWaitHandle(waitObject, std::move(callBack), state, millisecondsTimeOutInterval, executeOnlyOnce);
        }
    };

} // namespace System::Threading
