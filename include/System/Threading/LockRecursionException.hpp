// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Exception.hpp"

namespace System::Threading {

    class LockRecursionException : public System::Exception {
    public:
        LockRecursionException() : Exception("Recursive lock entry is not allowed.") {}
        explicit LockRecursionException(const std::string& message) : Exception(message) {}
        LockRecursionException(const std::string& message, const std::exception& inner)
            : Exception(message + " | inner: " + inner.what()) {}
    };

} // namespace System::Threading
