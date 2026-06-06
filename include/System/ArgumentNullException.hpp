// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <string>

#include "System/ArgumentException.hpp"

namespace System {

    /**
     * @brief The exception that is thrown when a null reference is passed to a
     * method that does not accept it as a valid argument.
     *
     * @note Status: Implemented
     */
    class ArgumentNullException : public ArgumentException {
    public:
        ArgumentNullException();
        explicit ArgumentNullException(const char* paramName);
        ArgumentNullException(const char* paramName, const char* message);
        explicit ArgumentNullException(const std::string& paramName);
        ArgumentNullException(const std::string& paramName, const std::string& message);
    };

} // namespace System
