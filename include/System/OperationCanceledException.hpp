// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/SystemException.hpp"
namespace System {
    /** @brief The exception thrown in a thread upon cancellation of an operation. @note Status: Implemented */
    class OperationCanceledException : public SystemException {
    public:
        OperationCanceledException();
        explicit OperationCanceledException(const char* message);
        explicit OperationCanceledException(const std::string& message);
    };
} // namespace System
