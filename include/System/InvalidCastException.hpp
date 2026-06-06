// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/SystemException.hpp"
namespace System {
    /** @brief The exception thrown for invalid casting or explicit conversion. @note Status: Implemented */
    class InvalidCastException : public SystemException {
    public:
        InvalidCastException();
        explicit InvalidCastException(const char* message);
        explicit InvalidCastException(const std::string& message);
    };
} // namespace System
