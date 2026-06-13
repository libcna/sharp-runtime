// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/SystemException.hpp"

namespace System {

    /// The exception thrown when a data type misalignment is detected in a load or store instruction.
    class DataMisalignedException : public SystemException {
    public:
        /// Initializes a new instance with the default misalignment message.
        DataMisalignedException() : SystemException("A datatype misalignment was detected in a load or store instruction.") {}
        /// Initializes a new instance with the specified error message.
        explicit DataMisalignedException(const std::string& message) : SystemException(message) {}
        /// Initializes a new instance with the specified message and inner exception.
        DataMisalignedException(const std::string& message, const std::exception& inner)
            : SystemException(message + " | inner: " + inner.what()) {}
    };

} // namespace System
