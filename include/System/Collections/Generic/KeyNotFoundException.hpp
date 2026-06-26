// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/SystemException.hpp"

namespace System::Collections::Generic {

    /** Exception thrown when a key is not present in the dictionary. */
    class KeyNotFoundException : public System::SystemException {
    public:
        /** Constructs a KeyNotFoundException with the default message. */
        KeyNotFoundException() : SystemException("The given key was not present in the dictionary.") {}
        /** Constructs a KeyNotFoundException with the specified message. */
        explicit KeyNotFoundException(const std::string& message) : SystemException(message) {}
        /** Constructs a KeyNotFoundException with a message and an inner exception. */
        KeyNotFoundException(const std::string& message, std::exception_ptr inner)
            : SystemException(message, std::move(inner)) {}
    };

} // namespace System::Collections::Generic
