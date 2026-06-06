// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/SystemException.hpp"

namespace System {

    class DataMisalignedException : public SystemException {
    public:
        DataMisalignedException() : SystemException("A datatype misalignment was detected in a load or store instruction.") {}
        explicit DataMisalignedException(const std::string& message) : SystemException(message) {}
        DataMisalignedException(const std::string& message, const std::exception& inner)
            : SystemException(message + " | inner: " + inner.what()) {}
    };

} // namespace System
