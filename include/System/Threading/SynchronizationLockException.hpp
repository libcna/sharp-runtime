// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/SystemException.hpp"

namespace System::Threading {

    class SynchronizationLockException : public System::SystemException {
    public:
        SynchronizationLockException() : SystemException("Object synchronization method was called from an unsynchronized block of code.") {}
        explicit SynchronizationLockException(const std::string& message) : SystemException(message) {}
        SynchronizationLockException(const std::string& message, const std::exception& inner)
            : SystemException(message + " | inner: " + inner.what()) {}
    };

} // namespace System::Threading
