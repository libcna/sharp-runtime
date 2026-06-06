// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Exception.hpp"

namespace System::Diagnostics {

    // Thrown when code that should be unreachable is actually executed.
    class UnreachableException : public System::Exception {
    public:
        UnreachableException() : Exception("The program executed an instruction that was thought to be unreachable.") {}
        explicit UnreachableException(const std::string& message) : Exception(message) {}
        UnreachableException(const std::string& message, const std::exception& inner)
            : Exception(message + " | inner: " + inner.what()) {}
    };

} // namespace System::Diagnostics
