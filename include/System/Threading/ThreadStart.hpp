// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>

namespace System::Threading {

    // Delegate types for thread entry points.
    using ThreadStart            = std::function<void()>;
    using ParameterizedThreadStart = std::function<void(void*)>;

} // namespace System::Threading
