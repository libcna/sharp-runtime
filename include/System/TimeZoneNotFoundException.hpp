// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Exception.hpp"

namespace System {

    /** @brief The exception that is thrown when a time zone cannot be found. */
    class TimeZoneNotFoundException : public Exception {
    public:
        TimeZoneNotFoundException() : Exception("The time zone could not be found.") {}
        explicit TimeZoneNotFoundException(const std::string& message) : Exception(message) {}
        TimeZoneNotFoundException(const std::string& message, std::exception_ptr inner)
            : Exception(message, std::move(inner)) {}
    };

} // namespace System
