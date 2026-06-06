// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Exception.hpp"

namespace System {

    class InvalidTimeZoneException : public Exception {
    public:
        InvalidTimeZoneException() : Exception("The time zone information is invalid.") {}
        explicit InvalidTimeZoneException(const std::string& message) : Exception(message) {}
        InvalidTimeZoneException(const std::string& message, const std::exception& inner)
            : Exception(message + " | inner: " + inner.what()) {}
    };

} // namespace System
