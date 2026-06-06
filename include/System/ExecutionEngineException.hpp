// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/SystemException.hpp"

namespace System {

    class ExecutionEngineException : public SystemException {
    public:
        ExecutionEngineException() : SystemException("Internal error in the runtime.") {}
        explicit ExecutionEngineException(const std::string& message) : SystemException(message) {}
    };

} // namespace System
