// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/SystemException.hpp"
namespace System {
    /** @brief The exception thrown when there is not enough memory to continue execution. @note Status: Implemented */
    class OutOfMemoryException : public SystemException {
    public:
        OutOfMemoryException();
        explicit OutOfMemoryException(const char* message);
        explicit OutOfMemoryException(const std::string& message);
    };
} // namespace System
