// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/SystemException.hpp"

namespace System {

    /**
     * @brief The exception that is thrown when there is an attempt to access a
     * type that has been unloaded.
     */
    class TypeUnloadedException : public SystemException {
    public:
        /** @brief Initializes a new instance with the default message. */
        TypeUnloadedException() : SystemException("Type had been unloaded.") {}
        /** @brief Initializes a new instance with the specified error message. */
        explicit TypeUnloadedException(const std::string& message) : SystemException(message) {}
        /** @brief Initializes a new instance with the specified message and inner exception. */
        TypeUnloadedException(const std::string& message, std::exception_ptr inner)
            : SystemException(message, inner) {}
    };

} // namespace System
