// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>

namespace System {

    /// Base class for all custom attributes.
    class Attribute {
    public:
        /// Virtual destructor for safe polymorphic destruction.
        virtual ~Attribute() = default;

        /// Returns true if this is the default instance of the attribute.
        virtual bool getIsDefaultAttributeProperty() const { return false; }

        /// Returns true if this attribute is equivalent to the specified attribute.
        virtual bool Match(const Attribute& obj) const {
            return this == &obj;
        }
    };

} // namespace System
