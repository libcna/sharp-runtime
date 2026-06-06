// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/FormatException.hpp"

namespace System {

    namespace {
        constexpr const char* DefaultMsg = "One of the identified items was in an invalid format.";
    }

    FormatException::FormatException()
        : SystemException(DefaultMsg) {}

    FormatException::FormatException(const char* message)
        : SystemException(message) {}

    FormatException::FormatException(const std::string& message)
        : SystemException(message) {}

} // namespace System
