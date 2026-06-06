// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Attribute.hpp"

namespace System {

    class CLSCompliantAttribute : public Attribute {
        bool isCompliant_;
    public:
        explicit CLSCompliantAttribute(bool isCompliant) : isCompliant_(isCompliant) {}
        [[nodiscard]] bool getIsCompliantProperty() const { return isCompliant_; }
    };

} // namespace System
