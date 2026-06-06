// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Created by robertvokac on 6/5/25.
//

#include "System/ArgumentException.hpp"

namespace System {

    /**
     * \brief Initializes a new instance of the ArgumentException class
     * with an empty message.
     */
    ArgumentException::ArgumentException()
        : SystemException() {
    }

    /**
     * \brief Initializes a new instance of the ArgumentException class
     * with the specified error message.
     * \param str A null-terminated character string that describes the error.
     */
    ArgumentException::ArgumentException(const char* str)
        : SystemException(str) {
    }

    /**
     * \brief Initializes a new instance of the ArgumentException class
     * with the specified error message.
     * \param str A string that describes the error.
     */
    ArgumentException::ArgumentException(const std::string& str)
        : SystemException(str) {
    }

    ArgumentException::ArgumentException(const char* message, const char* paramName)
        : SystemException(std::string(message) + " (Parameter '" + paramName + "')") {
    }

    ArgumentException::ArgumentException(const std::string& message, const std::string& paramName)
        : SystemException(message + " (Parameter '" + paramName + "')") {
    }

} // namespace System