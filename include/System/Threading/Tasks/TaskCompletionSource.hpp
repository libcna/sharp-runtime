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

    template<typename TResult>
    class TaskCompletionSource {
        std::promise<TResult> promise_;
        std::shared_future<TResult> future_;
        bool completed_ = false;

    public:
        TaskCompletionSource() : future_(promise_.get_future().share()) {}

        void SetResult(const TResult& result) {
            if (completed_) throw std::invalid_argument("TaskCompletionSource already completed.");
            completed_ = true;
            promise_.set_value(result);
        }

        void SetException(std::exception_ptr ex) {
            if (completed_) throw std::invalid_argument("TaskCompletionSource already completed.");
            completed_ = true;
            promise_.set_exception(ex);
        }

        void SetCanceled() {
            if (completed_) throw std::invalid_argument("TaskCompletionSource already completed.");
            completed_ = true;
            promise_.set_exception(std::make_exception_ptr(std::runtime_error("Task was canceled.")));
        }

        bool TrySetResult(const TResult& result) {
            if (completed_) return false;
            SetResult(result); return true;
        }

        bool TrySetException(std::exception_ptr ex) {
            if (completed_) return false;
            SetException(ex); return true;
        }

        bool TrySetCanceled() {
            if (completed_) return false;
            SetCanceled(); return true;
        }

        TResult GetResult() { return future_.get(); }
    };

    template<>
    class TaskCompletionSource<void> {
        std::promise<void> promise_;
        std::shared_future<void> future_;
        bool completed_ = false;

    public:
        TaskCompletionSource() : future_(promise_.get_future().share()) {}

        void SetResult() {
            if (completed_) throw std::invalid_argument("TaskCompletionSource already completed.");
            completed_ = true;
            promise_.set_value();
        }

        void SetException(std::exception_ptr ex) {
            if (completed_) throw std::invalid_argument("TaskCompletionSource already completed.");
            completed_ = true;
            promise_.set_exception(ex);
        }

        void SetCanceled() {
            if (completed_) throw std::invalid_argument("TaskCompletionSource already completed.");
            completed_ = true;
            promise_.set_exception(std::make_exception_ptr(std::runtime_error("Task was canceled.")));
        }

        bool TrySetResult() { if (completed_) return false; SetResult(); return true; }
        bool TrySetException(std::exception_ptr ex) { if (completed_) return false; SetException(ex); return true; }
        bool TrySetCanceled() { if (completed_) return false; SetCanceled(); return true; }

        void Wait() { future_.get(); }
    };

} // namespace System::Threading::Tasks
