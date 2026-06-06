// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/MissingMemberException.hpp"

namespace System {

    class MissingFieldException : public MissingMemberException {
    public:
        MissingFieldException() : MissingMemberException("Attempted to access a field that does not exist.") {}
        explicit MissingFieldException(const std::string& message) : MissingMemberException(message) {}
        MissingFieldException(const std::string& className, const std::string& fieldName)
            : MissingMemberException("Field not found: " + className + "." + fieldName) {}
    };

} // namespace System
