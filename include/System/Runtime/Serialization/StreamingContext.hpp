// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System::Runtime::Serialization {

    /**
     * @brief Describes the source and destination of a serialized stream.
     *
     * Minimal C++ stub of .NET System.Runtime.Serialization.StreamingContext.
     * Required to declare the protected serialization constructors on exception classes
     * that mirror .NET's ISerializable pattern. Full serialization is not implemented.
     */
    struct StreamingContext {
    };

} // namespace System::Runtime::Serialization
