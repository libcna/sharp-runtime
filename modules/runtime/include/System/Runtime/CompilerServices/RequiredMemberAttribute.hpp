// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include "System/Attribute.hpp"

namespace System::Runtime::CompilerServices {

/**
 * Specifies that a type has required members or that a member is required.
 *
 * C++ counterpart of .NET System.Runtime.CompilerServices.RequiredMemberAttribute. It is an
 * inert marker because required-member validation is a C# compiler responsibility.
 */
class RequiredMemberAttribute final : public System::Attribute {
public:
    /** Initializes a RequiredMemberAttribute marker. */
    RequiredMemberAttribute() = default;
};

} // namespace System::Runtime::CompilerServices
