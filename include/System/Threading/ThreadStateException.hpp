// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/SystemException.hpp"

namespace System::Threading {

    /// The exception thrown when a method is invoked on a Thread that is in an invalid ThreadState.
    class ThreadStateException : public System::SystemException {
    public:
        /// Initializes a ThreadStateException with a default message.
        ThreadStateException() : SystemException("Thread is in an invalid state for the operation being executed.") {}
        /// Initializes a ThreadStateException with the specified message.
        explicit ThreadStateException(const std::string& message) : SystemException(message) {}
        /// Initializes a ThreadStateException with a message and an inner exception.
        ThreadStateException(const std::string& message, const std::exception& inner)
            : SystemException(message + " | inner: " + inner.what()) {}
    };

} // namespace System::Threading
