// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "SystemException.hpp"
#include <string>

namespace System {

    /**
     * @brief The exception that is thrown when one of the arguments provided to a method is not valid.
     *
     * C++ counterpart of .NET System.ArgumentException.
     */
    class ArgumentException : public SystemException {
        std::string paramName_;

    public:
        /**
         * @brief Initializes a new instance of the ArgumentException class.
         *
         * C++ counterpart of .NET ArgumentException().
         */
        ArgumentException();

        /**
         * @brief Initializes a new instance with the specified error message.
         * @param message A message that describes the error.
         */
        explicit ArgumentException(const char* message);

        /**
         * @brief Initializes a new instance with the specified error message.
         * @param message A message that describes the error.
         */
        explicit ArgumentException(const std::string& message);

        /**
         * @brief Initializes a new instance with a message and a reference to the inner exception.
         * @param message        A message that describes the error.
         * @param innerException The exception that is the cause of the current exception.
         */
        ArgumentException(const std::string& message, std::exception_ptr innerException);

        /**
         * @brief Initializes a new instance with a message and the name of the parameter that caused this exception.
         * @param message   A message that describes the error.
         * @param paramName The name of the parameter that caused the current exception.
         */
        ArgumentException(const char* message, const char* paramName);

        /**
         * @brief Initializes a new instance with a message and the name of the parameter that caused this exception.
         * @param message   A message that describes the error.
         * @param paramName The name of the parameter that caused the current exception.
         */
        ArgumentException(const std::string& message, const std::string& paramName);

        /**
         * @brief Initializes a new instance with a message, parameter name, and inner exception.
         * @param message        A message that describes the error.
         * @param paramName      The name of the parameter that caused the current exception.
         * @param innerException The exception that is the cause of the current exception.
         */
        ArgumentException(const std::string& message, const std::string& paramName,
                          std::exception_ptr innerException);

        /**
         * @brief Gets the name of the parameter that caused the current exception.
         *
         * C++ counterpart of .NET ArgumentException.ParamName.
         * @return The parameter name, or an empty string if none was specified.
         */
        [[nodiscard]] virtual const std::string& getParamNameProperty() const noexcept {
            return paramName_;
        }

        /**
         * @brief Throws an ArgumentException if @p argument is empty.
         *
         * C++ counterpart of .NET ArgumentException.ThrowIfNullOrEmpty.
         * Note: C++ std::string cannot be null; only the empty check applies.
         * @param argument  The string argument to validate as non-empty.
         * @param paramName The name of the parameter with which @p argument corresponds.
         * @throws ArgumentException if @p argument is empty.
         */
        static void ThrowIfNullOrEmpty(const std::string& argument,
                                       const std::string& paramName = "");

        /**
         * @brief Throws an ArgumentException if @p argument is empty or consists only of white-space characters.
         *
         * C++ counterpart of .NET ArgumentException.ThrowIfNullOrWhiteSpace.
         * Note: C++ std::string cannot be null; only the empty/whitespace check applies.
         * @param argument  The string argument to validate.
         * @param paramName The name of the parameter with which @p argument corresponds.
         * @throws ArgumentException if @p argument is empty or whitespace-only.
         */
        static void ThrowIfNullOrWhiteSpace(const std::string& argument,
                                            const std::string& paramName = "");
    };

} // namespace System
