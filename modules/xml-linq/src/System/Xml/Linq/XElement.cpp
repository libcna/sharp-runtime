// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Xml/Linq/XElement.hpp"
#include "System/Xml/Linq/detail/XLinqSerializationGuards.hpp"
#include <algorithm>
#include <fstream>
#include "System/ArgumentException.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/Xml/Linq/XDocument.hpp"
#include "System/Xml/Linq/XNamespace.hpp"
#include "System/Xml/Linq/XText.hpp"
#include "System/Xml/XmlException.hpp"
#include "System/Xml/XmlWriter.hpp"
#include "NamespaceScope.hpp"

namespace System::Xml::Linq {

    using System::Xml::XmlNodeType;

    // -----------------------------------------------------------------------------------
    // Namespace-aware serialization (ticket #2197, SR-AUD-334).
    //
    // Both serialization doors used to write `name_.getLocalNameProperty()`. The in-code
    // comments recorded that as a deliberate choice to avoid emitting XName's Clark notation
    // ("{uri}local") as an XML Name, which would have been literally unparseable. That
    // reasoning was right about Clark notation and wrong about the alternative: the
    // alternative is a PREFIX, not a bare local name. Measured, writing bare local names did
    // not merely lose the namespace, it produced malformed output -- two attributes differing
    // only by namespace serialized as `<r x="1" x="2"/>`, which this runtime's own parser
    // rejects with XML_ERROR_PARSING_ATTRIBUTE -- and it degraded an `xmlns:p` declaration
    // into an ordinary attribute named `p`, silently unbinding the prefix for a whole subtree.
    //
    // The scope is rebuilt from the tree on every entry point and cached nowhere. Caching it
    // would need a field on XObject, which is the object-layout approval #1896 was refused.
    // -----------------------------------------------------------------------------------

    void XElement::CollectInheritedScope(detail::NamespaceScope& scope) const {
        // getParentProperty() rather than parent_: it is public, it already returns nullptr for
        // a detached node and for a node whose parent is an XDocument, and #1890 made it safe
        // after an owner is destroyed. Nothing here dereferences a link it did not just read.
        std::vector<const XElement*> chain;
        for (const XElement* p = getParentProperty(); p != nullptr; p = p->getParentProperty())
            chain.push_back(p);
        for (auto it = chain.rbegin(); it != chain.rend(); ++it)
            (*it)->DeclareOwnNamespaces(scope);
    }

    void XElement::DeclareOwnNamespaces(detail::NamespaceScope& scope) const {
        for (const auto& a : attributes_) {
            if (!a || !a->getIsNamespaceDeclarationProperty()) continue;
            scope.Declare(detail::DeclaredPrefix(a->getNameProperty().getNamespaceNameProperty(),
                                                 a->getNameProperty().getLocalNameProperty()),
                          a->getValueProperty());
        }
    }

    void XElement::ResolveStartTag(detail::NamespaceScope& scope, std::string& qualifiedName,
                                   std::vector<std::pair<std::string, std::string>>& attributes) const {
        // Whether this element declares a default namespace of its own has to be known before
        // its declarations are folded into the scope: it decides whether an unqualified name
        // needs an xmlns="" undeclaration, and emitting both would put two `xmlns` attributes on
        // one start tag.
        bool declaresOwnDefault = false;
        for (const auto& a : attributes_) {
            if (a && a->getIsNamespaceDeclarationProperty() &&
                a->getNameProperty().getNamespaceNameProperty().empty()) {
                declaresOwnDefault = true;
                break;
            }
        }
        DeclareOwnNamespaces(scope);

        std::vector<std::pair<std::string, std::string>> generated;

        // The element's own name. An element name MAY take the default namespace, so the
        // lookup allows it; an unqualified element under an inherited default namespace has to
        // undeclare it with xmlns="" or it would silently acquire one on read-back.
        const std::string& elementUri = name_.getNamespaceNameProperty();
        std::string elementPrefix;
        if (elementUri.empty()) {
            // `declaresOwnDefault` guards the one self-contradictory tree this can be handed:
            // an unqualified element carrying its own non-empty xmlns="..." declaration. XML
            // has no way to express that -- the declaration governs the element's own name --
            // so the explicit declaration wins and no undeclaration is generated. Emitting both
            // produced `<e xmlns="" xmlns="urn:y"/>`, which is not well-formed.
            if (!declaresOwnDefault && !scope.DefaultUri().empty()) {
                generated.emplace_back(std::string(), std::string());
                scope.Declare(std::string(), std::string());
            }
        } else if (!scope.TryLookupUri(elementUri, /*allowDefault=*/true, elementPrefix)) {
            elementPrefix = scope.AllocatePrefix();
            generated.emplace_back(elementPrefix, elementUri);
            scope.Declare(elementPrefix, elementUri);
        }
        qualifiedName = detail::QualifiedName(elementPrefix, name_.getLocalNameProperty());

        // The attributes, in document order. A declaration is rendered AS a declaration -- that
        // is the half that used to corrupt the tree rather than merely lose fidelity.
        std::vector<std::pair<std::string, std::string>> rendered;
        rendered.reserve(attributes_.size());
        for (const auto& a : attributes_) {
            if (!a) continue;
            const XName& n = a->getNameProperty();
            if (a->getIsNamespaceDeclarationProperty()) {
                rendered.emplace_back(
                    detail::DeclarationAttributeName(detail::DeclaredPrefix(
                        n.getNamespaceNameProperty(), n.getLocalNameProperty())),
                    a->getValueProperty());
                continue;
            }
            const std::string& uri = n.getNamespaceNameProperty();
            if (uri.empty()) {
                rendered.emplace_back(n.getLocalNameProperty(), a->getValueProperty());
                continue;
            }
            // An attribute name never takes the default namespace, so a non-empty prefix is
            // required even when the default declaration already names this URI.
            std::string prefix;
            if (!scope.TryLookupUri(uri, /*allowDefault=*/false, prefix)) {
                prefix = scope.AllocatePrefix();
                generated.emplace_back(prefix, uri);
                scope.Declare(prefix, uri);
            }
            rendered.emplace_back(detail::QualifiedName(prefix, n.getLocalNameProperty()),
                                  a->getValueProperty());
        }

        attributes.clear();
        attributes.reserve(generated.size() + rendered.size());
        // Generated declarations first. Order inside one start tag is not significant -- every
        // declaration on a tag is in scope for that whole tag -- so this is purely for
        // readability and for a stable, testable rendering.
        for (const auto& g : generated)
            attributes.emplace_back(detail::DeclarationAttributeName(g.first), g.second);
        for (auto& r : rendered) attributes.push_back(std::move(r));
    }

    XNamespace XElement::GetDefaultNamespace() const {
        detail::NamespaceScope scope;
        CollectInheritedScope(scope);
        DeclareOwnNamespaces(scope);
        return XNamespace(scope.DefaultUri());
    }

    std::string XElement::GetPrefixOfNamespace(const XNamespace& ns) const {
        detail::NamespaceScope scope;
        CollectInheritedScope(scope);
        DeclareOwnNamespaces(scope);
        std::string prefix;
        if (scope.TryLookupUri(ns.getNamespaceNameProperty(), /*allowDefault=*/false, prefix))
            return prefix;
        return {};
    }

    void XElement::RelinkAttributes() {
        for (size_t i = 0; i < attributes_.size(); ++i) {
            attributes_[i]->setNextAttributeProperty(i + 1 < attributes_.size() ? attributes_[i + 1].get() : nullptr);
        }
    }

    void XElement::Add(std::shared_ptr<XAttribute> attr) {
        if (!attr) return;
        for (auto& a : attributes_) {
            if (a.get() != attr.get() && a->getNameProperty() == attr->getNameProperty()) {
                throw System::InvalidOperationException("Duplicate attribute: " + attr->getNameProperty().ToString());
            }
        }
        if (XElement* owner = attr->getParentProperty()) {
            owner->RemoveAttribute(attr.get());
        }
        AdoptObject(*attr, this);
        attributes_.push_back(std::move(attr));
        RelinkAttributes();
    }

    void XElement::Add(const std::string& text) {
        // Verified against XContainer.cs's AddString(): if the current last child is already a
        // plain XText (not XCData), real .NET appends into its existing Value rather than
        // creating a new sibling text node -- e.g. two consecutive Add(string) calls produce
        // one merged text node, not two adjacent ones. An empty string is a genuine no-op (no
        // node created at all), matching AddString's `if (s.Length > 0)` guard.
        if (text.empty()) return;
        if (!children_.empty()) {
            auto& last = children_.back();
            if (last->getNodeTypeProperty() == XmlNodeType::Text) {
                auto* lastText = static_cast<XText*>(last.get());
                lastText->setValueProperty(lastText->getValueProperty() + text);
                return;
            }
        }
        XContainer::Add(std::make_shared<XText>(text));
    }

    bool XElement::getHasElementsProperty() const {
        return std::any_of(children_.begin(), children_.end(),
                            [](const std::shared_ptr<XNode>& n) { return n->getNodeTypeProperty() == XmlNodeType::Element; });
    }

    void XElement::RemoveAttributes() {
        for (auto& a : attributes_) {
            AdoptObject(*a, nullptr);
            a->setNextAttributeProperty(nullptr);
        }
        attributes_.clear();
    }

    void XElement::RemoveAttribute(XAttribute* attr) {
        auto it = std::find_if(attributes_.begin(), attributes_.end(),
                                [attr](const std::shared_ptr<XAttribute>& a) { return a.get() == attr; });
        if (it == attributes_.end()) return;
        AdoptObject(**it, nullptr);
        (*it)->setNextAttributeProperty(nullptr);
        attributes_.erase(it);
        RelinkAttributes();
    }

    void XElement::RemoveAll() {
        RemoveAttributes();
        RemoveNodes();
    }

    void XElement::AppendTextValue(std::string& sb) const {
        for (auto& child : children_) {
            auto nt = child->getNodeTypeProperty();
            if (nt == XmlNodeType::Text || nt == XmlNodeType::CDATA) {
                sb += static_cast<const XText*>(child.get())->getValueProperty();
            } else if (nt == XmlNodeType::Element) {
                static_cast<const XElement*>(child.get())->AppendTextValue(sb);
            }
        }
    }

    std::string XElement::getValueProperty() const {
        std::string sb;
        AppendTextValue(sb);
        return sb;
    }

    void XElement::setValueProperty(const std::string& v) {
        RemoveNodes();
        if (!v.empty()) XContainer::Add(std::make_shared<XText>(v));
    }

    void XElement::WriteTo(System::Xml::XmlWriter& writer) const {
        detail::NamespaceScope scope;
        CollectInheritedScope(scope);
        WriteElementTo(writer, std::move(scope));
    }

    void XElement::WriteElementTo(System::Xml::XmlWriter& writer, detail::NamespaceScope scope) const {
        // The historical comment here recorded that XName::ToString()'s Clark notation
        // ("{uri}local") must never be written as an XML Name, because '{' and '}' are not
        // legal in the Name production. That is still true, and it is still not what happens.
        // What changed with #2197 is the alternative: a qualified name plus a declaration,
        // instead of a bare local name. XmlConvert::VerifyName -- which this writer already
        // applies to every name -- accepts a ':', so no writer surface had to change.
        std::string qualifiedName;
        std::vector<std::pair<std::string, std::string>> attributes;
        ResolveStartTag(scope, qualifiedName, attributes);

        writer.WriteStartElement(qualifiedName);
        for (const auto& a : attributes) writer.WriteAttributeString(a.first, a.second);
        for (const auto& c : children_) {
            if (!c) continue;
            if (c->getNodeTypeProperty() == XmlNodeType::Element) {
                static_cast<const XElement*>(c.get())->WriteElementTo(writer, scope);
            } else {
                c->WriteTo(writer);
            }
        }
        writer.WriteEndElement();
    }

    void XElement::Save(const std::string& fileName, SaveOptions options) const {
        std::ofstream ofs(fileName, std::ios::out | std::ios::trunc);
        if (!ofs) throw System::Xml::XmlException("XElement::Save: failed to open '" + fileName + "'.");
        bool indent = (options & SaveOptions::DisableFormatting) == SaveOptions::None;
        SerializeTo(ofs, 0, indent);
    }

    void XElement::Save(System::Xml::XmlWriter& writer) const {
        WriteTo(writer);
    }

    std::shared_ptr<XElement> XElement::Parse(const std::string& xml, LoadOptions options) {
        auto doc = XDocument::Parse(xml, options);
        auto root = doc->getRootProperty();
        if (root) root->Remove(); // detach from the temporary XDocument (see XContainer's doc-comment on lifetime)
        return root;
    }

    std::shared_ptr<XElement> XElement::Load(const std::string& filePath, LoadOptions options) {
        auto doc = XDocument::Load(filePath, options);
        auto root = doc->getRootProperty();
        if (root) root->Remove();
        return root;
    }

    void XElement::SerializeTo(std::ostream& os, int depth, bool indent) const {
        detail::NamespaceScope scope;
        CollectInheritedScope(scope);
        SerializeElementTo(os, depth, indent, std::move(scope));
    }

    void XElement::SerializeElementTo(std::ostream& os, int depth, bool indent,
                                      detail::NamespaceScope scope) const {
        std::string qualifiedName;
        std::vector<std::pair<std::string, std::string>> attributes;
        ResolveStartTag(scope, qualifiedName, attributes);

        // Ticket #2201. Guarded here rather than in the escapers, so the diagnostic can name
        // which part of the start tag carried the NUL. This is the NUL clause ONLY: it does not
        // impose the XML name grammar on an element or attribute name at this door, which stays
        // as permissive as it has always been -- a wider accepted-input question than #2201.
        detail::ThrowIfContainsNul(qualifiedName, "XElement::SerializeTo", "element name");
        for (const auto& a : attributes) {
            detail::ThrowIfContainsNul(a.first, "XElement::SerializeTo", "attribute name");
            detail::ThrowIfContainsNul(a.second, "XElement::SerializeTo", "attribute value");
        }

        std::string pad = indent ? std::string(static_cast<size_t>(depth) * 2, ' ') : "";
        os << pad << "<" << qualifiedName;
        for (const auto& a : attributes)
            // XAttribute::EscapeValue, not XNode::EscapeAttributeValue: the former also emits
            // character references for tab/LF/CR, without which attribute-value normalization
            // collapses them to spaces on reload. XElement is a friend of XAttribute, so the
            // one escaper both doors already used stays the one escaper.
            os << " " << a.first << "=\"" << XAttribute::EscapeValue(a.second) << "\"";
        if (children_.empty()) {
            os << "/>";
            return;
        }
        os << ">";
        bool multiline = indent && std::any_of(children_.begin(), children_.end(), [](const std::shared_ptr<XNode>& c) {
            auto nt = c->getNodeTypeProperty();
            return nt != XmlNodeType::Text && nt != XmlNodeType::CDATA;
        });
        for (auto& c : children_) {
            auto nt = c->getNodeTypeProperty();
            bool isText = (nt == XmlNodeType::Text || nt == XmlNodeType::CDATA);
            if (multiline && !isText) os << "\n";
            if (nt == XmlNodeType::Element) {
                static_cast<const XElement*>(c.get())->SerializeElementTo(os, depth + 1, indent, scope);
            } else {
                c->SerializeTo(os, depth + 1, indent);
            }
        }
        if (multiline) os << "\n" << pad;
        os << "</" << qualifiedName << ">";
    }

    SharpRuntime::intcs XElement::GetDeepHashCode() const {
        SharpRuntime::intcs h = static_cast<SharpRuntime::intcs>(std::hash<std::string>{}(name_.ToString()));
        for (auto& a : attributes_) {
            h ^= static_cast<SharpRuntime::intcs>(std::hash<std::string>{}(a->getNameProperty().ToString()));
            h ^= static_cast<SharpRuntime::intcs>(std::hash<std::string>{}(a->getValueProperty()));
        }
        for (auto& c : children_) {
            auto nt = c->getNodeTypeProperty();
            if (nt == XmlNodeType::Comment || nt == XmlNodeType::ProcessingInstruction) continue;
            h ^= c->GetDeepHashCode();
        }
        return h;
    }

    void XElement::ValidateNode(const XNode& node) const {
        // Verified against XElement.cs's ValidateNode: an XDocument or XDocumentType can never
        // be a legal child of an element (only of an XDocument, which enforces its own,
        // separate single-root/single-doctype rules) -- without this check, tinyxml2's own
        // insertion has no such restriction and would silently produce a structurally invalid
        // tree (a nested document, or a doctype declaration inside element content).
        auto nt = node.getNodeTypeProperty();
        if (nt == XmlNodeType::Document)
            throw System::ArgumentException("This operation would create an incorrectly structured document (cannot add an XDocument as a child of an XElement).");
        if (nt == XmlNodeType::DocumentType)
            throw System::ArgumentException("This operation would create an incorrectly structured document (cannot add an XDocumentType as a child of an XElement).");
    }

    bool XElement::DeepEqualsCore(const XNode& other) const {
        const auto& o = static_cast<const XElement&>(other);
        if (name_ != o.name_) return false;
        // Verified against XElement.cs's AttributesEqual: real .NET walks both attribute lists
        // in parallel by *position*, not by name lookup -- two elements with the same
        // attributes in a different order are NOT deep-equal (attribute order is part of an
        // XElement's identity for DeepEquals purposes, even though lookup-by-name is
        // order-independent).
        if (attributes_.size() != o.attributes_.size()) return false;
        for (size_t i = 0; i < attributes_.size(); ++i) {
            if (attributes_[i]->getNameProperty() != o.attributes_[i]->getNameProperty() ||
                attributes_[i]->getValueProperty() != o.attributes_[i]->getValueProperty())
                return false;
        }
        // Ignore comments/processing instructions on both sides; pairwise-compare the rest.
        std::vector<const XNode*> lhs, rhs;
        for (auto& c : children_) {
            auto nt = c->getNodeTypeProperty();
            if (nt != XmlNodeType::Comment && nt != XmlNodeType::ProcessingInstruction) lhs.push_back(c.get());
        }
        for (auto& c : o.children_) {
            auto nt = c->getNodeTypeProperty();
            if (nt != XmlNodeType::Comment && nt != XmlNodeType::ProcessingInstruction) rhs.push_back(c.get());
        }
        if (lhs.size() != rhs.size()) return false;
        for (size_t i = 0; i < lhs.size(); ++i) {
            if (!XNode::DeepEquals(lhs[i], rhs[i])) return false;
        }
        return true;
    }

} // namespace System::Xml::Linq
