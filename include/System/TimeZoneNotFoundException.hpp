// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Exception.hpp"

namespace System {

    class TimeZoneNotFoundException : public Exception {
    public:
        TimeZoneNotFoundException() : Exception("The time zone could not be found.") {}
        explicit TimeZoneNotFoundException(const std::string& message) : Exception(message) {}
        TimeZoneNotFoundException(const std::string& message, const std::exception& inner)
            : Exception(message + " | inner: " + inner.what()) {}
    };

} // namespace System
