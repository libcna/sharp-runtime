// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/SystemException.hpp"

namespace System::Security {

    class CryptographicException : public System::SystemException {
    public:
        CryptographicException() : System::SystemException("Error occurred during cryptographic operation.") {}
        explicit CryptographicException(const std::string& message) : System::SystemException(message) {}
        CryptographicException(const std::string& message, const std::string& inner)
            : System::SystemException(message + ": " + inner) {}
    };

} // namespace System::Security
