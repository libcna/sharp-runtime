// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/SystemException.hpp"
namespace System {
    /** @brief The exception thrown when the time allotted for a process or operation has expired. @note Status: Implemented */
    class TimeoutException : public SystemException {
    public:
        TimeoutException();
        explicit TimeoutException(const char* message);
        explicit TimeoutException(const std::string& message);
    };
} // namespace System
