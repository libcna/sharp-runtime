// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/InvalidOperationException.hpp"

namespace System {

    static constexpr const char* kDefaultMsg =
        "Operation is not valid due to the current state of the object.";

    InvalidOperationException::InvalidOperationException()
        : SystemException(kDefaultMsg) {}

    InvalidOperationException::InvalidOperationException(const char* message)
        : SystemException(message) {}

    InvalidOperationException::InvalidOperationException(const std::string& message)
        : SystemException(message) {}

    InvalidOperationException::InvalidOperationException(
        const std::string& message, const std::exception& innerException)
        : SystemException(message + " | inner: " + innerException.what()) {}

} // namespace System