// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/NotSupportedException.hpp"

namespace System {

    /**
     * @brief The exception thrown when a feature does not run on a particular platform.
     *
     * C++ counterpart of .NET System.PlatformNotSupportedException.
     */
    class PlatformNotSupportedException : public NotSupportedException {
    public:
        /** @brief Initializes a new instance with the default platform-not-supported message. */
        PlatformNotSupportedException() : NotSupportedException("Operation is not supported on this platform.") {}
        /** @brief Initializes a new instance with the specified error message. */
        explicit PlatformNotSupportedException(const std::string& message) : NotSupportedException(message) {}
        /** @brief Initializes a new instance with the specified message and inner exception. */
        PlatformNotSupportedException(const std::string& message, const std::exception& inner)
            : NotSupportedException(message + " | inner: " + inner.what()) {}
    };

} // namespace System
