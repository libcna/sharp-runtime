// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>

namespace System::Threading {

    /// Represents a method that executes on a Thread with no parameters.
    using ThreadStart            = std::function<void()>;
    /// Represents a method that executes on a Thread and receives a single void* parameter.
    using ParameterizedThreadStart = std::function<void(void*)>;

} // namespace System::Threading
