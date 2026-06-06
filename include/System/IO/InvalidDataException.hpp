// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/SystemException.hpp"
namespace System::IO {
    /** @brief The exception thrown when a data stream is in an invalid format. @note Status: Implemented */
    class InvalidDataException : public System::SystemException {
    public:
        InvalidDataException();
        explicit InvalidDataException(const char* message);
        explicit InvalidDataException(const std::string& message);
    };
} // namespace System::IO
