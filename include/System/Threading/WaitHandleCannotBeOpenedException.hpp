// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Exception.hpp"

namespace System::Threading {

    /// The exception thrown when an attempt is made to open a system mutex, semaphore, or event wait handle that does not exist.
    class WaitHandleCannotBeOpenedException : public System::Exception {
    public:
        /// Initializes a WaitHandleCannotBeOpenedException with a default message.
        WaitHandleCannotBeOpenedException() : Exception("No handle of the given name exists.") {}
        /// Initializes a WaitHandleCannotBeOpenedException with the specified message.
        explicit WaitHandleCannotBeOpenedException(const std::string& message) : Exception(message) {}
        /// Initializes a WaitHandleCannotBeOpenedException with a message and an inner exception.
        WaitHandleCannotBeOpenedException(const std::string& message, const std::exception& inner)
            : Exception(message + " | inner: " + inner.what()) {}
    };

} // namespace System::Threading
