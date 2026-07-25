// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <any>
#include <exception>
#include <functional>
#include <utility>

#include "System/EventArgs.hpp"
#include "System/InvalidOperationException.hpp"

namespace System::ComponentModel
{
    /**
     * @brief Provides data for the completion of an asynchronous operation.
     *
     * The error is represented by std::exception_ptr, the C++ equivalent of a
     * polymorphic Exception reference.  The .NET TargetInvocationException
     * wrapper is unavailable because the reflection subsystem is intentionally
     * not ported; an InvalidOperationException with the stored error as its
     * inner exception is thrown instead.
     */
    class AsyncCompletedEventArgs : public System::EventArgs
    {
    private:
        std::exception_ptr error_;
        bool cancelled_;
        std::any userState_;

    protected:
        /**
         * @brief Throws when the operation failed or was cancelled.
         *
         * C++ counterpart of .NET AsyncCompletedEventArgs.RaiseExceptionIfNecessary().
         */
        void RaiseExceptionIfNecessary() const
        {
            if (error_)
            {
                throw System::InvalidOperationException(
                    "An exception occurred during the operation, making the result invalid.  "
                    "Check InnerException for exception details.",
                    error_);
            }

            if (cancelled_)
            {
                throw System::InvalidOperationException("Operation has been cancelled.");
            }
        }

    public:
        /**
         * @brief Initializes the completion event data.
         * @param error Error that prevented completion, or an empty pointer when none occurred.
         * @param cancelled Whether the operation was cancelled.
         * @param userState Caller-defined state, or an empty std::any for null.
         */
        AsyncCompletedEventArgs(std::exception_ptr error, bool cancelled, std::any userState)
            : error_(std::move(error)), cancelled_(cancelled), userState_(std::move(userState)) {}

        /** Gets whether the asynchronous operation was cancelled. */
        [[nodiscard]] bool getCancelledProperty() const noexcept { return cancelled_; }

        /** Gets the error that occurred during the asynchronous operation. */
        [[nodiscard]] const std::exception_ptr& getErrorProperty() const noexcept { return error_; }

        /** Gets the user-defined state passed to the asynchronous operation. */
        [[nodiscard]] const std::any& getUserStateProperty() const noexcept { return userState_; }
    };

    /** Delegate invoked when an asynchronous operation completes. */
    using AsyncCompletedEventHandler = std::function<void(void*, const AsyncCompletedEventArgs&)>;
}
