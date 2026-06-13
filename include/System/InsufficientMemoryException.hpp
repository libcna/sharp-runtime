// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/OutOfMemoryException.hpp"

namespace System {

    /// The exception thrown when a check for sufficient available memory fails.
    class InsufficientMemoryException : public OutOfMemoryException {
    public:
        /// Initializes a new instance with the default insufficient-memory message.
        InsufficientMemoryException() : OutOfMemoryException("Insufficient memory to continue the execution of the program.") {}
        /// Initializes a new instance with the specified error message.
        explicit InsufficientMemoryException(const std::string& message) : OutOfMemoryException(message) {}
        /// Initializes a new instance with the specified message and inner exception.
        InsufficientMemoryException(const std::string& message, const std::exception& inner)
            : OutOfMemoryException(message + " | inner: " + inner.what()) {}
    };

} // namespace System
