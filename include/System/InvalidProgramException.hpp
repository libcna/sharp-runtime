// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/SystemException.hpp"

namespace System {

    /**
     * @brief The exception thrown when a program contains invalid IL code or metadata.
     *
     * C++ counterpart of .NET System.InvalidProgramException.
     */
    class InvalidProgramException : public SystemException {
    public:
        /** @brief Initializes a new instance with the default invalid-program message. */
        InvalidProgramException() : SystemException("Common Language Runtime detected an invalid program.") {}
        /** @brief Initializes a new instance with the specified error message. */
        explicit InvalidProgramException(const std::string& message) : SystemException(message) {}
    };

} // namespace System
