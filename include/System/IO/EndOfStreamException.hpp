// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <string>

#include "System/IO/IOException.hpp"

namespace System::IO {

    /**
     * @brief The exception that is thrown when reading is attempted past the
     * end of a stream.
     *
     * @note Status: Implemented
     */
    class EndOfStreamException : public IOException {
    public:
        EndOfStreamException();
        explicit EndOfStreamException(const char* message);
        explicit EndOfStreamException(const std::string& message);
    };

} // namespace System::IO
