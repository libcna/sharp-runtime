// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/SystemException.hpp"

namespace System {

    class InvalidProgramException : public SystemException {
    public:
        InvalidProgramException() : SystemException("Common Language Runtime detected an invalid program.") {}
        explicit InvalidProgramException(const std::string& message) : SystemException(message) {}
    };

} // namespace System
