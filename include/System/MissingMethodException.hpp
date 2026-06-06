// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/MissingMemberException.hpp"

namespace System {

    class MissingMethodException : public MissingMemberException {
    public:
        MissingMethodException() : MissingMemberException("Attempted to access a method that does not exist.") {}
        explicit MissingMethodException(const std::string& message) : MissingMemberException(message) {}
        MissingMethodException(const std::string& className, const std::string& methodName)
            : MissingMemberException("Method not found: " + className + "." + methodName) {}
    };

} // namespace System
