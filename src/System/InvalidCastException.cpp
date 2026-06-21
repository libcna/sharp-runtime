// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/InvalidCastException.hpp"
namespace System {
    InvalidCastException::InvalidCastException() : SystemException("Specified cast is not valid.") {}
    InvalidCastException::InvalidCastException(const char* message) : SystemException(message) {}
    InvalidCastException::InvalidCastException(const std::string& message) : SystemException(message) {}
    InvalidCastException::InvalidCastException(const std::string& message, const std::exception& innerException)
        : SystemException(message + " | inner: " + innerException.what()) {}
} // namespace System
