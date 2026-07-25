// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include "System/Attribute.hpp"

namespace System::Runtime::CompilerServices {

/**
 * Indicates that the C# compiler should omit the IL `.locals init` flag.
 *
 * C++ counterpart of .NET System.Runtime.CompilerServices.SkipLocalsInitAttribute. It has no
 * C++ runtime effect because local-initialization policy is not represented by CLR metadata.
 */
class SkipLocalsInitAttribute final : public System::Attribute {
public:
    /** Initializes a SkipLocalsInitAttribute marker. */
    SkipLocalsInitAttribute() = default;
};

} // namespace System::Runtime::CompilerServices
