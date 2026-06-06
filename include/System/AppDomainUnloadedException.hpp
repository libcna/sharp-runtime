// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/SystemException.hpp"

namespace System {

    class AppDomainUnloadedException : public SystemException {
    public:
        AppDomainUnloadedException() : SystemException("Attempted to access an unloaded AppDomain.") {}
        explicit AppDomainUnloadedException(const std::string& message) : SystemException(message) {}
        AppDomainUnloadedException(const std::string& message, const std::exception& inner)
            : SystemException(message + " | inner: " + inner.what()) {}
    };

} // namespace System
