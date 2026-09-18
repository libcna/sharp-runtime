// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <exception>
#include <string>
#include "System/ApplicationException.hpp"

namespace System::Reflection {

/**
 * @brief The exception that is thrown when the number of parameters for an invocation does not
 *        match the number expected.
 *
 * C++ counterpart of .NET System.Reflection.TargetParameterCountException. Thrown by
 * `MethodBase::Invoke` when the argument list is the wrong length.
 */
class TargetParameterCountException : public System::ApplicationException {
public:
    /** @brief Initializes the exception with .NET's default message. */
    TargetParameterCountException() : ApplicationException("Parameter count mismatch.") {}

    /**
     * @brief Initializes the exception with a message.
     * @param message The error message.
     */
    explicit TargetParameterCountException(const std::string& message) : ApplicationException(message) {}

    /**
     * @brief Initializes the exception with a message and an inner exception.
     * @param message The error message.
     * @param inner The exception that caused this one.
     */
    TargetParameterCountException(const std::string& message, std::exception_ptr inner)
        : ApplicationException(message, inner) {}
};

} // namespace System::Reflection
