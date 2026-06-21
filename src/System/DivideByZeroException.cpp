// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/DivideByZeroException.hpp"

namespace System {

    namespace {
        constexpr const char* DefaultMsg = "Attempted to divide by zero.";
    }

    DivideByZeroException::DivideByZeroException()
        : ArithmeticException(DefaultMsg) {}

    DivideByZeroException::DivideByZeroException(const char* message)
        : ArithmeticException(message) {}

    DivideByZeroException::DivideByZeroException(const std::string& message)
        : ArithmeticException(message) {}

    DivideByZeroException::DivideByZeroException(
        const std::string& message, const std::exception& innerException)
        : ArithmeticException(message + " | inner: " + innerException.what()) {}

} // namespace System
