// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/MemberAccessException.hpp"

namespace System {

    class MethodAccessException : public MemberAccessException {
    public:
        MethodAccessException() : MemberAccessException("Attempted to access a method that is not accessible by the caller.") {}
        explicit MethodAccessException(const std::string& message) : MemberAccessException(message) {}
        MethodAccessException(const std::string& message, const std::exception& inner)
            : MemberAccessException(message + " | inner: " + inner.what()) {}
    };

} // namespace System
