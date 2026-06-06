// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/TypeLoadException.hpp"

namespace System {

    class EntryPointNotFoundException : public TypeLoadException {
    public:
        EntryPointNotFoundException() : TypeLoadException("Entry point was not found.") {}
        explicit EntryPointNotFoundException(const std::string& message) : TypeLoadException(message) {}
        EntryPointNotFoundException(const std::string& message, const std::exception& inner)
            : TypeLoadException(message + " | inner: " + inner.what()) {}
    };

} // namespace System
