// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Xml/XmlNode.hpp"

#include <tinyxml2/tinyxml2.h>

#include <memory>
#include <vector>

#include "System/ArgumentException.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/Xml/XPath/XPathNodeIterator.hpp"
#include "System/Xml/XPath/XmlDocumentNavigator.hpp"
#include "System/Xml/IHasXmlNode.hpp"
#include "System/Xml/XmlAttribute.hpp"
#include "System/Xml/XmlDocument.hpp"
#include "System/Xml/XmlDocumentFragment.hpp"
#include "System/Xml/XmlElement.hpp"
#include "System/Xml/XmlException.hpp"
#include "XmlNodeChangeEvents.hpp"
#include "System/Xml/XmlNodeList.hpp"
#include "System/Xml/XmlWriter.hpp"

namespace System::Xml {

    namespace {
        // Verified against XmlNode.cs's InsertBefore/InsertAfter/AppendChild: real .NET throws
        // ArgumentException("Cannot insert a node or any ancestor of that node as a child of
        // itself.") when the node being inserted is `this` itself or one of this's ancestors --
        // otherwise the reparent creates a cycle. tinyxml2 performs no such check itself: its
        // InsertChildPreamble() unconditionally unlinks the node from its current parent and
        // reattaches it, so inserting an ancestor produces a genuine two-node parent/child cycle
        // (the node ends up as both its own descendant and its own ancestor).
        void ThrowIfSelfOrAncestor(const tinyxml2::XMLNode* thisNative, const tinyxml2::XMLNode* candidate) {
            for (const tinyxml2::XMLNode* n = thisNative; n; n = n->Parent()) {
                if (n == candidate)
                    throw System::ArgumentException("Cannot insert a node or any ancestor of that node as a child of itself.");
            }
        }

        // Verified against XmlNode.cs's InsertBefore/InsertAfter/AppendChild: real .NET throws
        // ArgumentException("The node to be inserted is from a different document context.")
        // rather than silently doing nothing. tinyxml2's InsertEndChild/InsertFirstChild/
        // InsertAfterChild already refuse a cross-document insert internally (comparing
        // XMLNode::_document), but they do so by silently returning nullptr -- this wrapper
        // never checked that return value, so a cross-document insert previously reported
        // success (returning newChild unchanged) while actually doing nothing.
        void ThrowIfDifferentDocument(const XmlNode* thisNode, const XmlDocument* thisDoc, const XmlDocument* childDoc) {
            if (childDoc && childDoc != thisDoc && static_cast<const void*>(childDoc) != static_cast<const void*>(thisNode))
                throw System::ArgumentException("The node to be inserted is from a different document context.");
        }

        // Ticket #2075 (SR-AUD-351, cause X-B). The port already validated DOCUMENT identity
        // (ThrowIfDifferentDocument) and ANCESTRY (ThrowIfSelfOrAncestor) but never
        // PARENTHOOD, so a same-document node belonging to a DIFFERENT parent passed every
        // check. Measured before this guard, with root -> {a, b} and b -> {childOfB}:
        //
        //   a->RemoveChild(childOfB)      -> B's child was detached
        //   a->InsertBefore(n, childOfB)  -> refChild silently ignored, n landed inside a
        //   a->InsertAfter(n, childOfB)   -> n ended up NOWHERE
        //   a->ReplaceChild(n, childOfB)  -> both wrongs at once
        //   root->RemoveChild(orphan)     -> a parentless node accepted silently
        //
        // Three different failure modes from one missing check. The wording follows this
        // file's existing ArgumentException messages; .NET's exact text is not verifiable here
        // (/rv/tmp/runtime/ absent) and is recorded as this port's choice
        // (docs/SystemXmlNamespaceReviewPlan.md §4.2).
        void ThrowIfNotChildOf(const tinyxml2::XMLNode* thisNative,
                               const tinyxml2::XMLNode* candidate,
                               const char* message) {
            if (!candidate || candidate->Parent() != thisNative)
                throw System::ArgumentException(message);
        }

        // Verified against XmlNode.cs's Normalize(): real .NET recurses into the full depth of
        // the sub-tree (`case XmlNodeType.Element: crtChild.Normalize(); goto default;`), not
        // just direct children -- this port previously only merged adjacent text nodes among
        // `this`'s immediate children, silently leaving un-merged runs inside any descendant
        // element untouched.
        void NormalizeNative(tinyxml2::XMLNode* native, XmlDocument* doc) {
            tinyxml2::XMLNode* child = native->FirstChild();
            while (child) {
                tinyxml2::XMLNode* next = child->NextSibling();
                if (child->ToElement()) {
                    NormalizeNative(child, doc);
                    child = next;
                    continue;
                }
                auto* text = child->ToText();
                if (text && !text->CData() && next) {
                    auto* nextText = next->ToText();
                    if (nextText && !nextText->CData()) {
                        std::string merged = (text->Value() ? text->Value() : std::string());
                        merged += (nextText->Value() ? nextText->Value() : std::string());
                        text->SetValue(merged.c_str());
                        if (doc) doc->PurgeCache(next);
                        native->DeleteChild(next); // genuinely discarded (merged away), not detached for reuse
                        continue; // re-check this node against its new next sibling
                    }
                }
                child = next;
            }
        }
    }

    XmlNode::~XmlNode() = default;

    std::string XmlNode::getNameProperty() const {
        return native_ && native_->Value() ? native_->Value() : "";
    }

    std::string XmlNode::getLocalNameProperty() const {
        std::string name = getNameProperty();
        size_t colon = name.find(':');
        return colon == std::string::npos ? name : name.substr(colon + 1);
    }

    std::string XmlNode::getPrefixProperty() const {
        std::string name = getNameProperty();
        size_t colon = name.find(':');
        return colon == std::string::npos ? "" : name.substr(0, colon);
    }

    std::string XmlNode::getNamespaceURIProperty() const {
        std::string prefix = getPrefixProperty();
        std::string attrName = prefix.empty() ? "xmlns" : "xmlns:" + prefix;
        // Walk up through ancestor elements looking for the nearest xmlns/xmlns:prefix declaration.
        for (tinyxml2::XMLNode* n = native_; n; n = n->Parent()) {
            if (auto* el = n->ToElement()) {
                const char* v = el->Attribute(attrName.c_str());
                if (v) return v;
            }
        }
        return {};
    }

    std::string XmlNode::getValueProperty() const { return {}; }

    void XmlNode::setValueProperty(const std::string&) {
        throw System::InvalidOperationException("Value setting is not implemented on node type " +
                                                 std::to_string(static_cast<int>(getNodeTypeProperty())) + ".");
    }

    std::string XmlNode::getInnerTextProperty() const {
        std::string result;
        if (!native_) return result;
        XmlDocument* doc = GetDocument();
        for (tinyxml2::XMLNode* child = native_->FirstChild(); child; child = child->NextSibling()) {
            if (auto* txt = child->ToText()) {
                result += txt->Value() ? txt->Value() : "";
            } else if (child->ToElement()) {
                if (auto* wrapped = doc ? doc->WrapNode(child) : nullptr)
                    result += wrapped->getInnerTextProperty();
            }
        }
        return result;
    }

    void XmlNode::setInnerTextProperty(const std::string& text) {
        RemoveAllChildren();
        XmlDocument* doc = GetDocument();
        if (!text.empty() && doc) AppendChild(doc->CreateTextNode(text));
    }

    std::string XmlNode::getInnerXmlProperty() const {
        if (!native_) return {};
        // compact=true: real .NET's InnerXml serializes exact markup with no inserted
        // whitespace between nodes. tinyxml2::XMLPrinter defaults to compact=false (pretty-
        // printed with inserted newlines/indentation), which this port previously left
        // unchanged, silently reformatting the tree's own markup instead of reproducing it.
        tinyxml2::XMLPrinter printer(nullptr, /*compact=*/true);
        for (tinyxml2::XMLNode* child = native_->FirstChild(); child; child = child->NextSibling())
            child->Accept(&printer);
        return printer.CStr() ? printer.CStr() : "";
    }

    // Ticket #2074 (SR-AUD-350, cause X-A). The destructive step used to run FIRST and the
    // parse's status was discarded, so an invalid fragment emptied the node and returned
    // normally:
    //
    //     element with <keep/>  ->  setInnerXml("<bad>")  ->  innerXml "" , no exception
    //
    // That is silent, total content loss on public input. The correct ordering was already one
    // call away in this same module -- XmlDocument::LoadXml parses the same text through the
    // same substrate and throws a shaped XmlException -- so this is an ordering change plus an
    // existing error path, not a new policy (docs/SystemXmlNamespaceReviewPlan.md §4.1).
    //
    // Parse into the scratch document FIRST; on failure throw and leave the node exactly as it
    // was; only then remove the old children and clone the new ones in. The clone loop is the
    // last step and cannot fail in a way that leaves the node half-populated: every child is
    // cloned into the owning document before any of them is inserted.
    void XmlNode::setInnerXmlProperty(const std::string& xml) {
        XmlDocument* doc = GetDocument();
        if (!doc) return;
        if (xml.empty()) { RemoveAllChildren(); return; }

        tinyxml2::XMLDocument fragmentDoc;
        std::string wrapped = "<root>" + xml + "</root>";
        if (fragmentDoc.Parse(wrapped.c_str()) != tinyxml2::XML_SUCCESS)
            throw XmlException(std::string("XmlNode::InnerXml: parse error: ") +
                               (fragmentDoc.ErrorStr() ? fragmentDoc.ErrorStr() : "unknown error"));
        auto* root = fragmentDoc.RootElement();
        if (!root)
            throw XmlException("XmlNode::InnerXml: parse error: the fragment has no content.");

        // Clone every child before touching this node, so a failure inside DeepClone cannot
        // leave the node stripped of its old content and missing its new content too.
        std::vector<tinyxml2::XMLNode*> cloned;
        for (tinyxml2::XMLNode* child = root->FirstChild(); child; child = child->NextSibling())
            cloned.push_back(child->DeepClone(&doc->getNativeDocument()));

        RemoveAllChildren();
        // Ticket #2079: this loop inserts natively rather than through AppendChild, so it
        // needs its own dispatch or setInnerXml would be the one insert door that stayed
        // silent while setInnerText (which routes through AppendChild) fired. The removal
        // half is silent here for the same lifetime reason RemoveAllChildren records.
        XmlDocument* eventDoc = GetDocument();
        for (tinyxml2::XMLNode* child : cloned) {
            // Named for the node it wraps rather than reusing `wrapped`, which is already the
            // wrapped *markup* string earlier in this function -- two unrelated meanings for one
            // name in one body, and what MSVC /W4 reports as C4456.
            XmlNode* wrappedChild = eventDoc ? eventDoc->WrapNode(child) : nullptr;
            detail::RaiseNodeChanging(eventDoc, wrappedChild, nullptr, this, "", "",
                                      XmlNodeChangedAction::Insert);
            native_->InsertEndChild(child);
            detail::RaiseNodeChanged(eventDoc, wrappedChild, nullptr, this, "", "",
                                     XmlNodeChangedAction::Insert);
        }
    }

    std::string XmlNode::getOuterXmlProperty() const {
        if (!native_) return {};
        // See getInnerXmlProperty()'s comment: compact=true matches real .NET's OuterXml
        // producing exact markup with no inserted whitespace.
        tinyxml2::XMLPrinter printer(nullptr, /*compact=*/true);
        native_->Accept(&printer);
        return printer.CStr() ? printer.CStr() : "";
    }

    XmlNode* XmlNode::getParentNodeProperty() const {
        XmlDocument* doc = GetDocument();
        if (!native_ || !doc) return nullptr;
        auto* parent = native_->Parent();
        if (doc->IsDetached(parent)) return nullptr;
        return doc->WrapNode(parent);
    }

    XmlNode* XmlNode::getFirstChildProperty() const {
        XmlDocument* doc = GetDocument();
        return native_ && doc ? doc->WrapNode(native_->FirstChild()) : nullptr;
    }

    XmlNode* XmlNode::getLastChildProperty() const {
        XmlDocument* doc = GetDocument();
        return native_ && doc ? doc->WrapNode(native_->LastChild()) : nullptr;
    }

    XmlNode* XmlNode::getPreviousSiblingProperty() const {
        XmlDocument* doc = GetDocument();
        return native_ && doc ? doc->WrapNode(native_->PreviousSibling()) : nullptr;
    }

    XmlNode* XmlNode::getNextSiblingProperty() const {
        XmlDocument* doc = GetDocument();
        return native_ && doc ? doc->WrapNode(native_->NextSibling()) : nullptr;
    }

    XmlNodeList* XmlNode::getChildNodesProperty() const {
        std::vector<XmlNode*> children;
        XmlDocument* doc = GetDocument();
        if (native_ && doc)
            for (tinyxml2::XMLNode* child = native_->FirstChild(); child; child = child->NextSibling())
                children.push_back(doc->WrapNode(child));
        childNodesSnapshot_ = std::make_unique<XmlNodeList>(std::move(children));
        return childNodesSnapshot_.get();
    }

    bool XmlNode::getHasChildNodesProperty() const {
        return native_ && native_->FirstChild() != nullptr;
    }

    XmlNode* XmlNode::PrependChild(XmlNode* newChild) {
        if (!native_ || !newChild) return newChild;
        ThrowIfSelfOrAncestor(native_, newChild->native_);
        ThrowIfDifferentDocument(this, GetDocument(), newChild->GetDocument());
        if (auto* frag = dynamic_cast<XmlDocumentFragment*>(newChild)) {
            XmlNode* firstInserted = nullptr;
            XmlNode* child = frag->getLastChildProperty();
            while (child) {
                XmlNode* prev = child->getPreviousSiblingProperty();
                ThrowIfSelfOrAncestor(native_, child->native_);
                frag->RemoveChild(child);
                detail::RaiseNodeChanging(GetDocument(), child, frag, this, "", "",
                                          XmlNodeChangedAction::Insert);
                native_->InsertFirstChild(child->native_);
                detail::RaiseNodeChanged(GetDocument(), child, frag, this, "", "",
                                         XmlNodeChangedAction::Insert);
                firstInserted = child;
                child = prev;
            }
            return firstInserted;
        }
        XmlDocument* doc = GetDocument();
        XmlNode* oldParent = newChild->getParentNodeProperty();
        detail::RaiseNodeChanging(doc, newChild, oldParent, this, "", "",
                                  XmlNodeChangedAction::Insert);
        native_->InsertFirstChild(newChild->native_);
        detail::RaiseNodeChanged(doc, newChild, oldParent, this, "", "",
                                 XmlNodeChangedAction::Insert);
        return newChild;
    }

    XmlNode* XmlNode::AppendChild(XmlNode* newChild) {
        if (!native_ || !newChild) return newChild;
        ThrowIfSelfOrAncestor(native_, newChild->native_);
        ThrowIfDifferentDocument(this, GetDocument(), newChild->GetDocument());
        if (auto* frag = dynamic_cast<XmlDocumentFragment*>(newChild)) {
            XmlNode* lastInserted = nullptr;
            XmlNode* child = frag->getFirstChildProperty();
            while (child) {
                XmlNode* next = child->getNextSiblingProperty();
                ThrowIfSelfOrAncestor(native_, child->native_);
                frag->RemoveChild(child);
                detail::RaiseNodeChanging(GetDocument(), child, frag, this, "", "",
                                          XmlNodeChangedAction::Insert);
                native_->InsertEndChild(child->native_);
                detail::RaiseNodeChanged(GetDocument(), child, frag, this, "", "",
                                         XmlNodeChangedAction::Insert);
                lastInserted = child;
                child = next;
            }
            return lastInserted;
        }
        XmlDocument* doc = GetDocument();
        XmlNode* oldParent = newChild->getParentNodeProperty();
        detail::RaiseNodeChanging(doc, newChild, oldParent, this, "", "",
                                  XmlNodeChangedAction::Insert);
        native_->InsertEndChild(newChild->native_);
        detail::RaiseNodeChanged(doc, newChild, oldParent, this, "", "",
                                 XmlNodeChangedAction::Insert);
        return newChild;
    }

    XmlNode* XmlNode::InsertBefore(XmlNode* newChild, XmlNode* refChild) {
        if (!refChild) return AppendChild(newChild);
        if (!native_ || !newChild) return newChild;
        ThrowIfSelfOrAncestor(native_, newChild->native_);
        ThrowIfDifferentDocument(this, GetDocument(), newChild->GetDocument());
        ThrowIfNotChildOf(native_, refChild->native_,
                          "The reference node is not a child of this node.");
        XmlDocument* doc = GetDocument();
        XmlNode* oldParent = newChild->getParentNodeProperty();
        tinyxml2::XMLNode* prevSibling = refChild->native_ ? refChild->native_->PreviousSibling() : nullptr;
        detail::RaiseNodeChanging(doc, newChild, oldParent, this, "", "",
                                  XmlNodeChangedAction::Insert);
        if (!prevSibling) native_->InsertFirstChild(newChild->native_);
        else               native_->InsertAfterChild(prevSibling, newChild->native_);
        detail::RaiseNodeChanged(doc, newChild, oldParent, this, "", "",
                                 XmlNodeChangedAction::Insert);
        return newChild;
    }

    XmlNode* XmlNode::InsertAfter(XmlNode* newChild, XmlNode* refChild) {
        if (!refChild) return PrependChild(newChild);
        if (!native_ || !newChild) return newChild;
        ThrowIfSelfOrAncestor(native_, newChild->native_);
        ThrowIfDifferentDocument(this, GetDocument(), newChild->GetDocument());
        ThrowIfNotChildOf(native_, refChild->native_,
                          "The reference node is not a child of this node.");
        XmlDocument* doc = GetDocument();
        XmlNode* oldParent = newChild->getParentNodeProperty();
        detail::RaiseNodeChanging(doc, newChild, oldParent, this, "", "",
                                  XmlNodeChangedAction::Insert);
        native_->InsertAfterChild(refChild->native_, newChild->native_);
        detail::RaiseNodeChanged(doc, newChild, oldParent, this, "", "",
                                 XmlNodeChangedAction::Insert);
        return newChild;
    }

    XmlNode* XmlNode::RemoveChild(XmlNode* oldChild) {
        XmlDocument* doc = GetDocument();
        if (!native_ || !oldChild || !oldChild->native_ || !doc) return oldChild;
        ThrowIfNotChildOf(native_, oldChild->native_,
                          "The node to be removed is not a child of this node.");
        // Safe to report AFTER the mutation as well: DetachNode moves the node under the
        // document's scratch parent rather than destroying it, so the wrapper the handler
        // receives is still alive. RemoveAllChildren, which DOES destroy its children,
        // deliberately raises nothing -- see its own comment.
        detail::RaiseNodeChanging(doc, oldChild, this, nullptr, "", "",
                                  XmlNodeChangedAction::Remove);
        doc->DetachNode(oldChild->native_);
        detail::RaiseNodeChanged(doc, oldChild, this, nullptr, "", "",
                                 XmlNodeChangedAction::Remove);
        return oldChild;
    }

    // The parenthood check must run BEFORE InsertBefore inserts anything: otherwise a foreign
    // oldChild would leave newChild already spliced into this node when the removal threw --
    // the very partial state this repair exists to prevent (#2075).
    XmlNode* XmlNode::ReplaceChild(XmlNode* newChild, XmlNode* oldChild) {
        if (native_ && oldChild)
            ThrowIfNotChildOf(native_, oldChild->native_,
                              "The node to be replaced is not a child of this node.");
        InsertBefore(newChild, oldChild);
        RemoveChild(oldChild);
        return oldChild;
    }

    /**
     * @brief Removes every child, raising the node-change pair for each. Ticket #2086.
     *
     * #2079 left this door silent, and #2086 recorded two candidate approaches with neither
     * selected: (a) raise `NodeRemoving` per child and `NodeRemoved` never, rejected as
     * misleading; (b) detach each child instead of destroying it, *"NOT compatible without
     * approval"* because it changes lifetime semantics.
     *
     * **The reference selects (b), so it is derived rather than chosen.** `XmlNode.RemoveAll`
     * is literally a loop over `RemoveChild`:
     *
     * @code
     * public virtual void RemoveAll()
     * {
     *     XmlNode? child = FirstChild;
     *     XmlNode? sibling;
     *     while (child != null)
     *     {
     *         sibling = child.NextSibling;
     *         RemoveChild(child);
     *         child = sibling;
     *     }
     * }
     * @endcode
     *
     * So every child gets the SYMMETRIC pair, and every removed child stays alive as an orphan
     * — which is .NET's lifetime for a removed node too. This port's `RemoveChild` already
     * detaches rather than destroying (`XmlDocument::DetachNode` moves the node under a scratch
     * parent), so the borrowed-pointer hazard #2079 refused to introduce simply does not arise
     * on this route: the wrapper a handler receives names a live object.
     *
     * The sibling is captured BEFORE the removal, exactly as .NET does, because detaching
     * re-parents the child and its `NextSibling()` then walks the holder's list instead of this
     * node's.
     *
     * @par The cost, stated
     * Detached children live until the document does. That is not a new policy — every
     * `RemoveChild` in this port has behaved that way since the detached holder was introduced —
     * but this door is reached by the `InnerText`/`InnerXml` setters, so repeatedly reassigning
     * them now accumulates orphans instead of freeing them. It is the same trade .NET makes,
     * minus the garbage collector.
     */
    void XmlNode::RemoveAllChildren() {
        XmlDocument* doc = GetDocument();
        if (!native_ || !doc) return;
        tinyxml2::XMLNode* child = native_->FirstChild();
        while (child != nullptr) {
            tinyxml2::XMLNode* sibling = child->NextSibling();
            RemoveChild(doc->WrapNode(child));
            child = sibling;
        }
    }

    void XmlNode::RemoveAll() {
        RemoveAllChildren();
    }

    XmlNode* XmlNode::CloneNode(bool deep) const {
        XmlDocument* doc = GetDocument();
        if (!native_ || !doc) return nullptr;
        tinyxml2::XMLNode* cloned = deep
            ? native_->DeepClone(&doc->getNativeDocument())
            : native_->ShallowClone(&doc->getNativeDocument());
        return doc->WrapNode(cloned);
    }

    void XmlNode::Normalize() {
        if (!native_) return;
        NormalizeNative(native_, GetDocument());
    }

    XmlElement* XmlNode::Item(const std::string& name) const {
        XmlDocument* doc = GetDocument();
        if (!native_ || !doc) return nullptr;
        for (tinyxml2::XMLNode* child = native_->FirstChild(); child; child = child->NextSibling())
            if (auto* el = child->ToElement())
                if (el->Name() && name == el->Name())
                    return static_cast<XmlElement*>(doc->WrapNode(child));
        return nullptr;
    }

    bool XmlNode::Supports(const std::string&, const std::string&) const { return false; }

    void XmlNode::WriteTo(XmlWriter& writer) const {
        WriteContentTo(writer);
    }

    void XmlNode::WriteContentTo(XmlWriter& writer) const {
        XmlDocument* doc = GetDocument();
        if (!native_ || !doc) return;
        for (tinyxml2::XMLNode* child = native_->FirstChild(); child; child = child->NextSibling())
            if (auto* wrapped = doc->WrapNode(child))
                wrapped->WriteTo(writer);
    }

    XPath::XPathNavigator* XmlNode::CreateNavigator() const {
        XmlDocument* doc = GetDocument();
        if (getNodeTypeProperty() == XmlNodeType::Attribute) {
            // Attributes are not part of the tinyxml2 node tree, so XmlDocumentNavigator tracks
            // them as (owning element, attribute identity) rather than a native_-bearing node;
            // re-locate this attribute from its owner so the navigator lands in the Attribute
            // position instead of misreporting it as a bare (parentless) content node.
            const auto* attr = static_cast<const XmlAttribute*>(this);
            XmlElement* owner = attr->getOwnerElementProperty();
            if (!owner)
                throw System::InvalidOperationException("Cannot create a navigator for a detached attribute.");
            auto* nav = new XPath::XmlDocumentNavigator(doc, owner);
            if (nav->MoveToAttribute(attr->getLocalNameProperty(), attr->getNamespaceURIProperty())) return nav;
            delete nav;
            throw System::InvalidOperationException("Cannot create a navigator for a detached attribute.");
        }
        return new XPath::XmlDocumentNavigator(doc, const_cast<XmlNode*>(this));
    }

    XmlNode* XmlNode::SelectSingleNode(const std::string& xpath) const {
        std::unique_ptr<XPath::XPathNavigator> nav(CreateNavigator());
        std::unique_ptr<XPath::XPathNavigator> result(nav->SelectSingleNode(xpath));
        if (!result) return nullptr;
        const auto* hasNode = dynamic_cast<const IHasXmlNode*>(result.get());
        return hasNode ? hasNode->GetNode() : nullptr;
    }

    XmlNodeList* XmlNode::SelectNodes(const std::string& xpath) const {
        std::unique_ptr<XPath::XPathNavigator> nav(CreateNavigator());
        std::unique_ptr<XPath::XPathNodeIterator> it(nav->Select(xpath));
        std::vector<XmlNode*> nodes;
        while (it->MoveNext()) {
            const auto* hasNode = dynamic_cast<const IHasXmlNode*>(it->getCurrentProperty());
            if (hasNode && hasNode->GetNode()) nodes.push_back(hasNode->GetNode());
        }
        return new XmlNodeList(std::move(nodes));
    }

} // namespace System::Xml
