// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include "System/Attribute.hpp"

namespace System::Runtime::CompilerServices {

/**
 * Indicates that a method, class, or assembly contains extension methods.
 *
 * C++ counterpart of .NET System.Runtime.CompilerServices.ExtensionAttribute. Extension methods
 * are an ordinary C# compiler feature; this marker is retained for source-level compatibility.
 */
class ExtensionAttribute final : public System::Attribute {
public:
    /** Initializes an ExtensionAttribute marker. */
    ExtensionAttribute() = default;
};

} // namespace System::Runtime::CompilerServices
