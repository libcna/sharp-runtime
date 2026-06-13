// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>
#include "System/Xml/Linq/XName.hpp"
#include "System/Xml/Linq/XAttribute.hpp"

namespace System::Xml::Linq {

    /// Represents an XML element with a name, optional text value, attributes, and child elements.
    class XElement {
        XName name_;
        std::string value_;
        std::vector<std::shared_ptr<XAttribute>> attributes_;
        std::vector<std::shared_ptr<XElement>> children_;

    public:
        /// Constructs an empty element with the given XName.
        explicit XElement(const XName& name) : name_(name) {}

        /// Constructs an empty element with a plain string name.
        explicit XElement(const std::string& name) : name_(name) {}

        /// Constructs an element with an XName and an initial text value.
        XElement(const XName& name, const std::string& value) : name_(name), value_(value) {}

        /// @return The qualified name of the element.
        [[nodiscard]] const XName&       getNameProperty()  const { return name_; }

        /// @return The text content of the element.
        [[nodiscard]] const std::string& getValueProperty() const { return value_; }

        /// Sets the text content of the element.
        void setValueProperty(const std::string& v)               { value_ = v; }

        // Attributes

        /// Appends an attribute to this element.
        void Add(std::shared_ptr<XAttribute> attr) { attributes_.push_back(std::move(attr)); }

        /// Appends a child element to this element.
        void Add(std::shared_ptr<XElement> child)  { children_.push_back(std::move(child)); }

        /// Returns the first attribute matching @p name, or nullptr.
        [[nodiscard]] std::shared_ptr<XAttribute> Attribute(const XName& name) const {
            for (auto& a : attributes_)
                if (a->getNameProperty() == name) return a;
            return nullptr;
        }

        /// Returns the first attribute matching the plain string @p name, or nullptr.
        [[nodiscard]] std::shared_ptr<XAttribute> Attribute(const std::string& name) const {
            return Attribute(XName(name));
        }

        /// @return All attributes of this element.
        [[nodiscard]] const std::vector<std::shared_ptr<XAttribute>>& getAttributesProperty() const { return attributes_; }

        // Children

        /// Returns the first direct child element matching @p name, or nullptr.
        [[nodiscard]] std::shared_ptr<XElement> Element(const XName& name) const {
            for (auto& c : children_)
                if (c->getNameProperty() == name) return c;
            return nullptr;
        }

        /// Returns the first direct child element matching the plain string @p name, or nullptr.
        [[nodiscard]] std::shared_ptr<XElement> Element(const std::string& name) const {
            return Element(XName(name));
        }

        /// @return All direct child elements.
        [[nodiscard]] std::vector<std::shared_ptr<XElement>> Elements() const { return children_; }

        /// Returns all direct child elements whose name matches @p name.
        [[nodiscard]] std::vector<std::shared_ptr<XElement>> Elements(const XName& name) const {
            std::vector<std::shared_ptr<XElement>> result;
            for (auto& c : children_)
                if (c->getNameProperty() == name) result.push_back(c);
            return result;
        }

        /// Returns all direct child elements whose name matches the plain string @p name.
        [[nodiscard]] std::vector<std::shared_ptr<XElement>> Elements(const std::string& name) const {
            return Elements(XName(name));
        }

        /// Returns all descendant elements (recursive) whose name matches @p name.
        [[nodiscard]] std::vector<std::shared_ptr<XElement>> Descendants(const XName& name) const {
            std::vector<std::shared_ptr<XElement>> result;
            for (auto& c : children_) {
                if (c->getNameProperty() == name) result.push_back(c);
                auto sub = c->Descendants(name);
                result.insert(result.end(), sub.begin(), sub.end());
            }
            return result;
        }

        /// Returns the value of the attribute named @p name, or std::nullopt if absent.
        [[nodiscard]] std::optional<std::string> getAttributeValue(const std::string& name) const {
            auto a = Attribute(name);
            if (a) return a->getValueProperty();
            return std::nullopt;
        }

        /// Serialises the element to a string.
        /// @param indent If true, apply indentation.
        [[nodiscard]] std::string ToString(bool indent = false) const {
            std::ostringstream oss;
            writeXml(oss, 0, indent);
            return oss.str();
        }

        /// Parses XML text into an XElement (stub — requires XML parser backend).
        /// @param xml The XML source text (currently ignored).
        static std::shared_ptr<XElement> Parse(const std::string& /*xml*/) {
            // Stub — requires XML parser backend (tinyxml2/pugixml)
            return std::make_shared<XElement>("root");
        }

    private:
        void writeXml(std::ostringstream& oss, int depth, bool indent) const {
            std::string pad = indent ? std::string(depth * 2, ' ') : "";
            oss << pad << "<" << name_.getLocalNameProperty();
            for (auto& a : attributes_)
                oss << " " << a->ToString();
            if (children_.empty() && value_.empty()) {
                oss << "/>";
            } else {
                oss << ">";
                if (!value_.empty()) oss << value_;
                for (auto& c : children_) {
                    if (indent) oss << "\n";
                    c->writeXml(oss, depth + 1, indent);
                }
                if (!children_.empty() && indent) oss << "\n" << pad;
                oss << "</" << name_.getLocalNameProperty() << ">";
            }
        }
    };

} // namespace System::Xml::Linq
