// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/SystemException.hpp"

namespace System::Threading {

    class ThreadAbortException : public System::SystemException {
    public:
        ThreadAbortException() : SystemException("Thread was being aborted.") {}
        explicit ThreadAbortException(const std::string& message) : SystemException(message) {}
    };

} // namespace System::Threading
