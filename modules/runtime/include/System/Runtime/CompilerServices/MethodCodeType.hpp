// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System::Runtime::CompilerServices {

/**
 * Specifies the implementation type of a method.
 *
 * C++ counterpart of .NET System.Runtime.CompilerServices.MethodCodeType.
 * Values are preserved for metadata compatibility; C++ does not expose CLR method bodies.
 */
enum class MethodCodeType {
    IL = 0,
    Native = 1,
    OPTIL = 2,
    Runtime = 3,
};

} // namespace System::Runtime::CompilerServices
