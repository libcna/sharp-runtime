// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/IO/IOException.hpp"

namespace System::IO {

    namespace {
        constexpr const char* DefaultMsg = "I/O error occurred.";
    }

    IOException::IOException()
        : System::SystemException(DefaultMsg) {}

    IOException::IOException(const char* message)
        : System::SystemException(message) {}

    IOException::IOException(const std::string& message)
        : System::SystemException(message) {}

} // namespace System::IO
