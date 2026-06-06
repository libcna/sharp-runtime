// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <string>

#include "System/SystemException.hpp"

namespace System {

    /**
     * @brief The exception that is thrown when an attempt is made to access
     * an element of an array or collection with an index that is outside its bounds.
     *
     * @note Status: Implemented
     */
    class IndexOutOfRangeException : public SystemException {
    public:
        IndexOutOfRangeException();
        explicit IndexOutOfRangeException(const char* message);
        explicit IndexOutOfRangeException(const std::string& message);
    };

} // namespace System
