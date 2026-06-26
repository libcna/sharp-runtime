// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/ArgumentOutOfRangeException.hpp"

namespace System {

    ArgumentOutOfRangeException::ArgumentOutOfRangeException()
        : ArgumentException("Specified argument was out of the range of valid values.") {}

    static const char* kDefaultMsg = "Specified argument was out of the range of valid values.";

    ArgumentOutOfRangeException::ArgumentOutOfRangeException(const char* paramName)
        : ArgumentException(kDefaultMsg, paramName ? paramName : "") {}

    ArgumentOutOfRangeException::ArgumentOutOfRangeException(const std::string& paramName)
        : ArgumentException(kDefaultMsg, paramName) {}

    ArgumentOutOfRangeException::ArgumentOutOfRangeException(const std::string& message,
                                                             std::exception_ptr inner)
        : ArgumentException(message, std::move(inner)) {}

    ArgumentOutOfRangeException::ArgumentOutOfRangeException(const std::string& paramName,
                                                             const std::string& message)
        : ArgumentException(message, paramName) {}

    ArgumentOutOfRangeException::ArgumentOutOfRangeException(const std::string& paramName,
                                                             const std::string& actualValue,
                                                             const std::string& message)
        : ArgumentException(message, paramName), actualValue_(actualValue) {}

} // namespace System
