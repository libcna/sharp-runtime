// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Xml/Linq/XStreamingElement.hpp"
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "System/Xml/XmlException.hpp"
#include "System/Xml/XmlWriter.hpp"
#include "NamespaceScope.hpp"

namespace System::Xml::Linq {

    void XStreamingElement::Add(const std::string& text) { content_.emplace_back(text); }
    void XStreamingElement::Add(std::shared_ptr<XAttribute> attr) { content_.emplace_back(std::move(attr)); }
    void XStreamingElement::Add(std::shared_ptr<XNode> node) { content_.emplace_back(std::move(node)); }
    void XStreamingElement::Add(std::shared_ptr<XStreamingElement> nested) { content_.emplace_back(std::move(nested)); }

    void XStreamingElement::WriteContent(System::Xml::XmlWriter& writer, const std::vector<std::any>& items) const {
        for (const auto& item : items) {
            if (const auto* s = std::any_cast<std::string>(&item)) {
                writer.WriteString(*s);
            } else if (std::any_cast<std::shared_ptr<XAttribute>>(&item) != nullptr) {
                // Attributes were already written by WriteTo, which needs all of them before it
                // can resolve namespace prefixes for the start tag.
                continue;
            } else if (const auto* n = std::any_cast<std::shared_ptr<XNode>>(&item)) {
                if (*n) (*n)->WriteTo(writer);
            } else if (const auto* se = std::any_cast<std::shared_ptr<XStreamingElement>>(&item)) {
                if (*se) (*se)->WriteTo(writer);
            }
            // Any other stored type is silently skipped — see class doc-comment on the scoped content model.
        }
    }

    void XStreamingElement::WriteTo(System::Xml::XmlWriter& writer) const {
        // Ticket #2197 (SR-AUD-334). This door wrote local names only, exactly as
        // XElement::WriteTo did and for the same recorded reason, so it inherited the same
        // defect: a namespaced streaming element serialized unqualified and a namespace
        // declaration among its content degraded into an ordinary attribute.
        //
        // A streaming element is not part of the node hierarchy, so it has no ancestors to
        // inherit declarations from: its scope is exactly what its own attribute content
        // declares. A nested XStreamingElement or XElement therefore resolves its own scope
        // independently and may re-declare a prefix its parent already bound. That is
        // redundant, never wrong, and is the price of a content model with no parent link.
        detail::NamespaceScope scope;
        for (const auto& item : content_) {
            const auto* a = std::any_cast<std::shared_ptr<XAttribute>>(&item);
            if (!a || !*a || !(*a)->getIsNamespaceDeclarationProperty()) continue;
            const XName& n = (*a)->getNameProperty();
            scope.Declare(detail::DeclaredPrefix(n.getNamespaceNameProperty(), n.getLocalNameProperty()),
                          (*a)->getValueProperty());
        }

        std::vector<std::pair<std::string, std::string>> attributes;
        std::string elementPrefix;
        const std::string& elementUri = name_.getNamespaceNameProperty();
        if (elementUri.empty()) {
            if (!scope.DefaultUri().empty()) {
                attributes.emplace_back(detail::DeclarationAttributeName(std::string()), std::string());
                scope.Declare(std::string(), std::string());
            }
        } else if (!scope.TryLookupUri(elementUri, /*allowDefault=*/true, elementPrefix)) {
            elementPrefix = scope.AllocatePrefix();
            attributes.emplace_back(detail::DeclarationAttributeName(elementPrefix), elementUri);
            scope.Declare(elementPrefix, elementUri);
        }

        for (const auto& item : content_) {
            const auto* a = std::any_cast<std::shared_ptr<XAttribute>>(&item);
            if (!a || !*a) continue;
            const XName& n = (*a)->getNameProperty();
            if ((*a)->getIsNamespaceDeclarationProperty()) {
                attributes.emplace_back(
                    detail::DeclarationAttributeName(detail::DeclaredPrefix(
                        n.getNamespaceNameProperty(), n.getLocalNameProperty())),
                    (*a)->getValueProperty());
                continue;
            }
            const std::string& uri = n.getNamespaceNameProperty();
            if (uri.empty()) {
                attributes.emplace_back(n.getLocalNameProperty(), (*a)->getValueProperty());
                continue;
            }
            std::string prefix;
            if (!scope.TryLookupUri(uri, /*allowDefault=*/false, prefix)) {
                prefix = scope.AllocatePrefix();
                attributes.emplace_back(detail::DeclarationAttributeName(prefix), uri);
                scope.Declare(prefix, uri);
            }
            attributes.emplace_back(detail::QualifiedName(prefix, n.getLocalNameProperty()),
                                    (*a)->getValueProperty());
        }

        writer.WriteStartElement(detail::QualifiedName(elementPrefix, name_.getLocalNameProperty()));
        for (const auto& a : attributes) writer.WriteAttributeString(a.first, a.second);
        WriteContent(writer, content_);
        writer.WriteEndElement();
    }

    void XStreamingElement::Save(const std::string& fileName, SaveOptions /*options*/) const {
        std::unique_ptr<System::Xml::XmlWriter> w(System::Xml::XmlWriter::Create(fileName));
        if (!w) throw System::Xml::XmlException("XStreamingElement::Save: failed to open '" + fileName + "'.");
        WriteTo(*w);
        w->Close();
    }

    void XStreamingElement::Save(System::Xml::XmlWriter& writer) const {
        writer.WriteStartDocument();
        WriteTo(writer);
        writer.WriteEndDocument();
    }

    std::string XStreamingElement::ToString(SaveOptions /*options*/) const {
        std::unique_ptr<System::Xml::XmlWriter> w(System::Xml::XmlWriter::CreateToString());
        WriteTo(*w);
        return w->ToString();
    }

} // namespace System::Xml::Linq
