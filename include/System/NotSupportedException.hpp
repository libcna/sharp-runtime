// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <string>

#include "System/SystemException.hpp"

namespace System {

    /**
     * @brief The exception that is thrown when an invoked method is not
     * supported, or when there is an attempt to read, seek, or write to a
     * stream that does not support the invoked functionality.
     *
     * @note Status: Implemented
     */
    class NotSupportedException : public SystemException {
    public:
        NotSupportedException();
        explicit NotSupportedException(const char* message);
        explicit NotSupportedException(const std::string& message);
    };

} // namespace System
