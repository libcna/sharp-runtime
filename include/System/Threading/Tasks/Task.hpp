// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/OperationCanceledException.hpp"
#include "System/Threading/CancellationToken.hpp"
#include "System/Threading/Tasks/TaskCanceledException.hpp"
#include "System/Threading/Tasks/TaskStatus.hpp"
#if defined(__EMSCRIPTEN__)
#  include "System/PlatformNotSupportedException.hpp"
#endif

namespace System::Threading::Tasks {

    using SharpRuntime::intcs;

    class TaskFactory;
    template<typename TResult> class TaskT;

    // Lightweight Task stub backed by std::future<void>.
    // State is held in a shared_ptr so the async lambda never captures `this`.
    class Task {
        struct State {
            std::atomic<bool> isCompleted{false};
            std::atomic<bool> isCanceled{false};
            std::atomic<bool> isFaulted{false};
            std::exception_ptr exception;
        };

        // shared_future, not future: a Task's shared_ptr-based state is designed to be copied
        // and handed to multiple consumers (matching real .NET's Task, which supports being
        // awaited/Wait()'d from more than one caller) -- but std::future::get() is explicitly
        // documented as unsafe for concurrent calls on the same future instance. Confirmed via
        // a standalone ThreadSanitizer repro before this fix: two threads calling Wait() on
        // copies of the same Task (sharing the same underlying future_ via shared_ptr) raced
        // inside std::future<void>::get()'s internal state teardown. shared_future::get() is
        // documented safe for concurrent calls (it doesn't consume/invalidate shared state).
        std::shared_ptr<std::shared_future<void>> future_;
        std::shared_ptr<State>             state_;
        System::Threading::CancellationToken cancellationToken_ = System::Threading::CancellationToken::None();

        // Tag-dispatched private constructor for FromExternalFuture() below: a fresh, NOT-yet-
        // completed State with no future_ and no launched action -- distinct from the public
        // default constructor above, which is intentionally already-completed.
        struct PendingTag {};
        explicit Task(PendingTag) : state_(std::make_shared<State>()) {}

    public:
        /** Constructs an already-completed Task (equivalent to Task.CompletedTask). */
        Task() : state_(std::make_shared<State>()) { state_->isCompleted = true; }

        /**
         * Constructs and immediately starts a Task that executes @p action on a thread pool thread.
         * On Emscripten, throws PlatformNotSupportedException.
         * @param action The work to execute asynchronously.
         */
        explicit Task(std::function<void()> action) {
#if defined(__EMSCRIPTEN__)
            (void)action;
            throw System::PlatformNotSupportedException("Task: std::async requires pthreads (not available in Emscripten single-threaded build)");
#else
            state_ = std::make_shared<State>();
            auto s = state_;
            future_ = std::make_shared<std::shared_future<void>>(
                std::async(std::launch::async, [action, s]() {
                    try {
                        action();
                        s->isCompleted = true;
                    } catch (...) {
                        s->exception   = std::current_exception();
                        s->isFaulted   = true;
                        s->isCompleted = true;
                    }
                }).share()
            );
#endif
        }

        /**
         * Constructs and immediately starts a Task that executes @p action, cooperatively observing
         * @p token. If @p token is already canceled, the Task is created directly in the Canceled
         * state without launching a thread. Otherwise, @p action is expected to check the token
         * itself (e.g. via CancellationToken::ThrowIfCancellationRequested()); an OperationCanceledException
         * escaping @p action while @p token reports cancellation requested transitions the Task to
         * Canceled rather than Faulted, matching .NET's cooperative-cancellation contract.
         * On Emscripten, throws PlatformNotSupportedException.
         */
        Task(std::function<void()> action, System::Threading::CancellationToken token)
            : cancellationToken_(token)
        {
#if defined(__EMSCRIPTEN__)
            (void)action;
            throw System::PlatformNotSupportedException("Task: std::async requires pthreads (not available in Emscripten single-threaded build)");
#else
            state_ = std::make_shared<State>();
            if (token.getIsCancellationRequestedProperty()) {
                state_->isCanceled  = true;
                state_->isCompleted = true;
                return;
            }
            auto s = state_;
            future_ = std::make_shared<std::shared_future<void>>(
                std::async(std::launch::async, [action, s, token]() {
                    try {
                        action();
                        s->isCompleted = true;
                    } catch (const System::OperationCanceledException&) {
                        if (token.getIsCancellationRequestedProperty()) {
                            s->isCanceled = true;
                        } else {
                            s->exception = std::current_exception();
                            s->isFaulted = true;
                        }
                        s->isCompleted = true;
                    } catch (...) {
                        s->exception   = std::current_exception();
                        s->isFaulted   = true;
                        s->isCompleted = true;
                    }
                }).share()
            );
#endif
        }

        /** Returns true when the task has finished (successfully, faulted, or canceled). */
        [[nodiscard]] bool getIsCompletedProperty()            const { return state_->isCompleted; }
        /** Returns true when the task was canceled via a CancellationToken. */
        [[nodiscard]] bool getIsCanceledProperty()             const { return state_->isCanceled; }
        /** Returns true when the task threw an unhandled exception. */
        [[nodiscard]] bool getIsFaultedProperty()              const { return state_->isFaulted; }
        /** Returns true when the task completed without faulting or being canceled. */
        [[nodiscard]] bool getIsCompletedSuccessfullyProperty() const {
            return state_->isCompleted && !state_->isFaulted && !state_->isCanceled;
        }
        /** Returns the CancellationToken associated with this task (CancellationToken::None() if none was supplied). */
        [[nodiscard]] const System::Threading::CancellationToken& getCancellationTokenProperty() const {
            return cancellationToken_;
        }
        /** Returns the current lifecycle stage of this task. */
        [[nodiscard]] TaskStatus getStatusProperty() const {
            if (!state_->isCompleted) return TaskStatus::Running;
            if (state_->isCanceled)   return TaskStatus::Canceled;
            if (state_->isFaulted)    return TaskStatus::Faulted;
            return TaskStatus::RanToCompletion;
        }

        /**
         * Blocks until the task finishes; re-throws any stored exception, or throws
         * TaskCanceledException if the task was canceled.
         *
         * @note Verified against Task.cs's Wait()/ThrowIfExceptional(true)/GetExceptions(true):
         * real .NET throws an AggregateException wrapping a TaskCanceledException for a
         * canceled task with no other exception. This port's Wait() rethrows a faulted task's
         * stored exception directly rather than wrapping it in an AggregateException (an
         * established, deliberate simplification throughout this Task port — see the existing
         * FromException/Wait regression tests), so the canceled case follows the same
         * convention rather than introducing an inconsistent wrapping just for this path.
         */
        void Wait() {
            if (future_ && future_->valid()) future_->get();
            if (state_->isFaulted && state_->exception) std::rethrow_exception(state_->exception);
            if (state_->isCanceled) throw System::Threading::Tasks::TaskCanceledException();
        }

        /**
         * Creates and starts a new Task that runs @p action asynchronously.
         * @param action The work to execute.
         * @return The started Task.
         */
        static Task Run(std::function<void()> action) { return Task(std::move(action)); }

        /** Creates and starts a new Task that runs @p action asynchronously, observing @p token. */
        static Task Run(std::function<void()> action, System::Threading::CancellationToken token) {
            return Task(std::move(action), std::move(token));
        }

        /** Returns an already-completed Task. */
        static Task CompletedTask() { return Task(); }

        /**
         * @brief Gets the default TaskFactory for this runtime.
         *
         * C++ counterpart of .NET Task.Factory. Declared here and defined out-of-line in
         * TaskFactory.hpp (forward-declaration pattern, since TaskFactory itself constructs Tasks).
         */
        static TaskFactory Factory();

        /**
         * Creates a Task that is already in the Faulted state with @p ex as its exception.
         * @param ex Exception to store.
         */
        static Task FromException(std::exception_ptr ex) {
            Task t;
            t.state_->isFaulted   = true;
            t.state_->isCompleted = true;
            t.state_->exception   = ex;
            return t;
        }

        /**
         * Creates a Task that is already in the Canceled state.
         * @param token The CancellationToken associated with the cancellation; retrievable via getCancellationTokenProperty().
         */
        static Task FromCanceled(CancellationToken token) {
            Task t;
            t.cancellationToken_   = token;
            t.state_->isCanceled  = true;
            t.state_->isCompleted = true;
            return t;
        }

        /**
         * Internal: constructs a Task bound to an externally-completed future, with no action of
         * its own to run. Used by TaskCompletionSource<void>::getTaskProperty() to bridge that
         * type's producer-driven completion model onto the ordinary Task consumer-facing API.
         * Not part of the public .NET-mirroring surface (real .NET has no equivalent public
         * factory -- this exists purely because this port's Task/TaskT always launches an action
         * immediately on construction, unlike real .NET's Task, which supports an internal
         * "pending" construction mode TaskCompletionSource's own Task property is built on).
         *
         * @note Wraps @p sharedFuture in a NEW std::async task (rather than storing it directly
         * as future_ and mirroring its outcome from a separately-detached watcher thread, an
         * earlier version of this method) so that state mutation happens INSIDE the same async
         * lambda whose completion future_->get() blocks on -- exactly the invariant every other
         * Task constructor above already relies on (see e.g. the std::function<void()>
         * constructor). The earlier detached-thread version raced Wait()'s own future_->get()
         * against the watcher thread's separate sharedFuture->get(), with no ordering guarantee
         * that state_->isCompleted was set before Wait() returned -- caught by a real, flaky
         * test failure (TaskCompletionSourceTests.GetTaskProperty_AfterSetResult_*) before this
         * fix, not merely a theoretical concern.
         *
         * @param cancellationFlag Optional. When non-null and observed true after @p
         * sharedFuture throws, the resulting Task is marked Canceled instead of Faulted,
         * regardless of the caught exception's own type/content. An earlier version of this
         * method used `catch (const TaskCanceledException&)` to detect cancellation, which
         * collided with TaskCompletionSource::SetException(ex) being called with a
         * caller-supplied TaskCanceledException: since TrySetCanceled() also completes the
         * promise with a TaskCanceledException internally, the watcher couldn't tell the two
         * apart by type alone, silently discarding the caller's real exception and its message
         * (confirmed via a standalone repro, 2026-07-14). The producer side (e.g.
         * TaskCompletionSource::TrySetCanceled()) sets this flag itself, out of band from the
         * exception, so only an ACTUAL cancellation sets it -- a caller-supplied
         * TaskCanceledException passed to SetException() now correctly faults instead.
         */
        static Task FromExternalFuture(std::shared_ptr<std::shared_future<void>> sharedFuture,
                                        std::shared_ptr<std::atomic<bool>> cancellationFlag = nullptr) {
            Task t(PendingTag{});
            auto s = t.state_;
            t.future_ = std::make_shared<std::shared_future<void>>(
                std::async(std::launch::async, [sharedFuture, s, cancellationFlag]() {
                    try {
                        sharedFuture->get();
                        s->isCompleted = true;
                    } catch (...) {
                        if (cancellationFlag && cancellationFlag->load()) {
                            s->isCanceled = true;
                        } else {
                            s->exception = std::current_exception();
                            s->isFaulted = true;
                        }
                        s->isCompleted = true;
                    }
                }).share()
            );
            return t;
        }

        /**
         * Creates a Task that completes when all of the provided tasks have completed.
         *
         * C++ counterpart of .NET Task.WhenAll(IEnumerable&lt;Task&gt;). All input tasks are
         * waited on to completion (not short-circuited on the first fault, matching real .NET).
         * If one or more faulted, the FIRST fault encountered (in input order) is rethrown when
         * the returned Task is later Wait()'d — unlike real .NET, which wraps ALL faulted tasks'
         * exceptions in a single AggregateException. This follows the same "rethrow directly,
         * don't wrap in AggregateException" simplification already established by this class's
         * own Wait() (see its doc-comment). If no task faulted but at least one was canceled,
         * the returned Task's Wait() throws TaskCanceledException — as a consequence of routing
         * through the existing action-based Task constructor (which has no way to directly set
         * the Canceled state from inside the action), the returned Task reports Faulted (not
         * Canceled) via getStatusProperty()/getIsCanceledProperty() in that case, a further
         * simplification versus real .NET's exact Canceled-state propagation.
         *
         * @param tasks The tasks to wait on. Real .NET's Task.WhenAll(Array.Empty&lt;Task&gt;())
         *              returns an already-completed task rather than throwing — verified against
         *              the .NET reference — so an empty @p tasks is likewise valid here and
         *              returns CompletedTask() directly without spawning a thread.
         * @return A Task that completes once every task in @p tasks has completed.
         */
        static Task WhenAll(std::vector<Task> tasks) {
            if (tasks.empty()) return Task::CompletedTask();
            return Task([tasks]() mutable {
                std::exception_ptr firstFault;
                bool anyCanceled = false;
                for (auto& t : tasks) {
                    try {
                        t.Wait();
                    } catch (const System::Threading::Tasks::TaskCanceledException&) {
                        anyCanceled = true;
                    } catch (...) {
                        if (!firstFault) firstFault = std::current_exception();
                    }
                }
                if (firstFault) std::rethrow_exception(firstFault);
                if (anyCanceled) throw System::Threading::Tasks::TaskCanceledException();
            });
        }

        /**
         * Creates a TaskT<Task> that completes with the first task in @p tasks to reach a
         * terminal state (RanToCompletion, Faulted, OR Canceled).
         *
         * C++ counterpart of .NET Task.WhenAny(IEnumerable&lt;Task&gt;). Matches real .NET's own
         * documented contract exactly: the RETURNED task always itself completes successfully
         * with its Result set to the first-completed input Task -- even if that inner task
         * faulted or was canceled. The caller inspects the winning Task's own status/exception
         * separately (e.g. via getIsFaultedProperty() or Wait()); this wrapper never propagates
         * the winning task's own exception.
         *
         * Body is defined out-of-line, after TaskT's own definition later in this file -- same
         * forward-declaration pattern Factory() already uses, since TaskT<Task> can only be
         * instantiated once TaskT's full template definition is visible.
         *
         * @param tasks The tasks to wait on. Must be non-empty.
         * @throws System::ArgumentException if @p tasks is empty, matching real .NET.
         */
        static TaskT<Task> WhenAny(std::vector<Task> tasks);

        /**
         * Creates a Task that completes after the specified delay in milliseconds.
         * On Emscripten, throws PlatformNotSupportedException.
         * @param milliseconds Delay duration in milliseconds. -1 is accepted (matching real
         * .NET's "no timeout" sentinel for API-surface parity) but is NOT given special infinite-
         * wait semantics here -- std::chrono::milliseconds(-1) passed to sleep_for simply returns
         * immediately, unlike real .NET's genuine indefinite wait. Implementing a true infinite,
         * cancellation-aware wait would need a different mechanism than this class's plain
         * thread+sleep_for model; deferred as a documented simplification.
         * @throws System::ArgumentOutOfRangeException if @p milliseconds is less than -1,
         * matching real .NET's Task.Delay(int) (verified against Task.cs).
         */
        static Task Delay(intcs milliseconds) {
            if (milliseconds < -1) {
                throw System::ArgumentOutOfRangeException("millisecondsDelay",
                    "The value needs to be either -1 (signifying an infinite timeout), 0 or a positive integer.");
            }
#if defined(__EMSCRIPTEN__)
            (void)milliseconds;
            throw System::PlatformNotSupportedException("Task::Delay requires pthreads (not available on Emscripten).");
#else
            return Task([milliseconds]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
            });
#endif
        }
    };

    /** <summary>Represents an asynchronous operation that returns a value of type TResult.</summary> */
    template<typename TResult>
    class TaskT {
        struct State {
            std::atomic<bool> isCompleted{false};
            std::atomic<bool> isCanceled{false};
            std::atomic<bool> isFaulted{false};
            std::exception_ptr exception;
            TResult result{};
        };

        // shared_future, not future -- see Task::future_'s comment above for why (safe for
        // concurrent Wait()/getResultProperty() calls from multiple threads on the same TaskT).
        std::shared_ptr<std::shared_future<TResult>> future_;
        std::shared_ptr<State>                state_;
        System::Threading::CancellationToken cancellationToken_ = System::Threading::CancellationToken::None();

        // Pre-completed constructor — used by FromResult; never launches async.
        TaskT(const TResult& value, bool /*completed*/) : state_(std::make_shared<State>()) {
            state_->result = value;
            state_->isCompleted = true;
        }

        // Tag-dispatched private constructor for FromExternalFuture() below: a fresh, NOT-yet-
        // completed State with no future_ and no launched action -- see Task::PendingTag's
        // identical comment for the full rationale.
        struct PendingTag {};
        explicit TaskT(PendingTag) : state_(std::make_shared<State>()) {}

    public:
        /**
         * Constructs and immediately starts a TaskT that executes @p func on a thread pool thread.
         * On Emscripten, throws PlatformNotSupportedException.
         * @param func Factory function that produces the result.
         */
        explicit TaskT(std::function<TResult()> func) {
#if defined(__EMSCRIPTEN__)
            (void)func;
            throw System::PlatformNotSupportedException("TaskT: std::async requires pthreads (not available in Emscripten single-threaded build)");
#else
            state_ = std::make_shared<State>();
            auto s = state_;
            future_ = std::make_shared<std::shared_future<TResult>>(
                std::async(std::launch::async, [func, s]() -> TResult {
                    try {
                        TResult r  = func();
                        s->result  = r;
                        s->isCompleted = true;
                        return r;
                    } catch (...) {
                        s->exception   = std::current_exception();
                        s->isFaulted   = true;
                        s->isCompleted = true;
                        return TResult{};
                    }
                }).share()
            );
#endif
        }

        /**
         * Constructs and immediately starts a TaskT that executes @p func, cooperatively observing
         * @p token. Mirrors Task's own cancellation-token constructor (see its doc-comment for the
         * full cooperative-cancellation contract) -- this overload was previously missing entirely,
         * an asymmetry with the non-generic Task that made cancellation unavailable for any TaskT
         * caller. On Emscripten, throws PlatformNotSupportedException.
         */
        TaskT(std::function<TResult()> func, System::Threading::CancellationToken token)
            : cancellationToken_(token)
        {
#if defined(__EMSCRIPTEN__)
            (void)func;
            throw System::PlatformNotSupportedException("TaskT: std::async requires pthreads (not available in Emscripten single-threaded build)");
#else
            state_ = std::make_shared<State>();
            if (token.getIsCancellationRequestedProperty()) {
                state_->isCanceled  = true;
                state_->isCompleted = true;
                return;
            }
            auto s = state_;
            future_ = std::make_shared<std::shared_future<TResult>>(
                std::async(std::launch::async, [func, s, token]() -> TResult {
                    try {
                        TResult r  = func();
                        s->result  = r;
                        s->isCompleted = true;
                        return r;
                    } catch (const System::OperationCanceledException&) {
                        if (token.getIsCancellationRequestedProperty()) {
                            s->isCanceled = true;
                        } else {
                            s->exception = std::current_exception();
                            s->isFaulted = true;
                        }
                        s->isCompleted = true;
                        return TResult{};
                    } catch (...) {
                        s->exception   = std::current_exception();
                        s->isFaulted   = true;
                        s->isCompleted = true;
                        return TResult{};
                    }
                }).share()
            );
#endif
        }

        /** Returns true when the task has finished. */
        [[nodiscard]] bool getIsCompletedProperty() const { return state_->isCompleted; }
        /** Returns true when the task was canceled via a CancellationToken. */
        [[nodiscard]] bool getIsCanceledProperty()  const { return state_->isCanceled; }
        /** Returns true when the task threw an unhandled exception. */
        [[nodiscard]] bool getIsFaultedProperty()   const { return state_->isFaulted; }
        /** Returns true when the task completed without faulting or being canceled. */
        [[nodiscard]] bool getIsCompletedSuccessfullyProperty() const {
            return state_->isCompleted && !state_->isFaulted && !state_->isCanceled;
        }
        /** Returns the CancellationToken associated with this task (CancellationToken::None() if none was supplied). */
        [[nodiscard]] const System::Threading::CancellationToken& getCancellationTokenProperty() const {
            return cancellationToken_;
        }
        /** Returns the current lifecycle stage of this task. */
        [[nodiscard]] TaskStatus getStatusProperty() const {
            if (!state_->isCompleted) return TaskStatus::Running;
            if (state_->isCanceled)   return TaskStatus::Canceled;
            if (state_->isFaulted)    return TaskStatus::Faulted;
            return TaskStatus::RanToCompletion;
        }

        /**
         * Blocks until the task finishes and returns the result; re-throws any stored exception,
         * or throws TaskCanceledException if the task was canceled (matching Task::Wait()'s own
         * documented convention of rethrowing directly rather than wrapping in AggregateException).
         */
        TResult getResultProperty() {
            // Read into a local instead of writing back through state_->result: with future_
            // now a shared_future, multiple threads may call getResultProperty() concurrently
            // (that's the whole point of the shared_future switch), and state_->result is a
            // plain, non-atomic member -- writing to it from every caller would just move the
            // data race here instead of fixing it. shared_future::get() itself is safe to call
            // repeatedly/concurrently and already returns the completed value.
            TResult r = (future_ && future_->valid()) ? future_->get() : state_->result;
            if (state_->isFaulted && state_->exception) std::rethrow_exception(state_->exception);
            if (state_->isCanceled) throw System::Threading::Tasks::TaskCanceledException();
            return r;
        }

        /** Waits for the task and returns its result; equivalent to getResultProperty(). */
        TResult Wait() { return getResultProperty(); }

        /**
         * Creates a TaskT that is already completed with @p value — works on all platforms.
         * @param value The result value.
         */
        static TaskT<TResult> FromResult(const TResult& value) {
            return TaskT<TResult>(value, true);
        }

        /**
         * Internal: constructs a TaskT bound to an externally-completed future, with no action
         * of its own to run. Used by TaskCompletionSource<TResult>::getTaskProperty() to bridge
         * that type's producer-driven completion model onto the ordinary TaskT consumer-facing
         * API. Not part of the public .NET-mirroring surface -- see Task::FromExternalFuture's
         * identical doc-comment for the full rationale, including why this wraps @p sharedFuture
         * in a new std::async task (state mutation inside the same future-producing lambda that
         * getResultProperty()/Wait() blocks on) rather than a separately-detached watcher thread,
         * and why @p cancellationFlag exists (a producer-set, out-of-band signal distinguishing
         * genuine cancellation from a caller-supplied TaskCanceledException passed to
         * SetException() -- exception-type sniffing alone can't tell the two apart).
         */
        static TaskT<TResult> FromExternalFuture(std::shared_ptr<std::shared_future<TResult>> sharedFuture,
                                                  std::shared_ptr<std::atomic<bool>> cancellationFlag = nullptr) {
            TaskT<TResult> t(PendingTag{});
            auto s = t.state_;
            t.future_ = std::make_shared<std::shared_future<TResult>>(
                std::async(std::launch::async, [sharedFuture, s, cancellationFlag]() -> TResult {
                    try {
                        TResult r = sharedFuture->get();
                        s->result      = r;
                        s->isCompleted = true;
                        return r;
                    } catch (...) {
                        if (cancellationFlag && cancellationFlag->load()) {
                            s->isCanceled = true;
                        } else {
                            s->exception = std::current_exception();
                            s->isFaulted = true;
                        }
                        s->isCompleted = true;
                        return TResult{};
                    }
                }).share()
            );
            return t;
        }

        /**
         * Creates and starts a new TaskT that executes @p func asynchronously.
         * On Emscripten, throws PlatformNotSupportedException.
         * @param func The work to execute.
         */
        static TaskT<TResult> Run(std::function<TResult()> func) {
            return TaskT<TResult>(std::move(func));
        }

        /** Creates and starts a new TaskT that executes @p func asynchronously, observing @p token. */
        static TaskT<TResult> Run(std::function<TResult()> func, System::Threading::CancellationToken token) {
            return TaskT<TResult>(std::move(func), std::move(token));
        }
    };

    // Out-of-line since TaskT<Task> can only be instantiated once TaskT's definition above is
    // visible -- see WhenAny's own declaration/doc-comment inside class Task for the full
    // rationale and .NET-parity contract.
    //
    // std::future/std::shared_future has no native "wait for first of N" combinator, so this
    // spawns one lightweight watcher thread per input task, each calling Wait() on its own copy
    // of that Task (shared_future::get() is documented safe for concurrent calls -- see Task's
    // own future_ comment above). The first watcher to observe its task completing wins via an
    // atomic compare-exchange and notifies a shared condition variable; every other watcher's
    // result is simply discarded. The atomic CAS (not the mutex) is what decides the winner;
    // the mutex/condition_variable pair exists purely to wake the blocked caller thread without
    // busy-polling, and is race-free by the standard wait(lock, predicate)/notify protocol (a
    // watcher can't reach notify_all() until it acquires the same mutex the waiting thread either
    // never released [if the predicate was already true] or released atomically with entering the
    // wait queue [if it wasn't] -- no window exists where a notify can be sent with no one either
    // already registered to receive it or about to see the predicate already satisfied).
    //
    // Watchers are detach()'d, never join()'d: joining would block this function on EVERY
    // watcher, including "losing" ones still waiting on a slower task -- defeating WhenAny's
    // entire "return once the first completes" contract (confirmed via a real repro: an early
    // join()-based version of this consistently took as long as the slowest input task, not the
    // fastest). Each detached watcher captures its own Task COPY (by value, not a reference into
    // this function's stack frame) plus shared_ptr copies of mutex/cv/winnerIndex, so it stays
    // fully self-contained and safe to keep running in the background after this function
    // returns -- matching real .NET's own behavior, where non-winning tasks are not canceled and
    // simply keep running independently.
    //
    // This is a real cost -- N extra OS threads per WhenAny call, on top of the N already spawned
    // by the input tasks themselves -- but matches this port's existing "simple thread-per-task,
    // not tuned for extreme contention" model used throughout this class and Channel<T>.
    inline TaskT<Task> Task::WhenAny(std::vector<Task> tasks) {
        if (tasks.empty()) {
            throw System::ArgumentException("The tasks argument contains no tasks.", "tasks");
        }
        return TaskT<Task>([tasks]() mutable -> Task {
            auto mutex = std::make_shared<std::mutex>();
            auto cv = std::make_shared<std::condition_variable>();
            auto winnerIndex = std::make_shared<std::atomic<int>>(-1);

            for (size_t i = 0; i < tasks.size(); ++i) {
                std::thread([task = tasks[i], i, mutex, cv, winnerIndex]() mutable {
                    try { task.Wait(); } catch (...) {}
                    int expected = -1;
                    if (winnerIndex->compare_exchange_strong(expected, static_cast<int>(i))) {
                        std::lock_guard<std::mutex> lock(*mutex);
                        cv->notify_all();
                    }
                }).detach();
            }

            std::unique_lock<std::mutex> lock(*mutex);
            cv->wait(lock, [&] { return winnerIndex->load() >= 0; });
            return tasks[static_cast<size_t>(winnerIndex->load())];
        });
    }

} // namespace System::Threading::Tasks
