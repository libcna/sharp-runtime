// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/SystemException.hpp"

namespace System::Security {

    class SecurityException : public System::SystemException {
    public:
        SecurityException() : System::SystemException("Security error.") {}
        explicit SecurityException(const std::string& message) : System::SystemException(message) {}
        SecurityException(const std::string& message, const std::string& permissionType)
            : System::SystemException(message + " (" + permissionType + ")") {}
    };

} // namespace System::Security
