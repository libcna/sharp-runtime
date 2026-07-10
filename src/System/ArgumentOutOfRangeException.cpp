// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/ArgumentOutOfRangeException.hpp"

namespace System {

    ArgumentOutOfRangeException::ArgumentOutOfRangeException()
        : ArgumentException("Specified argument was out of the range of valid values.") {
        setHResultProperty(static_cast<SharpRuntime::intcs>(0x80131502)); // COR_E_ARGUMENTOUTOFRANGE
    }

    static const char* kDefaultMsg = "Specified argument was out of the range of valid values.";

    ArgumentOutOfRangeException::ArgumentOutOfRangeException(const char* paramName)
        : ArgumentException(kDefaultMsg, paramName ? paramName : "") {
        setHResultProperty(static_cast<SharpRuntime::intcs>(0x80131502)); // COR_E_ARGUMENTOUTOFRANGE
    }

    ArgumentOutOfRangeException::ArgumentOutOfRangeException(const std::string& paramName)
        : ArgumentException(kDefaultMsg, paramName) {
        setHResultProperty(static_cast<SharpRuntime::intcs>(0x80131502)); // COR_E_ARGUMENTOUTOFRANGE
    }

    ArgumentOutOfRangeException::ArgumentOutOfRangeException(const std::string& message,
                                                             std::exception_ptr inner)
        : ArgumentException(message, std::move(inner)) {
        setHResultProperty(static_cast<SharpRuntime::intcs>(0x80131502)); // COR_E_ARGUMENTOUTOFRANGE
    }

    ArgumentOutOfRangeException::ArgumentOutOfRangeException(const std::string& paramName,
                                                             const std::string& message)
        : ArgumentException(message, paramName) {
        setHResultProperty(static_cast<SharpRuntime::intcs>(0x80131502)); // COR_E_ARGUMENTOUTOFRANGE
    }

    ArgumentOutOfRangeException::ArgumentOutOfRangeException(const std::string& paramName,
                                                             const std::string& actualValue,
                                                             const std::string& message)
        : ArgumentException(message, paramName), actualValue_(actualValue) {
        setHResultProperty(static_cast<SharpRuntime::intcs>(0x80131502)); // COR_E_ARGUMENTOUTOFRANGE
    }

} // namespace System
