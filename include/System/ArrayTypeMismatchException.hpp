// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/SystemException.hpp"

namespace System {

    class ArrayTypeMismatchException : public SystemException {
    public:
        ArrayTypeMismatchException() : SystemException("Attempted to store an element of the wrong type within an array.") {}
        explicit ArrayTypeMismatchException(const std::string& message) : SystemException(message) {}
        ArrayTypeMismatchException(const std::string& message, const std::exception& inner)
            : SystemException(message + " | inner: " + inner.what()) {}
    };

} // namespace System
