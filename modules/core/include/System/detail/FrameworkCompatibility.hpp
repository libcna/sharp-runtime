// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/AppContext.hpp"

namespace System::detail {
/**
 * @brief Reports the opt-in .NET Framework parameter diagnostic format.
 * Standalone Sharp Runtime retains modern .NET messages. A .NET Framework host may
 * set AppContext switch SharpRuntime.UseNetFrameworkArgumentExceptionMessages before
 * constructing exceptions. This affects diagnostic text only, never validation or types.
 */
inline bool UseNetFrameworkArgumentExceptionMessages() {
    bool enabled = false;
    return System::AppContext::TryGetSwitch(
        "SharpRuntime.UseNetFrameworkArgumentExceptionMessages", enabled) && enabled;
}
}
