// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/SystemException.hpp"

namespace System {

    class RankException : public SystemException {
    public:
        RankException() : SystemException("Attempted to operate on an array with the incorrect number of dimensions.") {}
        explicit RankException(const std::string& message) : SystemException(message) {}
        RankException(const std::string& message, const std::exception& inner)
            : SystemException(message + " | inner: " + inner.what()) {}
    };

} // namespace System
