// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/TimeoutException.hpp"
namespace System {
    TimeoutException::TimeoutException() : SystemException("The operation has timed out.") {}
    TimeoutException::TimeoutException(const char* message) : SystemException(message) {}
    TimeoutException::TimeoutException(const std::string& message) : SystemException(message) {}
} // namespace System
