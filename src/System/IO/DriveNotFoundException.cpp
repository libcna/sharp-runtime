// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/IO/DriveNotFoundException.hpp"

namespace System::IO {

    DriveNotFoundException::DriveNotFoundException()
        : IOException("Could not find the drive. The drive might not be ready or might not be mapped.") {}

    DriveNotFoundException::DriveNotFoundException(const char* message)
        : IOException(message) {}

    DriveNotFoundException::DriveNotFoundException(const std::string& message)
        : IOException(message) {}

    DriveNotFoundException::DriveNotFoundException(const std::string& message, std::exception_ptr inner)
        : IOException(message, std::move(inner)) {}

} // namespace System::IO
