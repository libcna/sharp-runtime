// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/SystemException.hpp"

namespace System {

    class InsufficientExecutionStackException : public SystemException {
    public:
        InsufficientExecutionStackException() : SystemException("Insufficient stack to continue executing the program safely. This can happen from having too many functions on the call stack or function on the stack using too much stack space.") {}
        explicit InsufficientExecutionStackException(const std::string& message) : SystemException(message) {}
    };

} // namespace System
