// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/SystemException.hpp"

namespace System {

    /**
     * @brief The exception thrown when there is insufficient execution stack available
     * to allow most methods to execute.
     *
     * C++ counterpart of .NET System.InsufficientExecutionStackException.
     */
    class InsufficientExecutionStackException : public SystemException {
    public:
        /** @brief Initializes a new instance with the default insufficient-stack message. */
        InsufficientExecutionStackException() : SystemException("Insufficient stack to continue executing the program safely. This can happen from having too many functions on the call stack or function on the stack using too much stack space.") {}
        /** @brief Initializes a new instance with the specified error message. */
        explicit InsufficientExecutionStackException(const std::string& message) : SystemException(message) {}
    };

} // namespace System
