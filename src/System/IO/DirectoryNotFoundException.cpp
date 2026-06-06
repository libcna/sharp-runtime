// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/IO/DirectoryNotFoundException.hpp"
namespace System::IO {
    DirectoryNotFoundException::DirectoryNotFoundException() : IOException("Attempted to access a path that is not on the disk.") {}
    DirectoryNotFoundException::DirectoryNotFoundException(const char* message) : IOException(message) {}
    DirectoryNotFoundException::DirectoryNotFoundException(const std::string& message) : IOException(message) {}
} // namespace System::IO
