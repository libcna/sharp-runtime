// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Attribute.hpp"
#include "System/AttributeTargets.hpp"

namespace System {

    /// Specifies the usage of another attribute class.
    class AttributeUsageAttribute : public Attribute {
        AttributeTargets validOn_;
        bool allowMultiple_ = false;
        bool inherited_ = true;

    public:
        /// Initializes a new instance specifying the valid target elements.
        explicit AttributeUsageAttribute(AttributeTargets validOn)
            : validOn_(validOn), inherited_(true) {}

        /// Returns the set of application elements on which the attribute is valid.
        [[nodiscard]] AttributeTargets getValidOnProperty()      const { return validOn_; }
        /// Returns whether more than one instance of the attribute is allowed on a single element.
        [[nodiscard]] bool             getAllowMultipleProperty() const { return allowMultiple_; }
        /// Returns whether the attribute is inherited by derived classes.
        [[nodiscard]] bool             getInheritedProperty()    const { return inherited_; }

        /// Sets whether more than one instance of the attribute is allowed on a single element.
        void setAllowMultipleProperty(bool v) { allowMultiple_ = v; }
        /// Sets whether the attribute is inherited by derived classes.
        void setInheritedProperty(bool v)     { inherited_ = v; }
    };

} // namespace System
