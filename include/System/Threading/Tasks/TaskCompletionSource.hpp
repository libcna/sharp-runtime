// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <exception>
#include <future>
#include <memory>
#include <stdexcept>
#include "System/Threading/Tasks/Task.hpp"

namespace System::Threading::Tasks {

    /// Provides producer-side control over a Task's completion for a result type TResult.
    ///
    /// Wraps std::promise/std::shared_future. Partial C++ counterpart of .NET TaskCompletionSource<TResult>.
    template<typename TResult>
    class TaskCompletionSource {
        std::promise<TResult> promise_;
        std::shared_future<TResult> future_;
        bool completed_ = false; ///< True once the source has been completed.

    public:
        /// Default constructor — creates an unresolved source.
        TaskCompletionSource() : future_(promise_.get_future().share()) {}

        /// Transitions the task to a successful result.
        /// @throws std::invalid_argument if the source was already completed.
        void SetResult(const TResult& result) {
            if (completed_) throw std::invalid_argument("TaskCompletionSource already completed.");
            completed_ = true;
            promise_.set_value(result);
        }

        /// Transitions the task to a faulted state with the given exception.
        /// @throws std::invalid_argument if the source was already completed.
        void SetException(std::exception_ptr ex) {
            if (completed_) throw std::invalid_argument("TaskCompletionSource already completed.");
            completed_ = true;
            promise_.set_exception(ex);
        }

        /// Transitions the task to a canceled state.
        /// @throws std::invalid_argument if the source was already completed.
        void SetCanceled() {
            if (completed_) throw std::invalid_argument("TaskCompletionSource already completed.");
            completed_ = true;
            promise_.set_exception(std::make_exception_ptr(std::runtime_error("Task was canceled.")));
        }

        /// Attempts to set a successful result; returns false if already completed.
        bool TrySetResult(const TResult& result) {
            if (completed_) return false;
            SetResult(result); return true;
        }

        /// Attempts to fault the task; returns false if already completed.
        bool TrySetException(std::exception_ptr ex) {
            if (completed_) return false;
            SetException(ex); return true;
        }

        /// Attempts to cancel the task; returns false if already completed.
        bool TrySetCanceled() {
            if (completed_) return false;
            SetCanceled(); return true;
        }

        /// Blocks until the task completes and returns its result.
        TResult GetResult() { return future_.get(); }
    };

    /// Specialisation of TaskCompletionSource for void tasks (no result value).
    template<>
    class TaskCompletionSource<void> {
        std::promise<void> promise_;
        std::shared_future<void> future_;
        bool completed_ = false; ///< True once the source has been completed.

    public:
        /// Default constructor — creates an unresolved source.
        TaskCompletionSource() : future_(promise_.get_future().share()) {}

        /// Transitions the task to the completed state.
        /// @throws std::invalid_argument if the source was already completed.
        void SetResult() {
            if (completed_) throw std::invalid_argument("TaskCompletionSource already completed.");
            completed_ = true;
            promise_.set_value();
        }

        /// Transitions the task to a faulted state with the given exception.
        /// @throws std::invalid_argument if the source was already completed.
        void SetException(std::exception_ptr ex) {
            if (completed_) throw std::invalid_argument("TaskCompletionSource already completed.");
            completed_ = true;
            promise_.set_exception(ex);
        }

        /// Transitions the task to a canceled state.
        /// @throws std::invalid_argument if the source was already completed.
        void SetCanceled() {
            if (completed_) throw std::invalid_argument("TaskCompletionSource already completed.");
            completed_ = true;
            promise_.set_exception(std::make_exception_ptr(std::runtime_error("Task was canceled.")));
        }

        /// Attempts to complete the task; returns false if already completed.
        bool TrySetResult() { if (completed_) return false; SetResult(); return true; }

        /// Attempts to fault the task; returns false if already completed.
        bool TrySetException(std::exception_ptr ex) { if (completed_) return false; SetException(ex); return true; }

        /// Attempts to cancel the task; returns false if already completed.
        bool TrySetCanceled() { if (completed_) return false; SetCanceled(); return true; }

        /// Blocks until the task completes.
        void Wait() { future_.get(); }
    };

} // namespace System::Threading::Tasks
