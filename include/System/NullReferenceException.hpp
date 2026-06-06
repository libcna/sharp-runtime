// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/SystemException.hpp"
namespace System {
    /** @brief The exception thrown when there is an attempt to dereference a null object reference. @note Status: Implemented */
    class NullReferenceException : public SystemException {
    public:
        NullReferenceException();
        explicit NullReferenceException(const char* message);
        explicit NullReferenceException(const std::string& message);
    };
} // namespace System
