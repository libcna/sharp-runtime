// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/ArgumentException.hpp"

namespace System {

    /// The exception thrown when a duplicate object exists in a wait array.
    class DuplicateWaitObjectException : public ArgumentException {
    public:
        /// Initializes a new instance with the default duplicate-object message.
        DuplicateWaitObjectException() : ArgumentException("Duplicate objects in argument.") {}
        /// Initializes a new instance with the name of the offending parameter.
        explicit DuplicateWaitObjectException(const std::string& parameterName)
            : ArgumentException("Duplicate objects in argument.", parameterName) {}
        /// Initializes a new instance with the parameter name and a custom message.
        DuplicateWaitObjectException(const std::string& parameterName, const std::string& message)
            : ArgumentException(message, parameterName) {}
    };

} // namespace System
