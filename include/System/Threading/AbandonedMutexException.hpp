// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/SystemException.hpp"

namespace System::Threading {

    class AbandonedMutexException : public System::SystemException {
    public:
        AbandonedMutexException() : SystemException("The wait completed due to an abandoned mutex.") {}
        explicit AbandonedMutexException(const std::string& message) : SystemException(message) {}
        AbandonedMutexException(const std::string& message, const std::exception& inner)
            : SystemException(message + " | inner: " + inner.what()) {}
    };

} // namespace System::Threading
