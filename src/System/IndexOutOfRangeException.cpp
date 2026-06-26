// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/IndexOutOfRangeException.hpp"

namespace System {

    namespace {
        constexpr const char* DefaultMsg = "Index was outside the bounds of the array.";
    }

    IndexOutOfRangeException::IndexOutOfRangeException()
        : SystemException(DefaultMsg) {}

    IndexOutOfRangeException::IndexOutOfRangeException(const char* message)
        : SystemException(message) {}

    IndexOutOfRangeException::IndexOutOfRangeException(const std::string& message)
        : SystemException(message) {}

    IndexOutOfRangeException::IndexOutOfRangeException(const std::string& message,
                                                       std::exception_ptr innerException)
        : SystemException(message, std::move(innerException)) {}

} // namespace System
