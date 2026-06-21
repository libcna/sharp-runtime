// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/MemberAccessException.hpp"

namespace System {

    /**
     * @brief The exception that is thrown when there is an invalid attempt to
     * access a private or protected field inside a class.
     *
     * C++ counterpart of .NET System.FieldAccessException.
     * Derives from MemberAccessException → SystemException → Exception.
     */
    class FieldAccessException : public MemberAccessException {
    public:
        /**
         * @brief Initializes a new instance with the default message.
         *
         * C++ counterpart of .NET FieldAccessException().
         * The default message is "Attempted to access a field that is not
         * accessible by the caller."
         */
        FieldAccessException()
            : MemberAccessException("Attempted to access a field that is not accessible by the caller.") {}

        /**
         * @brief Initializes a new instance with the specified error message.
         *
         * C++ counterpart of .NET FieldAccessException(string).
         * @param message A string that describes the error.
         */
        explicit FieldAccessException(const char* message)
            : MemberAccessException(message) {}

        /**
         * @brief Initializes a new instance with the specified error message.
         *
         * C++ counterpart of .NET FieldAccessException(string).
         * @param message A string that describes the error.
         */
        explicit FieldAccessException(const std::string& message)
            : MemberAccessException(message) {}

        /**
         * @brief Initializes a new instance with a specified error message and
         * a reference to the inner exception that caused this exception.
         *
         * C++ counterpart of .NET FieldAccessException(string, Exception).
         * @param message A string that describes the error.
         * @param inner   The exception that is the cause of this exception.
         */
        FieldAccessException(const std::string& message, const std::exception& inner)
            : MemberAccessException(message + " | inner: " + inner.what()) {}
    };

} // namespace System
