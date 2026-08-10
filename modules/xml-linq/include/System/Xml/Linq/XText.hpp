// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/Xml/Linq/XNode.hpp"

namespace System::Xml::Linq {

    /**
     * @brief Represents a text node.
     *
     * C++ counterpart of .NET System.Xml.Linq.XText.
     *
     * When directly contained by an XDocument, WriteTo serializes this node through
     * XmlWriter::WriteWhitespace; otherwise it uses XmlWriter::WriteString. This mirrors .NET's
     * distinction between legal prolog/epilog whitespace and ordinary element text.
     */
    class XText : public XNode {
        std::string text_;

    public:
        /** @brief Initializes a new text node with the given value. */
        explicit XText(const std::string& value) : text_(value) {}

        [[nodiscard]] System::Xml::XmlNodeType getNodeTypeProperty() const override { return System::Xml::XmlNodeType::Text; }

        /** @return The text content of this node. */
        [[nodiscard]] virtual const std::string& getValueProperty() const { return text_; }
        /** @brief Sets the text content of this node. */
        virtual void setValueProperty(const std::string& value) { text_ = value; }

        void WriteTo(System::Xml::XmlWriter& writer) const override;
        [[nodiscard]] SharpRuntime::intcs GetDeepHashCode() const override {
            return static_cast<SharpRuntime::intcs>(std::hash<std::string>{}(text_));
        }

    protected:
        void SerializeTo(std::ostream& os, int depth, bool indent) const override;
        [[nodiscard]] bool DeepEqualsCore(const XNode& other) const override;
    };

} // namespace System::Xml::Linq
