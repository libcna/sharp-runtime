// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/SystemException.hpp"

namespace System::Threading {

    /// The exception thrown when a thread is interrupted while it is in a waiting state.
    class ThreadInterruptedException : public System::SystemException {
    public:
        /// Initializes a ThreadInterruptedException with a default message.
        ThreadInterruptedException() : SystemException("Thread was interrupted from a waiting state.") {}
        /// Initializes a ThreadInterruptedException with the specified message.
        explicit ThreadInterruptedException(const std::string& message) : SystemException(message) {}
        /// Initializes a ThreadInterruptedException with a message and an inner exception.
        ThreadInterruptedException(const std::string& message, const std::exception& inner)
            : SystemException(message + " | inner: " + inner.what()) {}
    };

} // namespace System::Threading
