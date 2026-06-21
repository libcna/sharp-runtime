// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/SystemException.hpp"

namespace System {

    /**
     * @brief The exception that is thrown in a thread upon cancellation of an
     * operation that the thread was executing.
     *
     * C++ counterpart of .NET System.OperationCanceledException.
     */
    class OperationCanceledException : public SystemException {
    public:
        /** @brief Initializes a new instance with the default cancellation message. */
        OperationCanceledException();

        /** @brief Initializes a new instance with the specified C-string message. */
        explicit OperationCanceledException(const char* message);

        /** @brief Initializes a new instance with the specified message. */
        explicit OperationCanceledException(const std::string& message);

        /**
         * @brief Initializes a new instance with a message and an inner exception.
         *
         * C++ counterpart of .NET OperationCanceledException(string, Exception).
         */
        OperationCanceledException(const std::string& message, const std::exception& innerException);
    };

} // namespace System
