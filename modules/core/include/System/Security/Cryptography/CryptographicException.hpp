// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <exception>
#include <string>
#include "System/SystemException.hpp"

namespace System::Security::Cryptography {

    /**
     * @brief The exception thrown when an error occurs during a cryptographic operation.
     *
     * C++ counterpart of .NET System.Security.Cryptography.CryptographicException.
     */
    class CryptographicException : public System::SystemException {
    public:
        /** @brief Initializes a new instance with a default message. */
        CryptographicException() : System::SystemException("Error occurred during a cryptographic operation.") {}

        /** @brief Initializes a new instance with the specified native error code as its HResult. */
        explicit CryptographicException(SharpRuntime::intcs hr)
            : System::SystemException("Error occurred during a cryptographic operation.") {
            setHResultProperty(hr);
        }

        /** @brief Initializes a new instance with the specified message. */
        explicit CryptographicException(const std::string& message) : System::SystemException(message) {}

        /** @brief Initializes a new instance with the specified message and inner exception. */
        CryptographicException(const std::string& message, std::exception_ptr innerException)
            : System::SystemException(message, std::move(innerException)) {}

        /**
         * @brief Initializes a new instance with the specified message and no inner exception.
         *
         * This overload has no .NET counterpart and exists only to keep the spelling
         * `CryptographicException(message, nullptr)` compiling with the meaning it already
         * had. Once the `(format, insert)` constructor below exists, `nullptr` reaches both
         * `std::exception_ptr` and the C++23 deleted `std::basic_string(std::nullptr_t)`
         * through equal-rank user-defined conversions, and the call would otherwise become
         * ambiguous. It delegates to the inner-exception constructor with an empty
         * `std::exception_ptr`, which is exactly what `nullptr` produced before.
         */
        CryptographicException(const std::string& message, std::nullptr_t)
            : CryptographicException(message, std::exception_ptr{}) {}

        /**
         * @brief Initializes a new instance whose message is @p format with @p insert substituted.
         *
         * C++ counterpart of .NET `CryptographicException(string, string?)`, which composes
         * its message with `string.Format(CultureInfo.CurrentCulture, format, insert)`. The
         * substitution is performed by `System::String::Format`, so @p format uses the one
         * composite-format grammar this port implements: `{0}` is replaced by @p insert,
         * `{{` and `}}` are literal braces, and a malformed @p format throws
         * `System::FormatException` rather than producing a partially substituted message.
         *
         * @param format A composite format string.
         * @param insert The text substituted for the format string's `{0}` item.
         */
        CryptographicException(const std::string& format, const std::string& insert);
    };

} // namespace System::Security::Cryptography
