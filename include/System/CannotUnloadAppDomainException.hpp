// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/SystemException.hpp"

namespace System {

    /// The exception thrown when an attempt to unload an application domain fails.
    class CannotUnloadAppDomainException : public SystemException {
    public:
        /// Initializes a new instance with the default unload failure message.
        CannotUnloadAppDomainException() : SystemException("Attempt to unload the AppDomain failed.") {}
        /// Initializes a new instance with the specified error message.
        explicit CannotUnloadAppDomainException(const std::string& message) : SystemException(message) {}
        /// Initializes a new instance with the specified message and inner exception.
        CannotUnloadAppDomainException(const std::string& message, const std::exception& inner)
            : SystemException(message + " | inner: " + inner.what()) {}
    };

} // namespace System
