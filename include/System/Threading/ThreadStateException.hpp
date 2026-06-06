// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/SystemException.hpp"

namespace System::Threading {

    class ThreadStateException : public System::SystemException {
    public:
        ThreadStateException() : SystemException("Thread is in an invalid state for the operation being executed.") {}
        explicit ThreadStateException(const std::string& message) : SystemException(message) {}
        ThreadStateException(const std::string& message, const std::exception& inner)
            : SystemException(message + " | inner: " + inner.what()) {}
    };

} // namespace System::Threading
