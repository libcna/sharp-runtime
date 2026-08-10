// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/IO/Hashing/HashingArgumentValidation.hpp"
#include "System/ArgumentException.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"

namespace System::IO::Hashing::Detail {

    void ThrowNegativeLength() {
        throw System::ArgumentOutOfRangeException("length", "Non-negative number required.");
    }

    void ThrowNullSource() {
        throw System::ArgumentNullException("source");
    }

    void ThrowNullDestination() {
        throw System::ArgumentNullException("destination");
    }

    void ThrowDestinationTooShort() {
        throw System::ArgumentException("Destination is too short.", "destination");
    }

} // namespace System::IO::Hashing::Detail
