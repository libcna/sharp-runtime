// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/NullReferenceException.hpp"

namespace System {

    NullReferenceException::NullReferenceException()
        : SystemException("Object reference not set to an instance of an object.") {}

    NullReferenceException::NullReferenceException(const char* message)
        : SystemException(message) {}

    NullReferenceException::NullReferenceException(const std::string& message)
        : SystemException(message) {}

    NullReferenceException::NullReferenceException(
        const std::string& message, const std::exception& innerException)
        : SystemException(message + " | inner: " + innerException.what()) {}

} // namespace System
