// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/Attribute.hpp"

namespace System::Diagnostics {

    class DebuggerDisplayAttribute : public System::Attribute {
        std::string value_;
        std::string name_;
        std::string type_;
        std::string target_;

    public:
        explicit DebuggerDisplayAttribute(const std::string& value) : value_(value) {}

        [[nodiscard]] const std::string& getValueProperty()  const { return value_; }
        [[nodiscard]] const std::string& getNameProperty()   const { return name_; }
        [[nodiscard]] const std::string& getTypeProperty()   const { return type_; }
        [[nodiscard]] const std::string& getTargetProperty() const { return target_; }

        void setNameProperty(const std::string& v)   { name_ = v; }
        void setTypeProperty(const std::string& v)   { type_ = v; }
        void setTargetProperty(const std::string& v) { target_ = v; }
    };

} // namespace System::Diagnostics
