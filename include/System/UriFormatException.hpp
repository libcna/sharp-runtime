// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/FormatException.hpp"

namespace System {

    /**
     * @brief The exception thrown when an invalid Uniform Resource Identifier (URI) is detected.
     *
     * C++ counterpart of .NET System.UriFormatException.
     * Derives from FormatException.
     */
    class UriFormatException : public FormatException {
    public:
        /** @brief Initializes a new instance with the default URI-format message. */
        UriFormatException() : FormatException("Invalid URI: The URI format could not be determined.") {}
        /** @brief Initializes a new instance with the specified error message. */
        explicit UriFormatException(const std::string& message) : FormatException(message) {}
        /** @brief Initializes a new instance with the specified message and inner exception. */
        UriFormatException(const std::string& message, const std::exception& inner)
            : FormatException(message + " | inner: " + inner.what()) {}
    };

} // namespace System
