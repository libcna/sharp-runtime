// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/IO/IOException.hpp"

namespace System::IO {

    class PathTooLongException : public IOException {
    public:
        PathTooLongException() : IOException("The specified path, file name, or both are too long. The fully qualified file name must be less than 260 characters, and the directory name must be less than 248 characters.") {}
        explicit PathTooLongException(const std::string& message) : IOException(message) {}
        PathTooLongException(const std::string& message, const std::exception& inner)
            : IOException(message + " | inner: " + inner.what()) {}
    };

} // namespace System::IO
