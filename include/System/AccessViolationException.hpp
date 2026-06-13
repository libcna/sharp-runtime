// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/SystemException.hpp"

namespace System {

    /// The exception thrown when there is an attempt to read or write protected memory.
    class AccessViolationException : public SystemException {
    public:
        /// Initializes a new instance with the default access violation message.
        AccessViolationException() : SystemException("Attempted to read or write protected memory. This is often an indication that other memory is corrupt.") {}
        /// Initializes a new instance with the specified error message.
        explicit AccessViolationException(const std::string& message) : SystemException(message) {}
        /// Initializes a new instance with the specified message and inner exception.
        AccessViolationException(const std::string& message, const std::exception& inner)
            : SystemException(message + " | inner: " + inner.what()) {}
    };

} // namespace System
