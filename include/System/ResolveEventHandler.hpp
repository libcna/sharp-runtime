// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>
#include <string>
#include "System/ResolveEventArgs.hpp"

namespace System {
    using ResolveEventHandler = std::function<std::string(void*, ResolveEventArgs&)>;
} // namespace System
