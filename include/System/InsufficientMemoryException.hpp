// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/OutOfMemoryException.hpp"

namespace System {

    class InsufficientMemoryException : public OutOfMemoryException {
    public:
        InsufficientMemoryException() : OutOfMemoryException("Insufficient memory to continue the execution of the program.") {}
        explicit InsufficientMemoryException(const std::string& message) : OutOfMemoryException(message) {}
        InsufficientMemoryException(const std::string& message, const std::exception& inner)
            : OutOfMemoryException(message + " | inner: " + inner.what()) {}
    };

} // namespace System
