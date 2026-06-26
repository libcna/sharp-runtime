// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
#pragma once

#include "System/SystemException.hpp"
#include <string>

namespace System::Runtime::InteropServices
{
    /** Base class for exceptions thrown by, or related to, external (unmanaged) code. */
    class ExternalException : public System::SystemException
    {
    public:
        ExternalException()
            : System::SystemException("External component has thrown an exception.") {}

        explicit ExternalException(const std::string& message)
            : System::SystemException(message) {}

        ExternalException(const std::string& message, std::exception_ptr inner)
            : System::SystemException(message, std::move(inner)) {}

        ~ExternalException() override = default;
    };

} // namespace System::Runtime::InteropServices
