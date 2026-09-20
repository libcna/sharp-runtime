// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <exception>
#include <string>
#include <utility>

#include "System/Exception.hpp"

namespace System::Runtime::Serialization {

    /**
     * @brief The exception thrown when an error occurs during serialization or deserialization.
     *
     * C++ counterpart of .NET System.Runtime.Serialization.SerializationException. Thrown by
     * SerializationInfo when a name is added twice, requested and absent, or requested as a type it
     * was not stored as.
     */
    class SerializationException : public System::Exception {
    public:
        /** @brief Initializes a new instance with .NET's default message. */
        SerializationException()
            : System::Exception("Serialization error.") {}

        /**
         * @brief Initializes a new instance with the specified message.
         * @param message A string describing the error.
         */
        explicit SerializationException(const std::string& message)
            : System::Exception(message) {}

        /**
         * @brief Initializes a new instance with a message and a reference to an inner exception.
         * @param message A string describing the error.
         * @param inner The exception that caused this one.
         */
        SerializationException(const std::string& message, std::exception_ptr inner)
            : System::Exception(message, std::move(inner)) {}
    };

} // namespace System::Runtime::Serialization
