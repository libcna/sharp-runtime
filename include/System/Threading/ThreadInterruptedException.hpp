// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/SystemException.hpp"

namespace System::Threading {

    class ThreadInterruptedException : public System::SystemException {
    public:
        ThreadInterruptedException() : SystemException("Thread was interrupted from a waiting state.") {}
        explicit ThreadInterruptedException(const std::string& message) : SystemException(message) {}
        ThreadInterruptedException(const std::string& message, const std::exception& inner)
            : SystemException(message + " | inner: " + inner.what()) {}
    };

} // namespace System::Threading
