// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Exception.hpp"

namespace System {

    /// The exception that represents a non-fatal application error.
    class ApplicationException : public Exception {
    public:
        /// Initializes a new instance with the default application error message.
        ApplicationException() : Exception("Error in the application.") {}
        /// Initializes a new instance with the specified error message.
        explicit ApplicationException(const std::string& message) : Exception(message) {}
        /// Initializes a new instance with the specified message and inner exception.
        ApplicationException(const std::string& message, const std::exception& inner)
            : Exception(message + " | inner: " + inner.what()) {}
    };

} // namespace System
