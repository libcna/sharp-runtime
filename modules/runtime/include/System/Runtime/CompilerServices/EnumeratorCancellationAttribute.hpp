// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include "System/Attribute.hpp"

namespace System::Runtime::CompilerServices {

/**
 * Marks an async-enumerator parameter that receives the enumerator cancellation token.
 *
 * C++ counterpart of .NET System.Runtime.CompilerServices.EnumeratorCancellationAttribute.
 * It is metadata-only because C++ coroutines have no C# async-enumeration lowering contract.
 */
class EnumeratorCancellationAttribute final : public System::Attribute {
public:
    /** Initializes an EnumeratorCancellationAttribute marker. */
    EnumeratorCancellationAttribute() = default;
};

} // namespace System::Runtime::CompilerServices
