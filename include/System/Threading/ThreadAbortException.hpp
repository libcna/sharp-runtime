// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/SystemException.hpp"

namespace System::Threading {

    /// The exception thrown when Thread.Abort() is invoked on a thread.
    class ThreadAbortException : public System::SystemException {
    public:
        /// Initializes a ThreadAbortException with a default message.
        ThreadAbortException() : SystemException("Thread was being aborted.") {}
        /// Initializes a ThreadAbortException with the specified message.
        explicit ThreadAbortException(const std::string& message) : SystemException(message) {}
    };

} // namespace System::Threading
