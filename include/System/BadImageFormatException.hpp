// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/SystemException.hpp"

namespace System {

    /// The exception thrown when the file image of a dynamic-link library or an executable program is invalid.
    class BadImageFormatException : public SystemException {
    public:
        /// Initializes a new instance with the default invalid-format message.
        BadImageFormatException() : SystemException("Format of the executable (.exe) or library (.dll) is invalid.") {}
        /// Initializes a new instance with the specified error message.
        explicit BadImageFormatException(const std::string& message) : SystemException(message) {}
        /// Initializes a new instance with the specified message and inner exception.
        BadImageFormatException(const std::string& message, const std::exception& inner)
            : SystemException(message + " | inner: " + inner.what()) {}
    };

} // namespace System
