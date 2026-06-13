// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/SystemException.hpp"

namespace System::Threading {

    /// The exception thrown when one thread acquires a Mutex object that another thread has abandoned by exiting.
    class AbandonedMutexException : public System::SystemException {
    public:
        /// Initializes an AbandonedMutexException with a default message.
        AbandonedMutexException() : SystemException("The wait completed due to an abandoned mutex.") {}
        /// Initializes an AbandonedMutexException with the specified message.
        explicit AbandonedMutexException(const std::string& message) : SystemException(message) {}
        /// Initializes an AbandonedMutexException with a message and an inner exception.
        AbandonedMutexException(const std::string& message, const std::exception& inner)
            : SystemException(message + " | inner: " + inner.what()) {}
    };

} // namespace System::Threading
