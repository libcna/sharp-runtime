// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <atomic>
#include <chrono>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <stdexcept>
#include <thread>
#include "System/OperationCanceledException.hpp"
#include "System/Threading/CancellationToken.hpp"
#include "System/Threading/Tasks/TaskCanceledException.hpp"
#include "System/Threading/Tasks/TaskStatus.hpp"
#if defined(__EMSCRIPTEN__)
#  include "System/PlatformNotSupportedException.hpp"
#endif

namespace System::Threading::Tasks {

    class TaskFactory;

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
         * Creates a Task that completes after the specified delay in milliseconds.
         * On Emscripten, throws PlatformNotSupportedException.
         * @param milliseconds Delay duration in milliseconds.
         */
        static Task Delay(int milliseconds) {
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
            std::atomic<bool> isFaulted{false};
            std::exception_ptr exception;
            TResult result{};
        };

        // shared_future, not future -- see Task::future_'s comment above for why (safe for
        // concurrent Wait()/getResultProperty() calls from multiple threads on the same TaskT).
        std::shared_ptr<std::shared_future<TResult>> future_;
        std::shared_ptr<State>                state_;

        // Pre-completed constructor — used by FromResult; never launches async.
        TaskT(const TResult& value, bool /*completed*/) : state_(std::make_shared<State>()) {
            state_->result = value;
            state_->isCompleted = true;
        }

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

        /** Returns true when the task has finished. */
        [[nodiscard]] bool getIsCompletedProperty() const { return state_->isCompleted; }
        /** Returns true when the task threw an unhandled exception. */
        [[nodiscard]] bool getIsFaultedProperty()   const { return state_->isFaulted; }

        /** Blocks until the task finishes and returns the result; re-throws any stored exception. */
        TResult getResultProperty() {
            // Read into a local instead of writing back through state_->result: with future_
            // now a shared_future, multiple threads may call getResultProperty() concurrently
            // (that's the whole point of the shared_future switch), and state_->result is a
            // plain, non-atomic member -- writing to it from every caller would just move the
            // data race here instead of fixing it. shared_future::get() itself is safe to call
            // repeatedly/concurrently and already returns the completed value.
            TResult r = (future_ && future_->valid()) ? future_->get() : state_->result;
            if (state_->isFaulted && state_->exception) std::rethrow_exception(state_->exception);
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
         * Creates and starts a new TaskT that executes @p func asynchronously.
         * On Emscripten, throws PlatformNotSupportedException.
         * @param func The work to execute.
         */
        static TaskT<TResult> Run(std::function<TResult()> func) {
            return TaskT<TResult>(std::move(func));
        }
    };

} // namespace System::Threading::Tasks
