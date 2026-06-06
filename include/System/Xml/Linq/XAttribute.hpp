// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/Xml/Linq/XName.hpp"

namespace System::Xml::Linq {

    class XAttribute {
        XName name_;
        std::string value_;
        XAttribute* next_ = nullptr;

    public:
        XAttribute(const XName& name, const std::string& value)
            : name_(name), value_(value) {}
        XAttribute(const std::string& name, const std::string& value)
            : name_(name), value_(value) {}

        [[nodiscard]] const XName&       getNameProperty()  const { return name_; }
        [[nodiscard]] const std::string& getValueProperty() const { return value_; }
        void setValueProperty(const std::string& v)               { value_ = v; }

        [[nodiscard]] XAttribute* getNextAttributeProperty() const { return next_; }
        void setNextAttributeProperty(XAttribute* n)               { next_ = n; }

        [[nodiscard]] std::string ToString() const {
            return name_.ToString() + "=\"" + value_ + "\"";
        }
    };

} // namespace System::Xml::Linq
