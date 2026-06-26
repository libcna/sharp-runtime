// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/OverflowException.hpp"

namespace System {

    OverflowException::OverflowException()
        : ArithmeticException("Arithmetic operation resulted in an overflow.") {}

    OverflowException::OverflowException(const char* str)
        : ArithmeticException(str) {}

    OverflowException::OverflowException(const std::string& message)
        : ArithmeticException(message) {}

    OverflowException::OverflowException(const std::string& message, std::exception_ptr innerException)
        : ArithmeticException(message, std::move(innerException)) {}

} // namespace System
