// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/MemberAccessException.hpp"

namespace System {

    class MissingMemberException : public MemberAccessException {
    public:
        MissingMemberException() : MemberAccessException("Attempted to access a missing member.") {}
        explicit MissingMemberException(const std::string& message) : MemberAccessException(message) {}
        MissingMemberException(const std::string& className, const std::string& memberName)
            : MemberAccessException("Member not found: " + className + "." + memberName) {}
        MissingMemberException(const std::string& message, const std::exception& inner)
            : MemberAccessException(message + " | inner: " + inner.what()) {}
    };

} // namespace System
