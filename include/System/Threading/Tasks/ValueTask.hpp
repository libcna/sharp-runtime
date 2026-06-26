// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <exception>
#include <stdexcept>
#include <variant>
#include "System/Threading/Tasks/Task.hpp"

namespace System::Threading::Tasks {

    /** Represents an asynchronous operation that either completes synchronously or wraps a Task. */
    class ValueTask {
        bool completed_;
        std::exception_ptr exception_;

    public:
        /** Constructs a successfully completed ValueTask. */
        ValueTask() : completed_(true) {}
        /** Constructs a ValueTask from a Task, inheriting its completion state. */
        explicit ValueTask(Task t) : completed_(t.getIsCompletedProperty()) {}
        /** Constructs a faulted ValueTask from the given exception pointer. */
        explicit ValueTask(std::exception_ptr ex) : completed_(true), exception_(ex) {}

        /** Returns true if the operation has completed. */
        [[nodiscard]] bool getIsCompletedProperty()            const { return completed_; }
        /** Returns true if the operation completed successfully. */
        [[nodiscard]] bool getIsCompletedSuccessfullyProperty() const { return completed_ && !exception_; }
        /** Returns true if the operation faulted. */
        [[nodiscard]] bool getIsFaultedProperty()             const { return exception_ != nullptr; }

        /** Rethrows the stored exception, if any. */
        void GetAwaiter() const {
            if (exception_) std::rethrow_exception(exception_);
        }

        /** Returns a completed ValueTask. */
        static ValueTask CompletedTask() { return ValueTask(); }

        /** Returns a ValueTask that faulted with the given exception. */
        static ValueTask FromException(std::exception_ptr ex) { return ValueTask(ex); }
    };

    /** Represents an asynchronous operation that produces a result of type TResult. */
    template<typename TResult>
    class ValueTaskT {
        bool completed_;
        TResult result_{};
        std::exception_ptr exception_;

    public:
        /** Constructs an incomplete ValueTaskT. */
        ValueTaskT() : completed_(false) {}
        /** Constructs a successfully completed ValueTaskT with the given result. */
        explicit ValueTaskT(const TResult& result) : completed_(true), result_(result) {}
        /** Constructs a faulted ValueTaskT from the given exception pointer. */
        explicit ValueTaskT(std::exception_ptr ex) : completed_(true), exception_(ex) {}

        /** Returns true if the operation has completed. */
        [[nodiscard]] bool getIsCompletedProperty()            const { return completed_; }
        /** Returns true if the operation completed successfully. */
        [[nodiscard]] bool getIsCompletedSuccessfullyProperty() const { return completed_ && !exception_; }
        /** Returns true if the operation faulted. */
        [[nodiscard]] bool getIsFaultedProperty()             const { return exception_ != nullptr; }

        /** Returns the result, or rethrows the stored exception if faulted. */
        TResult getResultProperty() const {
            if (exception_) std::rethrow_exception(exception_);
            return result_;
        }

        /** Returns a successfully completed ValueTaskT holding v. */
        static ValueTaskT<TResult> FromResult(const TResult& v) { return ValueTaskT<TResult>(v); }
        /** Returns a faulted ValueTaskT wrapping the given exception. */
        static ValueTaskT<TResult> FromException(std::exception_ptr ex) { return ValueTaskT<TResult>(ex); }
    };

} // namespace System::Threading::Tasks
