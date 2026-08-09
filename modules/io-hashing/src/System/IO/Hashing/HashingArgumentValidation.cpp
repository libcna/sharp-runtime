// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/IO/Hashing/HashingArgumentValidation.hpp"
#include "System/ArgumentOutOfRangeException.hpp"

namespace System::IO::Hashing::Detail {

    void ThrowNegativeLength() {
        throw System::ArgumentOutOfRangeException("length", "Non-negative number required.");
    }

} // namespace System::IO::Hashing::Detail
