// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/TypeLoadException.hpp"

namespace System {

    /// The exception thrown when a DLL specified in a DLL import cannot be found.
    class DllNotFoundException : public TypeLoadException {
    public:
        /// Initializes a new instance with the default DLL-not-found message.
        DllNotFoundException() : TypeLoadException("Unable to load the DLL.") {}
        /// Initializes a new instance with the specified error message.
        explicit DllNotFoundException(const std::string& message) : TypeLoadException(message) {}
        /// Initializes a new instance with the specified message and inner exception.
        DllNotFoundException(const std::string& message, const std::exception& inner)
            : TypeLoadException(message + " | inner: " + inner.what()) {}
    };

} // namespace System
