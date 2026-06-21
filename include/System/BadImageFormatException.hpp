// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/SystemException.hpp"

namespace System {

    /**
     * @brief The exception that is thrown when the file image of a dynamic-link
     * library (DLL) or an executable program is invalid.
     *
     * C++ counterpart of .NET System.BadImageFormatException.
     */
    class BadImageFormatException : public SystemException {
    public:
        /** @brief Initializes a new instance with the default invalid-image-format message. */
        BadImageFormatException()
            : SystemException("Format of the executable (.exe) or library (.dll) is invalid.") {}

        /** @brief Initializes a new instance with the specified message. */
        explicit BadImageFormatException(const std::string& message) : SystemException(message) {}

        /**
         * @brief Initializes a new instance with a message and an inner exception.
         *
         * C++ counterpart of .NET BadImageFormatException(string, Exception).
         */
        BadImageFormatException(const std::string& message, const std::exception& inner)
            : SystemException(message + " | inner: " + inner.what()) {}
    };

} // namespace System
