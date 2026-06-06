// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <exception>
#include <stdexcept>
#include <variant>
#include "System/Threading/Tasks/Task.hpp"

namespace System::Threading::Tasks {

    // Simplified ValueTask backed by either a completed value or a Task.
    class ValueTask {
        bool completed_;
        std::exception_ptr exception_;

    public:
        ValueTask() : completed_(true) {}
        explicit ValueTask(Task t) : completed_(t.getIsCompletedProperty()) {}
        explicit ValueTask(std::exception_ptr ex) : completed_(true), exception_(ex) {}

        [[nodiscard]] bool getIsCompletedProperty()            const { return completed_; }
        [[nodiscard]] bool getIsCompletedSuccessfullyProperty() const { return completed_ && !exception_; }
        [[nodiscard]] bool getIsFaultedProperty()             const { return exception_ != nullptr; }

        void GetAwaiter() const {
            if (exception_) std::rethrow_exception(exception_);
        }

        static ValueTask CompletedTask() { return ValueTask(); }

        static ValueTask FromException(std::exception_ptr ex) { return ValueTask(ex); }
    };

    template<typename TResult>
    class ValueTaskT {
        bool completed_;
        TResult result_{};
        std::exception_ptr exception_;

    public:
        ValueTaskT() : completed_(false) {}
        explicit ValueTaskT(const TResult& result) : completed_(true), result_(result) {}
        explicit ValueTaskT(std::exception_ptr ex) : completed_(true), exception_(ex) {}

        [[nodiscard]] bool getIsCompletedProperty()            const { return completed_; }
        [[nodiscard]] bool getIsCompletedSuccessfullyProperty() const { return completed_ && !exception_; }
        [[nodiscard]] bool getIsFaultedProperty()             const { return exception_ != nullptr; }

        TResult getResultProperty() const {
            if (exception_) std::rethrow_exception(exception_);
            return result_;
        }

        static ValueTaskT<TResult> FromResult(const TResult& v) { return ValueTaskT<TResult>(v); }
        static ValueTaskT<TResult> FromException(std::exception_ptr ex) { return ValueTaskT<TResult>(ex); }
    };

} // namespace System::Threading::Tasks
