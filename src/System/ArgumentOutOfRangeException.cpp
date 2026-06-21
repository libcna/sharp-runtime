// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/ArgumentOutOfRangeException.hpp"

namespace System {

    ArgumentOutOfRangeException::ArgumentOutOfRangeException()
        : ArgumentException("Specified argument was out of the range of valid values.") {}

    ArgumentOutOfRangeException::ArgumentOutOfRangeException(const char* message)
        : ArgumentException(message) {}

    ArgumentOutOfRangeException::ArgumentOutOfRangeException(const std::string& message)
        : ArgumentException(message) {}

    ArgumentOutOfRangeException::ArgumentOutOfRangeException(const std::string& message,
                                                             const std::exception& inner)
        : ArgumentException(message + " | inner: " + inner.what()) {}

    ArgumentOutOfRangeException::ArgumentOutOfRangeException(const std::string& paramName,
                                                             const std::string& actualValue,
                                                             const std::string& message)
        : ArgumentException(message, paramName), actualValue_(actualValue) {}

} // namespace System
