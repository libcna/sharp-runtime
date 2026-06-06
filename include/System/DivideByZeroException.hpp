// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <string>

#include "System/ArithmeticException.hpp"

namespace System {

    /**
     * @brief The exception that is thrown when there is an attempt to divide
     * an integral or decimal value by zero.
     *
     * @note Status: Implemented
     */
    class DivideByZeroException : public ArithmeticException {
    public:
        DivideByZeroException();
        explicit DivideByZeroException(const char* message);
        explicit DivideByZeroException(const std::string& message);
    };

} // namespace System
