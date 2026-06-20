// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/TypeLoadException.hpp"

namespace System {

    /**
     * @brief The exception that is thrown when a method attempts to use a type
     * that it does not have access to.
     *
     * C++ counterpart of .NET System.TypeAccessException.
     * Inherits from TypeLoadException (matching .NET, where TypeAccessException
     * derives from TypeLoadException rather than MemberAccessException for
     * historical compatibility with pre-v4 runtimes).
     */
    class TypeAccessException : public TypeLoadException {
    public:
        /**
         * @brief Initializes a new instance of the TypeAccessException class with a
         * default error message.
         *
         * C++ counterpart of .NET TypeAccessException().
         */
        TypeAccessException()
            : TypeLoadException("Attempt by the method to access the type failed.") {}

        /**
         * @brief Initializes a new instance of the TypeAccessException class with a
         * specified error message.
         *
         * C++ counterpart of .NET TypeAccessException(string).
         * @param message The error message that explains the reason for the exception.
         */
        explicit TypeAccessException(const std::string& message)
            : TypeLoadException(message) {}

        /**
         * @brief Initializes a new instance of the TypeAccessException class with a
         * specified error message and a reference to the inner exception that is the
         * cause of this exception.
         *
         * C++ counterpart of .NET TypeAccessException(string, Exception).
         * @param message The error message that explains the reason for the exception.
         * @param inner   The exception that is the cause of the current exception.
         */
        TypeAccessException(const std::string& message, const std::exception& inner)
            : TypeLoadException(message + " | inner: " + inner.what()) {}
    };

} // namespace System
