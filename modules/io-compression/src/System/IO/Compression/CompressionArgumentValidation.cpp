// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/IO/Compression/CompressionArgumentValidation.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"

namespace System::IO::Compression::Detail {

    void ThrowNegativeLength(const char* paramName) {
        throw System::ArgumentOutOfRangeException(paramName, "Non-negative number required.");
    }

    void ThrowNullBuffer(const char* paramName) {
        throw System::ArgumentNullException(paramName);
    }

} // namespace System::IO::Compression::Detail
