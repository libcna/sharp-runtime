// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/SystemException.hpp"

namespace System {

    /**
     * @brief The exception that is thrown for errors in an arithmetic, casting,
     * or conversion operation.
     *
     * C++ counterpart of .NET System.ArithmeticException.
     * Base class for OverflowException, DivideByZeroException, and
     * NotFiniteNumberException.
     */
    class ArithmeticException : public SystemException {
    public:
        /**
         * @brief Initializes a new instance of ArithmeticException with the
         * default error message.
         *
         * C++ counterpart of .NET ArithmeticException().
         */
        ArithmeticException()
            : SystemException("Overflow or underflow in the arithmetic operation.") {}

        /**
         * @brief Initializes a new instance of ArithmeticException with the
         * specified error message.
         *
         * C++ counterpart of .NET ArithmeticException(string).
         * @param message The error message that explains the reason for the exception.
         */
        explicit ArithmeticException(const std::string& message)
            : SystemException(message) {}

        /**
         * @brief Initializes a new instance of ArithmeticException with the
         * specified error message (from a C string).
         *
         * C++ convenience overload — no .NET equivalent.
         * @param message The error message.
         */
        explicit ArithmeticException(const char* message)
            : SystemException(message) {}

        /**
         * @brief Initializes a new instance of ArithmeticException with a
         * specified error message and a reference to the inner exception that
         * is the cause of this exception.
         *
         * C++ counterpart of .NET ArithmeticException(string, Exception).
         * @param message The error message that explains the reason for the exception.
         * @param inner   The exception that is the cause of the current exception.
         */
        ArithmeticException(const std::string& message, const std::exception& inner)
            : SystemException(message + " | inner: " + inner.what()) {}
    };

} // namespace System
