// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/OutOfMemoryException.hpp"
namespace System {
    OutOfMemoryException::OutOfMemoryException() : SystemException("Insufficient memory to continue the execution of the program.") {}
    OutOfMemoryException::OutOfMemoryException(const char* message) : SystemException(message) {}
    OutOfMemoryException::OutOfMemoryException(const std::string& message) : SystemException(message) {}
    OutOfMemoryException::OutOfMemoryException(const std::string& message, std::exception_ptr inner)
        : SystemException(message, std::move(inner)) {}
} // namespace System
