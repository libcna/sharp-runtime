// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Xml/Linq/XAttribute.hpp"
#include "System/ArgumentException.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/Xml/Linq/XElement.hpp"
#include "System/Xml/Linq/XNamespace.hpp"
#include "System/Xml/Linq/detail/XLinqSerializationGuards.hpp"
#include "System/Xml/XmlConvert.hpp"
#include "NamespaceScope.hpp"

namespace System::Xml::Linq {

    namespace {
        // Verified against XAttribute.cs's ValidateAttribute: enforces the XML Namespaces
        // spec's constraints on namespace-declaration attributes (xmlns="..." /
        // xmlns:prefix="..."). A no-op for any ordinary (non-namespace-declaration) attribute.
        void ValidateAttribute(const XName& name, const std::string& value) {
            const std::string& namespaceName = name.getNamespaceNameProperty();
            const std::string& localName = name.getLocalNameProperty();
            if (namespaceName == XNamespace::Xmlns.getNamespaceNameProperty()) {
                // xmlns:localName="value" -- localName is the prefix being declared.
                if (value.empty()) {
                    throw System::ArgumentException(
                        "The empty namespace name cannot be declared with a prefixed namespace "
                        "declaration such as xmlns:" + localName + "='...'.");
                } else if (value == XNamespace::Xml.getNamespaceNameProperty()) {
                    // 'http://www.w3.org/XML/1998/namespace' can only be declared by the
                    // 'xml' prefix namespace declaration.
                    if (localName != "xml")
                        throw System::ArgumentException(
                            "The 'http://www.w3.org/XML/1998/namespace' namespace name can "
                            "only be declared with the 'xml' prefix.");
                } else if (value == XNamespace::Xmlns.getNamespaceNameProperty()) {
                    // 'http://www.w3.org/2000/xmlns/' must not be declared by any namespace
                    // declaration.
                    throw System::ArgumentException(
                        "The 'http://www.w3.org/2000/xmlns/' namespace name cannot be declared.");
                } else if (localName == "xml") {
                    // No other namespace name can be declared by the 'xml' prefix.
                    throw System::ArgumentException(
                        "The 'xml' prefix can only be declared to be "
                        "'http://www.w3.org/XML/1998/namespace'.");
                } else if (localName == "xmlns") {
                    // The 'xmlns' prefix must not be declared.
                    throw System::ArgumentException("The 'xmlns' prefix cannot be declared.");
                }
            } else if (namespaceName.empty() && localName == "xmlns") {
                // xmlns="value" -- the default namespace declaration.
                if (value == XNamespace::Xml.getNamespaceNameProperty())
                    throw System::ArgumentException(
                        "The 'http://www.w3.org/XML/1998/namespace' namespace name can only "
                        "be declared with the 'xml' prefix.");
                if (value == XNamespace::Xmlns.getNamespaceNameProperty())
                    throw System::ArgumentException(
                        "The 'http://www.w3.org/2000/xmlns/' namespace name cannot be declared.");
            }
        }
    }

    XAttribute::XAttribute(const XName& name, const std::string& value)
        : name_(name), value_(value) {
        ValidateAttribute(name_, value_);
    }

    void XAttribute::setValueProperty(const std::string& v) {
        ValidateAttribute(name_, v);
        value_ = v;
    }

    bool XAttribute::getIsNamespaceDeclarationProperty() const {
        const std::string& namespaceName = name_.getNamespaceNameProperty();
        if (namespaceName.empty())
            return name_.getLocalNameProperty() == "xmlns";
        return namespaceName == XNamespace::Xmlns.getNamespaceNameProperty();
    }

    std::string XAttribute::EscapeValue(const std::string& s) {
        std::string out;
        out.reserve(s.size());
        for (char c : s) {
            switch (c) {
                case '&': out += "&amp;"; break;
                case '<': out += "&lt;"; break;
                case '>': out += "&gt;"; break;
                case '"': out += "&quot;"; break;
                // Matches XmlEncodedRawTextWriter.WriteAttributeTextBlock's Tab/LineFeed/
                // CarriageReturnEntity: a literal tab/LF/CR written unescaped into an attribute
                // value is collapsed to a plain space by the XML spec's attribute-value
                // normalization on reload (section 3.3.3) -- character references are not
                // subject to that normalization, so escaping is the only way for the value to
                // round-trip unchanged.
                case '\t': out += "&#x9;"; break;
                case '\n': out += "&#xA;"; break;
                case '\r': out += "&#xD;"; break;
                default: out += c; break;
            }
        }
        return out;
    }

    std::string XAttribute::ToString() const {
        const std::string& uri = name_.getNamespaceNameProperty();
        const std::string& local = name_.getLocalNameProperty();

        // Ticket #2201. This is a direct serializer in its own right -- it is not reached through
        // any XNode::SerializeTo -- so it carries its own guard. The sibling path, an attribute
        // emitted as part of its element, is guarded in XElement::SerializeElementTo.
        detail::ThrowIfContainsNul(local, "XAttribute::ToString", "attribute name");
        detail::ThrowIfContainsNul(uri, "XAttribute::ToString", "attribute namespace name");
        detail::ThrowIfContainsNul(value_, "XAttribute::ToString", "attribute value");

        // Ticket #2350. The emitted name is built first so it can be validated as the single
        // string this door actually writes -- which is NOT always the local name: a declaration
        // renders as `xmlns`/`xmlns:p` and a qualified attribute as `prefix:local`. Every
        // branch's prefix is one this door chose and is itself a valid name, so the check falls
        // on the caller-supplied part exactly as XmlWriter::WriteAttributeString's does.
        std::string emittedName;
        std::string trailer; // an extra declaration this door must carry so the text stands alone

        // A declaration renders as itself. Before #2197 this door rendered `{xmlns-uri}p` as the
        // bare local name `p`, turning a namespace declaration into an ordinary attribute.
        if (getIsNamespaceDeclarationProperty()) {
            emittedName = detail::DeclarationAttributeName(detail::DeclaredPrefix(uri, local));
        } else if (uri.empty()) {
            emittedName = local;
        } else {
            // A qualified name needs a prefix. Ask the owning element for one it already has in
            // scope; a detached attribute, or one whose namespace nothing declares, gets a
            // generated prefix and carries its own declaration so the text stands alone.
            std::string prefix;
            if (const XElement* owner = getParentProperty())
                prefix = owner->GetPrefixOfNamespace(XNamespace(uri));
            if (!prefix.empty()) {
                emittedName = prefix + ":" + local;
            } else if (uri == detail::kXmlNamespaceUri) {
                emittedName = "xml:" + local;
            } else {
                emittedName = "p1:" + local;
                trailer = " xmlns:p1=\"" + EscapeValue(uri) + "\"";
            }
        }

        // Same validator, same diagnostic and same boundary as the element door and as #2196's
        // PI target: reject a name the sibling writer door already rejects, at the point the
        // text is produced -- never at XName construction.
        (void)System::Xml::XmlConvert::VerifyName(emittedName);
        return emittedName + "=\"" + EscapeValue(value_) + "\"" + trailer;
    }

    XAttribute* XAttribute::getPreviousAttributeProperty() const {
        XElement* owner = getParentProperty();
        if (owner == nullptr) return nullptr;
        XAttribute* prev = nullptr;
        for (const auto& a : owner->getAttributesProperty()) {
            if (a.get() == this) return prev;
            prev = a.get();
        }
        return nullptr;
    }

    void XAttribute::Remove() {
        XElement* owner = getParentProperty();
        if (owner == nullptr) throw System::InvalidOperationException("The parent is missing.");
        owner->RemoveAttribute(this);
    }

} // namespace System::Xml::Linq
