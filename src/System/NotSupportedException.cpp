// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/NotSupportedException.hpp"

namespace System {

    namespace {
        constexpr const char* DefaultMsg = "Specified method is not supported.";
    }

    NotSupportedException::NotSupportedException()
        : SystemException(DefaultMsg) {}

    NotSupportedException::NotSupportedException(const char* message)
        : SystemException(message) {}

    NotSupportedException::NotSupportedException(const std::string& message)
        : SystemException(message) {}

    NotSupportedException::NotSupportedException(const std::string& message,
                                                 std::exception_ptr innerException)
        : SystemException(message, std::move(innerException)) {}

} // namespace System
