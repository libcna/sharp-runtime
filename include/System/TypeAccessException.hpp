// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/TypeLoadException.hpp"

namespace System {

    class TypeAccessException : public TypeLoadException {
    public:
        TypeAccessException() : TypeLoadException("Attempt by the method to access the type failed.") {}
        explicit TypeAccessException(const std::string& message) : TypeLoadException(message) {}
        TypeAccessException(const std::string& message, const std::exception& inner)
            : TypeLoadException(message + " | inner: " + inner.what()) {}
    };

} // namespace System
