// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include "System/Attribute.hpp"

namespace System::Runtime::CompilerServices {

/**
 * Indicates that a method should run as part of its containing module's initializer.
 *
 * C++ counterpart of .NET System.Runtime.CompilerServices.ModuleInitializerAttribute. It is a
 * marker only; C++ initialization should use ordinary static initialization or explicit startup.
 */
class ModuleInitializerAttribute final : public System::Attribute {
public:
    /** Initializes a ModuleInitializerAttribute marker. */
    ModuleInitializerAttribute() = default;
};

} // namespace System::Runtime::CompilerServices
