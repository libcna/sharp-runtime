// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::Runtime::CompilerServices {

    /**
     * Specifies flags that control how the common language runtime implements a method.
     *
     * C++ counterpart of .NET System.Runtime.CompilerServices.MethodImplOptions. Most
     * flags are metadata or JIT hints and are consequently informational in this port.
     */
    enum class MethodImplOptions : SharpRuntime::intcs {
        Unmanaged         = 0x0004,
        NoInlining        = 0x0008,
        ForwardRef        = 0x0010,
        Synchronized      = 0x0020,
        NoOptimization    = 0x0040,
        PreserveSig       = 0x0080,
        AggressiveInlining = 0x0100,
        AggressiveOptimization = 0x0200,
        InternalCall      = 0x1000,
        Async             = 0x2000,
    };

    /** Combines two MethodImplOptions flag values. */
    [[nodiscard]] inline MethodImplOptions operator|(MethodImplOptions a, MethodImplOptions b) noexcept {
        return static_cast<MethodImplOptions>(static_cast<SharpRuntime::intcs>(a) |
                                              static_cast<SharpRuntime::intcs>(b));
    }

} // namespace System::Runtime::CompilerServices
