// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/SystemException.hpp"

namespace System {

    /// The exception thrown when an attempt is made to store an element of the wrong type in an array.
    class ArrayTypeMismatchException : public SystemException {
    public:
        /// Initializes a new instance with the default type mismatch message.
        ArrayTypeMismatchException() : SystemException("Attempted to store an element of the wrong type within an array.") {}
        /// Initializes a new instance with the specified error message.
        explicit ArrayTypeMismatchException(const std::string& message) : SystemException(message) {}
        /// Initializes a new instance with the specified message and inner exception.
        ArrayTypeMismatchException(const std::string& message, const std::exception& inner)
            : SystemException(message + " | inner: " + inner.what()) {}
    };

} // namespace System
