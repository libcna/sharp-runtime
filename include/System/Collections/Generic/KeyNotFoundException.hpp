// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/SystemException.hpp"

namespace System::Collections::Generic {

    class KeyNotFoundException : public System::SystemException {
    public:
        KeyNotFoundException() : SystemException("The given key was not present in the dictionary.") {}
        explicit KeyNotFoundException(const std::string& message) : SystemException(message) {}
        KeyNotFoundException(const std::string& message, const std::exception& inner)
            : SystemException(message + " | inner: " + inner.what()) {}
    };

} // namespace System::Collections::Generic
