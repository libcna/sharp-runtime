// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>

namespace System::Runtime::Serialization {

    /**
     * @brief Stores all the data needed to serialize or deserialize an object.
     *
     * Minimal C++ stub of .NET System.Runtime.Serialization.SerializationInfo.
     * Required to declare the protected serialization constructors on exception classes
     * that mirror .NET's ISerializable pattern. Full serialization is not implemented.
     */
    class SerializationInfo {
    public:
        /** @brief Default constructor. */
        SerializationInfo() = default;
        virtual ~SerializationInfo() = default;
    };

} // namespace System::Runtime::Serialization
