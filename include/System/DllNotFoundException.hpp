// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/TypeLoadException.hpp"

namespace System {

    class DllNotFoundException : public TypeLoadException {
    public:
        DllNotFoundException() : TypeLoadException("Unable to load the DLL.") {}
        explicit DllNotFoundException(const std::string& message) : TypeLoadException(message) {}
        DllNotFoundException(const std::string& message, const std::exception& inner)
            : TypeLoadException(message + " | inner: " + inner.what()) {}
    };

} // namespace System
