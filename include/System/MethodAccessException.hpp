// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <exception>
#include "System/MemberAccessException.hpp"

namespace System {

    /**
     * @brief The exception that is thrown when there is an invalid attempt to access a method,
     * such as accessing a private method from partially trusted code.
     *
     * C++ counterpart of .NET System.MethodAccessException.
     * Derives from MemberAccessException → SystemException → Exception.
     */
    class MethodAccessException : public MemberAccessException {
    public:
        /** @brief Initializes a new instance with a default message. */
        MethodAccessException()
            : MemberAccessException(
                  "Attempted to access a method that is not accessible by the caller.") {}

        /**
         * @brief Initializes a new instance with the specified error message.
         *
         * C++ counterpart of .NET MethodAccessException(string message).
         * @param message A description of the error.
         */
        explicit MethodAccessException(const std::string& message)
            : MemberAccessException(message) {}

        /**
         * @brief Initializes a new instance with the specified message and inner exception.
         *
         * C++ counterpart of .NET MethodAccessException(string message, Exception inner).
         * @param message A description of the error.
         * @param inner   The exception that caused this exception.
         */
        MethodAccessException(const std::string& message, std::exception_ptr inner)
            : MemberAccessException(message, inner) {}
    };

} // namespace System
