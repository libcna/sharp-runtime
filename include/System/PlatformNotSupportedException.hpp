// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/NotSupportedException.hpp"

namespace System {

    class PlatformNotSupportedException : public NotSupportedException {
    public:
        PlatformNotSupportedException() : NotSupportedException("Operation is not supported on this platform.") {}
        explicit PlatformNotSupportedException(const std::string& message) : NotSupportedException(message) {}
        PlatformNotSupportedException(const std::string& message, const std::exception& inner)
            : NotSupportedException(message + " | inner: " + inner.what()) {}
    };

} // namespace System
