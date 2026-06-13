// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/MemberAccessException.hpp"

namespace System {

    /// The exception thrown when there is an invalid attempt to access a field.
    class FieldAccessException : public MemberAccessException {
    public:
        /// Initializes a new instance with the default field-access message.
        FieldAccessException() : MemberAccessException("Attempted to access a field that is not accessible by the caller.") {}
        /// Initializes a new instance with the specified error message.
        explicit FieldAccessException(const std::string& message) : MemberAccessException(message) {}
        /// Initializes a new instance with the specified message and inner exception.
        FieldAccessException(const std::string& message, const std::exception& inner)
            : MemberAccessException(message + " | inner: " + inner.what()) {}
    };

} // namespace System
