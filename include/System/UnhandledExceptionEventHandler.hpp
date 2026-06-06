// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>
#include "System/UnhandledExceptionEventArgs.hpp"

namespace System {
    using UnhandledExceptionEventHandler = std::function<void(void*, UnhandledExceptionEventArgs&)>;
} // namespace System
