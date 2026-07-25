// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include "System/Attribute.hpp"

namespace System::Runtime::CompilerServices {

/**
 * Indicates that an attributed type is an interpolated-string handler.
 *
 * C++ counterpart of .NET System.Runtime.CompilerServices.InterpolatedStringHandlerAttribute.
 * It stores no state because interpolated-string lowering is performed by the C# compiler.
 */
class InterpolatedStringHandlerAttribute final : public System::Attribute {
public:
    /** Initializes an InterpolatedStringHandlerAttribute marker. */
    InterpolatedStringHandlerAttribute() = default;
};

} // namespace System::Runtime::CompilerServices
