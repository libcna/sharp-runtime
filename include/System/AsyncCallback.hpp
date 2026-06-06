// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>
#include "System/IAsyncResult.hpp"

namespace System {
    using AsyncCallback = std::function<void(IAsyncResult&)>;
} // namespace System
