// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/SystemException.hpp"

namespace System {

    class CannotUnloadAppDomainException : public SystemException {
    public:
        CannotUnloadAppDomainException() : SystemException("Attempt to unload the AppDomain failed.") {}
        explicit CannotUnloadAppDomainException(const std::string& message) : SystemException(message) {}
        CannotUnloadAppDomainException(const std::string& message, const std::exception& inner)
            : SystemException(message + " | inner: " + inner.what()) {}
    };

} // namespace System
