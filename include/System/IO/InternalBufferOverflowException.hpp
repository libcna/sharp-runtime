// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include <utility>
#include "System/SystemException.hpp"

namespace System::IO {

    /** @brief The exception that is thrown when the internal buffer overflows. @note Status: Implemented */
    class InternalBufferOverflowException : public System::SystemException {
    public:
        InternalBufferOverflowException() : System::SystemException("The internal buffer overflows.") {}
        explicit InternalBufferOverflowException(const std::string& message) : System::SystemException(message) {}
        InternalBufferOverflowException(const std::string& message, std::exception_ptr inner)
            : System::SystemException(message, std::move(inner)) {}
    };

} // namespace System::IO
