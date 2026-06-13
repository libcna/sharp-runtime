// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/IO/IOException.hpp"

namespace System::IO {

    /// The exception thrown when a path or file name is too long.
    class PathTooLongException : public IOException {
    public:
        /// Initializes a PathTooLongException with a default message.
        PathTooLongException() : IOException("The specified path, file name, or both are too long. The fully qualified file name must be less than 260 characters, and the directory name must be less than 248 characters.") {}
        /// Initializes a PathTooLongException with the specified message.
        explicit PathTooLongException(const std::string& message) : IOException(message) {}
        /// Initializes a PathTooLongException with a message and an inner exception.
        PathTooLongException(const std::string& message, const std::exception& inner)
            : IOException(message + " | inner: " + inner.what()) {}
    };

} // namespace System::IO
