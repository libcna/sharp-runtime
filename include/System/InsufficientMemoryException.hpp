// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/OutOfMemoryException.hpp"

namespace System {

    /**
     * @brief The exception thrown when a check for sufficient available memory
     * fails. This class cannot be inherited.
     *
     * C++ counterpart of .NET System.InsufficientMemoryException (sealed).
     * Use this exception for recoverable out-of-memory conditions — cases like
     * MemoryFailPoint or a TryAllocate pattern — where shared state is not
     * corrupted and the caller can reasonably catch and recover.
     *
     * Unlike OutOfMemoryException (which signals a fatal allocation failure),
     * InsufficientMemoryException is intended to be caught by the caller.
     */
    class InsufficientMemoryException final : public OutOfMemoryException {
    public:
        /**
         * @brief Initializes a new instance with the default message.
         *
         * C++ counterpart of .NET InsufficientMemoryException().
         */
        InsufficientMemoryException()
            : OutOfMemoryException("Insufficient memory to continue the execution of the program.") {}

        /**
         * @brief Initializes a new instance with the specified error message.
         *
         * C++ counterpart of .NET InsufficientMemoryException(string).
         * @param message A string that describes the error.
         */
        explicit InsufficientMemoryException(const std::string& message)
            : OutOfMemoryException(message) {}

        /**
         * @brief Initializes a new instance with the specified error message.
         *
         * @param message A string that describes the error.
         */
        explicit InsufficientMemoryException(const char* message)
            : OutOfMemoryException(message) {}

        /**
         * @brief Initializes a new instance with the specified message and
         * inner exception.
         *
         * C++ counterpart of .NET InsufficientMemoryException(string, Exception).
         * @param message A string that describes the error.
         * @param inner   The exception that is the cause of this exception.
         */
        InsufficientMemoryException(const std::string& message, const std::exception& inner)
            : OutOfMemoryException(message + " | inner: " + inner.what()) {}
    };

} // namespace System
