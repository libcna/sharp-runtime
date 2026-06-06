// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Attribute.hpp"
#include "System/AttributeTargets.hpp"

namespace System {

    class AttributeUsageAttribute : public Attribute {
        AttributeTargets validOn_;
        bool allowMultiple_ = false;
        bool inherited_ = true;

    public:
        explicit AttributeUsageAttribute(AttributeTargets validOn)
            : validOn_(validOn), inherited_(true) {}

        [[nodiscard]] AttributeTargets getValidOnProperty()      const { return validOn_; }
        [[nodiscard]] bool             getAllowMultipleProperty() const { return allowMultiple_; }
        [[nodiscard]] bool             getInheritedProperty()    const { return inherited_; }

        void setAllowMultipleProperty(bool v) { allowMultiple_ = v; }
        void setInheritedProperty(bool v)     { inherited_ = v; }
    };

} // namespace System
