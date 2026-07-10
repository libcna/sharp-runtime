// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <atomic>
#include <exception>
#include <future>
#include <memory>
#include <stdexcept>
#include "System/InvalidOperationException.hpp"
#include "System/Threading/Tasks/Task.hpp"
#include "System/Threading/Tasks/TaskCanceledException.hpp"

namespace System::Threading::Tasks {

    namespace detail {
        // Verified against TaskT_TransitionToFinal_AlreadyCompleted in Strings.resx.
        inline const char* taskCompletionSourceAlreadyCompletedMessage() {
            return "An attempt was made to transition a task to a final state when it had already completed.";
        }
    }

    /**
     * Provides producer-side control over a Task's completion for a result type TResult.
     *
     * Wraps std::promise/std::shared_future. Partial C++ counterpart of .NET TaskCompletionSource<TResult>.
     *
     * @note Matches TaskCompletionSource_T.cs's structure: the Try* methods are the atomic
     * primitives (an std::atomic compare-exchange claims the single completing thread), and
     * the non-Try Set* methods call them and throw if the claim fails -- not the reverse. The
     * previous implementation had non-atomic check-then-set logic in Set*, so two threads
     * racing a TrySet* call could both observe "not yet completed" and both proceed to call
     * promise_.set_value()/set_exception(), and the loser would throw an uncaught
     * std::future_error instead of TrySet* returning false as .NET guarantees.
     */
    template<typename TResult>
    class TaskCompletionSource {
        std::promise<TResult> promise_;
        std::shared_future<TResult> future_;
        std::atomic<bool> completed_{false}; ///< True once the source has been completed.

    public:
        /** Default constructor — creates an unresolved source. */
        TaskCompletionSource() : future_(promise_.get_future().share()) {}

        /**
         * Transitions the task to a successful result.
         * @throws System::InvalidOperationException if the source was already completed.
         */
        void SetResult(const TResult& result) {
            if (!TrySetResult(result))
                throw System::InvalidOperationException(detail::taskCompletionSourceAlreadyCompletedMessage());
        }

        /**
         * Transitions the task to a faulted state with the given exception.
         * @throws System::InvalidOperationException if the source was already completed.
         */
        void SetException(std::exception_ptr ex) {
            if (!TrySetException(ex))
                throw System::InvalidOperationException(detail::taskCompletionSourceAlreadyCompletedMessage());
        }

        /**
         * Transitions the task to a canceled state.
         * @throws System::InvalidOperationException if the source was already completed.
         */
        void SetCanceled() {
            if (!TrySetCanceled())
                throw System::InvalidOperationException(detail::taskCompletionSourceAlreadyCompletedMessage());
        }

        /** Attempts to set a successful result; returns false if already completed. */
        bool TrySetResult(const TResult& result) {
            bool expected = false;
            if (!completed_.compare_exchange_strong(expected, true)) return false;
            promise_.set_value(result);
            return true;
        }

        /** Attempts to fault the task; returns false if already completed. */
        bool TrySetException(std::exception_ptr ex) {
            bool expected = false;
            if (!completed_.compare_exchange_strong(expected, true)) return false;
            promise_.set_exception(ex);
            return true;
        }

        /** Attempts to cancel the task; returns false if already completed. */
        bool TrySetCanceled() {
            bool expected = false;
            if (!completed_.compare_exchange_strong(expected, true)) return false;
            promise_.set_exception(std::make_exception_ptr(System::Threading::Tasks::TaskCanceledException()));
            return true;
        }

        /** Blocks until the task completes and returns its result. */
        TResult GetResult() { return future_.get(); }
    };

    /** Specialisation of TaskCompletionSource for void tasks (no result value). */
    template<>
    class TaskCompletionSource<void> {
        std::promise<void> promise_;
        std::shared_future<void> future_;
        std::atomic<bool> completed_{false}; ///< True once the source has been completed.

    public:
        /** Default constructor — creates an unresolved source. */
        TaskCompletionSource() : future_(promise_.get_future().share()) {}

        /**
         * Transitions the task to the completed state.
         * @throws System::InvalidOperationException if the source was already completed.
         */
        void SetResult() {
            if (!TrySetResult())
                throw System::InvalidOperationException(detail::taskCompletionSourceAlreadyCompletedMessage());
        }

        /**
         * Transitions the task to a faulted state with the given exception.
         * @throws System::InvalidOperationException if the source was already completed.
         */
        void SetException(std::exception_ptr ex) {
            if (!TrySetException(ex))
                throw System::InvalidOperationException(detail::taskCompletionSourceAlreadyCompletedMessage());
        }

        /**
         * Transitions the task to a canceled state.
         * @throws System::InvalidOperationException if the source was already completed.
         */
        void SetCanceled() {
            if (!TrySetCanceled())
                throw System::InvalidOperationException(detail::taskCompletionSourceAlreadyCompletedMessage());
        }

        /** Attempts to complete the task; returns false if already completed. */
        bool TrySetResult() {
            bool expected = false;
            if (!completed_.compare_exchange_strong(expected, true)) return false;
            promise_.set_value();
            return true;
        }

        /** Attempts to fault the task; returns false if already completed. */
        bool TrySetException(std::exception_ptr ex) {
            bool expected = false;
            if (!completed_.compare_exchange_strong(expected, true)) return false;
            promise_.set_exception(ex);
            return true;
        }

        /** Attempts to cancel the task; returns false if already completed. */
        bool TrySetCanceled() {
            bool expected = false;
            if (!completed_.compare_exchange_strong(expected, true)) return false;
            promise_.set_exception(std::make_exception_ptr(System::Threading::Tasks::TaskCanceledException()));
            return true;
        }

        /** Blocks until the task completes. */
        void Wait() { future_.get(); }
    };

} // namespace System::Threading::Tasks
