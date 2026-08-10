// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System::Runtime::CompilerServices {

/**
 * Reserved C# compiler metadata marker for init-only properties.
 *
 * C++ counterpart of .NET System.Runtime.CompilerServices.IsExternalInit. It deliberately has
 * no instances or runtime behaviour because C++ has no corresponding CLR metadata construct.
 */
class IsExternalInit final {
    IsExternalInit() = delete;
};

} // namespace System::Runtime::CompilerServices
