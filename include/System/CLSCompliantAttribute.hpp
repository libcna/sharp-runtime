// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Attribute.hpp"

namespace System {

    /// Indicates whether a program element is compliant with the Common Language Specification.
    class CLSCompliantAttribute : public Attribute {
        bool isCompliant_;
    public:
        /// Initializes a new instance indicating the CLS compliance of the target element.
        explicit CLSCompliantAttribute(bool isCompliant) : isCompliant_(isCompliant) {}
        /// Returns true if the attributed element is CLS-compliant.
        [[nodiscard]] bool getIsCompliantProperty() const { return isCompliant_; }
    };

} // namespace System
