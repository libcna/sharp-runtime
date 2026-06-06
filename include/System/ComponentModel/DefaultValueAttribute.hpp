// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <any>
#include <string>
#include "System/Attribute.hpp"

namespace System::ComponentModel {

    class DefaultValueAttribute : public System::Attribute {
        std::any value_;
    public:
        explicit DefaultValueAttribute(bool v)        : value_(v) {}
        explicit DefaultValueAttribute(int v)         : value_(v) {}
        explicit DefaultValueAttribute(long v)        : value_(v) {}
        explicit DefaultValueAttribute(double v)      : value_(v) {}
        explicit DefaultValueAttribute(float v)       : value_(v) {}
        explicit DefaultValueAttribute(char v)        : value_(v) {}
        explicit DefaultValueAttribute(const std::string& v) : value_(v) {}
        explicit DefaultValueAttribute(const std::any& v) : value_(v) {}

        [[nodiscard]] const std::any& getValueProperty() const { return value_; }
    };

} // namespace System::ComponentModel
