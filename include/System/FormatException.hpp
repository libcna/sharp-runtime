// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <string>

#include "System/SystemException.hpp"

namespace System {

    /**
     * @brief The exception that is thrown when the format of an argument is
     * invalid, or when a composite format string is not well formed.
     *
     * @note Status: Implemented
     */
    class FormatException : public SystemException {
    public:
        /// Initializes a new instance with the default format-error message.
        FormatException();
        /// Initializes a new instance with the specified error message.
        explicit FormatException(const char* message);
        /// Initializes a new instance with the specified error message.
        explicit FormatException(const std::string& message);
    };

} // namespace System
