// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/MissingMemberException.hpp"

namespace System {

    /**
     * @brief The exception thrown when there is an attempt to dynamically access a field
     * that does not exist.
     *
     * C++ counterpart of .NET System.MissingFieldException.
     */
    class MissingFieldException : public MissingMemberException {
    public:
        /** @brief Initializes a new instance with the default missing-field message. */
        MissingFieldException() : MissingMemberException("Attempted to access a field that does not exist.") {}
        /** @brief Initializes a new instance with the specified error message. */
        explicit MissingFieldException(const std::string& message) : MissingMemberException(message) {}
        /** @brief Initializes a new instance with the class and field name. */
        MissingFieldException(const std::string& className, const std::string& fieldName)
            : MissingMemberException("Field not found: " + className + "." + fieldName) {}
        /** @brief Initializes a new instance with the specified message and inner exception. */
        MissingFieldException(const std::string& message, const std::exception& innerException)
            : MissingMemberException(message + " | inner: " + innerException.what()) {}
    };

} // namespace System
