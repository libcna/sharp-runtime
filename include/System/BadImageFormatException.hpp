// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/SystemException.hpp"

namespace System {

    class BadImageFormatException : public SystemException {
    public:
        BadImageFormatException() : SystemException("Format of the executable (.exe) or library (.dll) is invalid.") {}
        explicit BadImageFormatException(const std::string& message) : SystemException(message) {}
        BadImageFormatException(const std::string& message, const std::exception& inner)
            : SystemException(message + " | inner: " + inner.what()) {}
    };

} // namespace System
