// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/TypeLoadException.hpp"

namespace System {

    /// The exception thrown when an attempt to load a class fails because the entry point cannot be found.
    class EntryPointNotFoundException : public TypeLoadException {
    public:
        /// Initializes a new instance with the default entry-point-not-found message.
        EntryPointNotFoundException() : TypeLoadException("Entry point was not found.") {}
        /// Initializes a new instance with the specified error message.
        explicit EntryPointNotFoundException(const std::string& message) : TypeLoadException(message) {}
        /// Initializes a new instance with the specified message and inner exception.
        EntryPointNotFoundException(const std::string& message, const std::exception& inner)
            : TypeLoadException(message + " | inner: " + inner.what()) {}
    };

} // namespace System
