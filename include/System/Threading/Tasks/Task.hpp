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
        Task() : state_(std::make_shared<State>()) { state_->isCompleted = true; }

        explicit Task(std::function<void()> action) {
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
        }

        [[nodiscard]] bool getIsCompletedProperty()            const { return state_->isCompleted; }
        [[nodiscard]] bool getIsCanceledProperty()             const { return state_->isCanceled; }
        [[nodiscard]] bool getIsFaultedProperty()              const { return state_->isFaulted; }
        [[nodiscard]] bool getIsCompletedSuccessfullyProperty() const {
            return state_->isCompleted && !state_->isFaulted && !state_->isCanceled;
        }

        void Wait() {
            if (future_ && future_->valid()) future_->get();
            if (state_->isFaulted && state_->exception) std::rethrow_exception(state_->exception);
        }

        static Task Run(std::function<void()> action) { return Task(std::move(action)); }

        static Task CompletedTask() { return Task(); }

        static Task FromException(std::exception_ptr ex) {
            Task t;
            t.state_->isFaulted   = true;
            t.state_->isCompleted = true;
            t.state_->exception   = ex;
            return t;
        }

        static Task FromCanceled(CancellationToken) {
            Task t;
            t.state_->isCanceled  = true;
            t.state_->isCompleted = true;
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
        struct State {
            std::atomic<bool> isCompleted{false};
            std::atomic<bool> isFaulted{false};
            std::exception_ptr exception;
            TResult result{};
        };

        std::shared_ptr<std::future<TResult>> future_;
        std::shared_ptr<State>                state_;

    public:
        explicit TaskT(std::function<TResult()> func) {
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
        }

        [[nodiscard]] bool getIsCompletedProperty() const { return state_->isCompleted; }
        [[nodiscard]] bool getIsFaultedProperty()   const { return state_->isFaulted; }

        TResult getResultProperty() {
            if (future_ && future_->valid()) state_->result = future_->get();
            if (state_->isFaulted && state_->exception) std::rethrow_exception(state_->exception);
            return state_->result;
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
