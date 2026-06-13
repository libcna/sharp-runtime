// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/SystemException.hpp"

namespace System {

    /// The exception thrown when there is an internal error in the execution engine.
    class ExecutionEngineException : public SystemException {
    public:
        /// Initializes a new instance with the default engine-error message.
        ExecutionEngineException() : SystemException("Internal error in the runtime.") {}
        /// Initializes a new instance with the specified error message.
        explicit ExecutionEngineException(const std::string& message) : SystemException(message) {}
    };

} // namespace System
