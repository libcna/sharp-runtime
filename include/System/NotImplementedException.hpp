// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Created by robertvokac on 6/7/25.
//

#pragma once

#include <string>

#include "System/SystemException.hpp"

namespace System {

    /**
     * @brief The exception that is thrown when a requested method or operation
     * is not implemented.
     *
     * This is a partial C++ counterpart of the .NET
     * System::NotImplementedException type.
     *
     * @note Status: Partial.
     * @note HResult, serialization, and inner exception support
     * are not implemented here.
     */
    class NotImplementedException : public SystemException {
    public:
        /**
         * @brief Initializes a new instance of the NotImplementedException class
         * with a default message.
         */
        NotImplementedException();

        /**
         * @brief Initializes a new instance of the NotImplementedException class
         * with the specified error message.
         *
         * @param message A null-terminated character string that describes the error.
         */
        explicit NotImplementedException(const char* message);

        /**
         * @brief Initializes a new instance of the NotImplementedException class
         * with the specified error message.
         *
         * @param message A string that describes the error.
         */
        explicit NotImplementedException(const std::string& message);

        /**
         * @brief Initializes a new instance with the specified error message and inner exception.
         *
         * @param message A string that describes the error.
         * @param inner   The exception that is the cause of the current exception.
         */
        NotImplementedException(const std::string& message, std::exception_ptr inner);
    };

} // namespace System
