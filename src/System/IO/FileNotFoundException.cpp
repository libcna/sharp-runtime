// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/IO/FileNotFoundException.hpp"

namespace System::IO {

    namespace {
        constexpr const char* DefaultMsg = "Unable to find the specified file.";
    }

    FileNotFoundException::FileNotFoundException()
        : IOException(DefaultMsg) {}

    FileNotFoundException::FileNotFoundException(const char* message)
        : IOException(message) {}

    FileNotFoundException::FileNotFoundException(const std::string& message)
        : IOException(message) {}

    FileNotFoundException::FileNotFoundException(const std::string& message, const std::string& fileName)
        : IOException(message), fileName_(fileName) {}

} // namespace System::IO
