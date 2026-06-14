// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Attribute.hpp"

namespace System {
    /** @brief Indicates that a field of a serializable class should not be serialized. */
    class NonSerializedAttribute : public Attribute {};
} // namespace System
