// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/SystemException.hpp"

namespace System {

    class TypeLoadException : public SystemException {
    public:
        TypeLoadException() : SystemException("Failure has occurred while loading a type.") {}
        explicit TypeLoadException(const std::string& message) : SystemException(message) {}
        TypeLoadException(const std::string& message, const std::exception& inner)
            : SystemException(message + " | inner: " + inner.what()) {}
    };

} // namespace System
