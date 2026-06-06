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

    class XElement {
        XName name_;
        std::string value_;
        std::vector<std::shared_ptr<XAttribute>> attributes_;
        std::vector<std::shared_ptr<XElement>> children_;

    public:
        explicit XElement(const XName& name) : name_(name) {}
        explicit XElement(const std::string& name) : name_(name) {}
        XElement(const XName& name, const std::string& value) : name_(name), value_(value) {}

        [[nodiscard]] const XName&       getNameProperty()  const { return name_; }
        [[nodiscard]] const std::string& getValueProperty() const { return value_; }
        void setValueProperty(const std::string& v)               { value_ = v; }

        // Attributes
        void Add(std::shared_ptr<XAttribute> attr) { attributes_.push_back(std::move(attr)); }
        void Add(std::shared_ptr<XElement> child)  { children_.push_back(std::move(child)); }

        [[nodiscard]] std::shared_ptr<XAttribute> Attribute(const XName& name) const {
            for (auto& a : attributes_)
                if (a->getNameProperty() == name) return a;
            return nullptr;
        }
        [[nodiscard]] std::shared_ptr<XAttribute> Attribute(const std::string& name) const {
            return Attribute(XName(name));
        }

        [[nodiscard]] const std::vector<std::shared_ptr<XAttribute>>& getAttributesProperty() const { return attributes_; }

        // Children
        [[nodiscard]] std::shared_ptr<XElement> Element(const XName& name) const {
            for (auto& c : children_)
                if (c->getNameProperty() == name) return c;
            return nullptr;
        }
        [[nodiscard]] std::shared_ptr<XElement> Element(const std::string& name) const {
            return Element(XName(name));
        }

        [[nodiscard]] std::vector<std::shared_ptr<XElement>> Elements() const { return children_; }

        [[nodiscard]] std::vector<std::shared_ptr<XElement>> Elements(const XName& name) const {
            std::vector<std::shared_ptr<XElement>> result;
            for (auto& c : children_)
                if (c->getNameProperty() == name) result.push_back(c);
            return result;
        }
        [[nodiscard]] std::vector<std::shared_ptr<XElement>> Elements(const std::string& name) const {
            return Elements(XName(name));
        }

        [[nodiscard]] std::vector<std::shared_ptr<XElement>> Descendants(const XName& name) const {
            std::vector<std::shared_ptr<XElement>> result;
            for (auto& c : children_) {
                if (c->getNameProperty() == name) result.push_back(c);
                auto sub = c->Descendants(name);
                result.insert(result.end(), sub.begin(), sub.end());
            }
            return result;
        }

        [[nodiscard]] std::optional<std::string> getAttributeValue(const std::string& name) const {
            auto a = Attribute(name);
            if (a) return a->getValueProperty();
            return std::nullopt;
        }

        [[nodiscard]] std::string ToString(bool indent = false) const {
            std::ostringstream oss;
            writeXml(oss, 0, indent);
            return oss.str();
        }

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
