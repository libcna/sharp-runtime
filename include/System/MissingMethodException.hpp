// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/MissingMemberException.hpp"

namespace System {

    /**
     * @brief The exception that is thrown when there is an attempt to dynamically
     * access a method that does not exist.
     *
     * C++ counterpart of .NET System.MissingMethodException.
     */
    class MissingMethodException : public MissingMemberException {
    public:
        /** @brief Initializes a new instance with the default message. */
        MissingMethodException()
            : MissingMemberException("Attempted to access a method that does not exist.") {}

        /** @brief Initializes a new instance with the specified message. */
        explicit MissingMethodException(const std::string& message)
            : MissingMemberException(message) {}

        /** @brief Initializes a new instance with the class name, method name, and an inner exception. */
        MissingMethodException(const std::string& message, std::exception_ptr innerException)
            : MissingMemberException(message, std::move(innerException)) {}

        /**
         * @brief Initializes a new instance with the class name and the method name.
         *
         * C++ counterpart of .NET MissingMethodException(string className, string methodName).
         */
        MissingMethodException(const std::string& className, const std::string& methodName)
            : MissingMemberException("Method not found: " + className + "." + methodName) {}
    };

} // namespace System
