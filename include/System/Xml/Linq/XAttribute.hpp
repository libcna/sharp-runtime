// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/Xml/Linq/XName.hpp"

namespace System::Xml::Linq {

    /// Represents an XML attribute (name/value pair) on an XElement.
    class XAttribute {
        XName name_;
        std::string value_;
        XAttribute* next_ = nullptr;

    public:
        /// Constructs an attribute with an XName key and string value.
        XAttribute(const XName& name, const std::string& value)
            : name_(name), value_(value) {}

        /// Constructs an attribute with a plain string name and string value.
        XAttribute(const std::string& name, const std::string& value)
            : name_(name), value_(value) {}

        /// @return The qualified name of the attribute.
        [[nodiscard]] const XName&       getNameProperty()  const { return name_; }

        /// @return The attribute value string.
        [[nodiscard]] const std::string& getValueProperty() const { return value_; }

        /// Sets the attribute value.
        void setValueProperty(const std::string& v)               { value_ = v; }

        /// @return Pointer to the next sibling attribute in the linked list, or nullptr.
        [[nodiscard]] XAttribute* getNextAttributeProperty() const { return next_; }

        /// Sets the next sibling attribute in the linked list.
        void setNextAttributeProperty(XAttribute* n)               { next_ = n; }

        /// @return The attribute serialised as name="value".
        [[nodiscard]] std::string ToString() const {
            return name_.ToString() + "=\"" + value_ + "\"";
        }
    };

} // namespace System::Xml::Linq
