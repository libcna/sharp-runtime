// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/MemberAccessException.hpp"

namespace System {

    class FieldAccessException : public MemberAccessException {
    public:
        FieldAccessException() : MemberAccessException("Attempted to access a field that is not accessible by the caller.") {}
        explicit FieldAccessException(const std::string& message) : MemberAccessException(message) {}
        FieldAccessException(const std::string& message, const std::exception& inner)
            : MemberAccessException(message + " | inner: " + inner.what()) {}
    };

} // namespace System
