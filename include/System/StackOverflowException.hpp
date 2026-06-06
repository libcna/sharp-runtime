// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/SystemException.hpp"

namespace System {

    class StackOverflowException : public SystemException {
    public:
        StackOverflowException() : SystemException("The requested operation caused a stack overflow.") {}
        explicit StackOverflowException(const std::string& message) : SystemException(message) {}
    };

} // namespace System
