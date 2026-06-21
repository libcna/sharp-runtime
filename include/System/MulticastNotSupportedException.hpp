// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/SystemException.hpp"

namespace System {

    /**
     * @brief The exception thrown when there is an attempt to combine two delegates based
     * on the Delegate type instead of the MulticastDelegate type.
     *
     * C++ counterpart of .NET System.MulticastNotSupportedException.
     */
    class MulticastNotSupportedException : public SystemException {
    public:
        /** @brief Initializes a new instance with the default multicast message. */
        MulticastNotSupportedException() : SystemException("Attempted to combine delegates that are not multicast.") {}
        /** @brief Initializes a new instance with the specified error message. */
        explicit MulticastNotSupportedException(const std::string& message) : SystemException(message) {}
        /** @brief Initializes a new instance with the specified message and inner exception. */
        MulticastNotSupportedException(const std::string& message, const std::exception& innerException)
            : SystemException(message + " | inner: " + innerException.what()) {}
    };

} // namespace System
