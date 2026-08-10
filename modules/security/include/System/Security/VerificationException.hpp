// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/SystemException.hpp"

namespace System::Security {

    class VerificationException : public System::SystemException {
    public:
        VerificationException() : System::SystemException("Verification failed.") {
            setHResultProperty(static_cast<SharpRuntime::intcs>(0x8013150Du)); // COR_E_VERIFICATION
        }
        explicit VerificationException(const std::string& message) : System::SystemException(message) {
            setHResultProperty(static_cast<SharpRuntime::intcs>(0x8013150Du)); // COR_E_VERIFICATION
        }
    };

} // namespace System::Security
