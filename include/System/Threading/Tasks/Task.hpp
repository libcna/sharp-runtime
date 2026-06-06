// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <stdexcept>
#include "System/Threading/CancellationToken.hpp"

namespace System::Threading::Tasks {

    // Lightweight Task stub backed by std::future<void>.
    class Task {
        std::shared_ptr<std::future<void>> future_;
        bool isCompleted_  = false;
        bool isCanceled_   = false;
        bool isFaulted_    = false;
        std::exception_ptr exception_;

    public:
        Task() : isCompleted_(true) {}

        explicit Task(std::function<void()> action) {
            future_ = std::make_shared<std::future<void>>(
                std::async(std::launch::async, [action, this]() {
                    try { action(); isCompleted_ = true; }
                    catch (...) { exception_ = std::current_exception(); isFaulted_ = true; isCompleted_ = true; }
                })
            );
        }

        [[nodiscard]] bool getIsCompletedProperty()   const { return isCompleted_; }
        [[nodiscard]] bool getIsCanceledProperty()    const { return isCanceled_; }
        [[nodiscard]] bool getIsFaultedProperty()     const { return isFaulted_; }
        [[nodiscard]] bool getIsCompletedSuccessfullyProperty() const { return isCompleted_ && !isFaulted_ && !isCanceled_; }

        void Wait() {
            if (future_ && future_->valid()) future_->get();
            if (isFaulted_ && exception_) std::rethrow_exception(exception_);
        }

        static Task Run(std::function<void()> action) { return Task(std::move(action)); }

        static Task CompletedTask() { return Task(); }

        static Task FromException(std::exception_ptr ex) {
            Task t;
            t.isFaulted_   = true;
            t.isCompleted_ = true;
            t.exception_   = ex;
            return t;
        }

        static Task FromCanceled(CancellationToken) {
            Task t;
            t.isCanceled_  = true;
            t.isCompleted_ = true;
            return t;
        }

        static Task Delay(int milliseconds) {
            return Task([milliseconds]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
            });
        }
    };

    template<typename TResult>
    class TaskT {
        std::shared_ptr<std::future<TResult>> future_;
        TResult result_{};
        bool isCompleted_ = false;
        bool isFaulted_   = false;
        std::exception_ptr exception_;

    public:
        explicit TaskT(std::function<TResult()> func) {
            future_ = std::make_shared<std::future<TResult>>(
                std::async(std::launch::async, [func, this]() -> TResult {
                    try {
                        TResult r = func();
                        result_ = r;
                        isCompleted_ = true;
                        return r;
                    } catch (...) {
                        exception_ = std::current_exception();
                        isFaulted_ = true;
                        isCompleted_ = true;
                        return TResult{};
                    }
                })
            );
        }

        [[nodiscard]] bool getIsCompletedProperty()   const { return isCompleted_; }
        [[nodiscard]] bool getIsFaultedProperty()     const { return isFaulted_; }

        TResult getResultProperty() {
            if (future_ && future_->valid()) result_ = future_->get();
            if (isFaulted_ && exception_) std::rethrow_exception(exception_);
            return result_;
        }

        TResult Wait() { return getResultProperty(); }

        static TaskT<TResult> FromResult(const TResult& value) {
            return TaskT<TResult>([value]() { return value; });
        }

        static TaskT<TResult> Run(std::function<TResult()> func) {
            return TaskT<TResult>(std::move(func));
        }
    };

} // namespace System::Threading::Tasks
