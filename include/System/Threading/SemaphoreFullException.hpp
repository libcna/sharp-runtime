// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/SystemException.hpp"

namespace System::Threading {

    class SemaphoreFullException : public System::SystemException {
    public:
        SemaphoreFullException() : SystemException("Adding the specified count to the semaphore would cause it to exceed its maximum count.") {}
        explicit SemaphoreFullException(const std::string& message) : SystemException(message) {}
        SemaphoreFullException(const std::string& message, const std::exception& inner)
            : SystemException(message + " | inner: " + inner.what()) {}
    };

} // namespace System::Threading
