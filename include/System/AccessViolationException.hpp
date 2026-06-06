// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/SystemException.hpp"

namespace System {

    class AccessViolationException : public SystemException {
    public:
        AccessViolationException() : SystemException("Attempted to read or write protected memory. This is often an indication that other memory is corrupt.") {}
        explicit AccessViolationException(const std::string& message) : SystemException(message) {}
        AccessViolationException(const std::string& message, const std::exception& inner)
            : SystemException(message + " | inner: " + inner.what()) {}
    };

} // namespace System
