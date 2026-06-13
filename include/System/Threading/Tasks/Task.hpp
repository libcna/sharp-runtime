// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <atomic>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <stdexcept>
#include "System/Threading/CancellationToken.hpp"
#if defined(__EMSCRIPTEN__)
#  include "System/PlatformNotSupportedException.hpp"
#endif

namespace System::Threading::Tasks {

    // Lightweight Task stub backed by std::future<void>.
    // State is held in a shared_ptr so the async lambda never captures `this`.
    class Task {
        struct State {
            std::atomic<bool> isCompleted{false};
            std::atomic<bool> isCanceled{false};
            std::atomic<bool> isFaulted{false};
            std::exception_ptr exception;
        };

        std::shared_ptr<std::future<void>> future_;
        std::shared_ptr<State>             state_;

    public:
        /// Constructs an already-completed Task (equivalent to Task.CompletedTask).
        Task() : state_(std::make_shared<State>()) { state_->isCompleted = true; }

        /// Constructs and immediately starts a Task that executes @p action on a thread pool thread.
        /// On Emscripten, throws PlatformNotSupportedException.
        /// @param action The work to execute asynchronously.
        explicit Task(std::function<void()> action) {
#if defined(__EMSCRIPTEN__)
            (void)action;
            throw System::PlatformNotSupportedException("Task: std::async requires pthreads (not available in Emscripten single-threaded build)");
#else
            state_ = std::make_shared<State>();
            auto s = state_;
            future_ = std::make_shared<std::future<void>>(
                std::async(std::launch::async, [action, s]() {
                    try {
                        action();
                        s->isCompleted = true;
                    } catch (...) {
                        s->exception   = std::current_exception();
                        s->isFaulted   = true;
                        s->isCompleted = true;
                    }
                })
            );
#endif
        }

        /// Returns true when the task has finished (successfully, faulted, or canceled).
        [[nodiscard]] bool getIsCompletedProperty()            const { return state_->isCompleted; }
        /// Returns true when the task was canceled via a CancellationToken.
        [[nodiscard]] bool getIsCanceledProperty()             const { return state_->isCanceled; }
        /// Returns true when the task threw an unhandled exception.
        [[nodiscard]] bool getIsFaultedProperty()              const { return state_->isFaulted; }
        /// Returns true when the task completed without faulting or being canceled.
        [[nodiscard]] bool getIsCompletedSuccessfullyProperty() const {
            return state_->isCompleted && !state_->isFaulted && !state_->isCanceled;
        }

        /// Blocks until the task finishes; re-throws any stored exception.
        void Wait() {
            if (future_ && future_->valid()) future_->get();
            if (state_->isFaulted && state_->exception) std::rethrow_exception(state_->exception);
        }

        /// Creates and starts a new Task that runs @p action asynchronously.
        /// @param action The work to execute.
        /// @return The started Task.
        static Task Run(std::function<void()> action) { return Task(std::move(action)); }

        /// Returns an already-completed Task.
        static Task CompletedTask() { return Task(); }

        /// Creates a Task that is already in the Faulted state with @p ex as its exception.
        /// @param ex Exception to store.
        static Task FromException(std::exception_ptr ex) {
            Task t;
            t.state_->isFaulted   = true;
            t.state_->isCompleted = true;
            t.state_->exception   = ex;
            return t;
        }

        /// Creates a Task that is already in the Canceled state.
        /// @param token CancellationToken (stored for .NET API compatibility; not observed).
        static Task FromCanceled(CancellationToken) {
            Task t;
            t.state_->isCanceled  = true;
            t.state_->isCompleted = true;
            return t;
        }

        /// Creates a Task that completes after the specified delay in milliseconds.
        /// On Emscripten, throws PlatformNotSupportedException.
        /// @param milliseconds Delay duration in milliseconds.
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

    /// <summary>Represents an asynchronous operation that returns a value of type TResult.</summary>
    template<typename TResult>
    class TaskT {
        struct State {
            std::atomic<bool> isCompleted{false};
            std::atomic<bool> isFaulted{false};
            std::exception_ptr exception;
            TResult result{};
        };

        std::shared_ptr<std::future<TResult>> future_;
        std::shared_ptr<State>                state_;

    public:
        /// Constructs and immediately starts a TaskT that executes @p func on a thread pool thread.
        /// On Emscripten, throws PlatformNotSupportedException.
        /// @param func Factory function that produces the result.
        explicit TaskT(std::function<TResult()> func) {
#if defined(__EMSCRIPTEN__)
            (void)func;
            throw System::PlatformNotSupportedException("TaskT: std::async requires pthreads (not available in Emscripten single-threaded build)");
#else
            state_ = std::make_shared<State>();
            auto s = state_;
            future_ = std::make_shared<std::future<TResult>>(
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
                })
            );
#endif
        }

        /// Returns true when the task has finished.
        [[nodiscard]] bool getIsCompletedProperty() const { return state_->isCompleted; }
        /// Returns true when the task threw an unhandled exception.
        [[nodiscard]] bool getIsFaultedProperty()   const { return state_->isFaulted; }

        /// Blocks until the task finishes and returns the result; re-throws any stored exception.
        TResult getResultProperty() {
            if (future_ && future_->valid()) state_->result = future_->get();
            if (state_->isFaulted && state_->exception) std::rethrow_exception(state_->exception);
            return state_->result;
        }

        /// Waits for the task and returns its result; equivalent to getResultProperty().
        TResult Wait() { return getResultProperty(); }

        /// Creates a TaskT that is already completed with the specified @p value.
        /// @param value The result value.
        static TaskT<TResult> FromResult(const TResult& value) {
            return TaskT<TResult>([value]() { return value; });
        }

        /// Creates and starts a new TaskT that executes @p func asynchronously.
        /// @param func The work to execute.
        static TaskT<TResult> Run(std::function<TResult()> func) {
            return TaskT<TResult>(std::move(func));
        }
    };

} // namespace System::Threading::Tasks
