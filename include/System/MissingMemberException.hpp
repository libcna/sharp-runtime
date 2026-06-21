// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/MemberAccessException.hpp"

namespace System {

    /**
     * @brief The exception that is thrown when there is an attempt to dynamically
     * access a class member that does not exist or that is not declared as public.
     *
     * C++ counterpart of .NET System.MissingMemberException.
     */
    class MissingMemberException : public MemberAccessException {
    public:
        /** @brief Initializes a new instance with the default missing-member message. */
        MissingMemberException()
            : MemberAccessException("Attempted to access a missing member.") {}

        /** @brief Initializes a new instance with the specified message. */
        explicit MissingMemberException(const std::string& message)
            : MemberAccessException(message) {}

        /**
         * @brief Initializes a new instance with the class and member name.
         *
         * C++ counterpart of .NET MissingMemberException(string className, string memberName).
         */
        MissingMemberException(const std::string& className, const std::string& memberName)
            : MemberAccessException("Member not found: " + className + "." + memberName) {}

        /**
         * @brief Initializes a new instance with a message and an inner exception.
         *
         * C++ counterpart of .NET MissingMemberException(string, Exception).
         */
        MissingMemberException(const std::string& message, const std::exception& inner)
            : MemberAccessException(message + " | inner: " + inner.what()) {}
    };

} // namespace System
