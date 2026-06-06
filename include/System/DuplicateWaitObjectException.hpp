// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/ArgumentException.hpp"

namespace System {

    class DuplicateWaitObjectException : public ArgumentException {
    public:
        DuplicateWaitObjectException() : ArgumentException("Duplicate objects in argument.") {}
        explicit DuplicateWaitObjectException(const std::string& parameterName)
            : ArgumentException("Duplicate objects in argument.", parameterName) {}
        DuplicateWaitObjectException(const std::string& parameterName, const std::string& message)
            : ArgumentException(message, parameterName) {}
    };

} // namespace System
