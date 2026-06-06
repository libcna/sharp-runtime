// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <string>

#include "System/SystemException.hpp"

namespace System::IO {

    /**
     * @brief The exception that is thrown when an I/O error occurs.
     *
     * @note Status: Implemented
     */
    class IOException : public System::SystemException {
    public:
        IOException();
        explicit IOException(const char* message);
        explicit IOException(const std::string& message);
    };

} // namespace System::IO
