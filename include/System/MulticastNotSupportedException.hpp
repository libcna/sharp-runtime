// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/SystemException.hpp"

namespace System {

    class MulticastNotSupportedException : public SystemException {
    public:
        MulticastNotSupportedException() : SystemException("Attempted to combine delegates that are not multicast.") {}
        explicit MulticastNotSupportedException(const std::string& message) : SystemException(message) {}
    };

} // namespace System
