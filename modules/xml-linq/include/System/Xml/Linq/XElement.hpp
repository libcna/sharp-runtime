// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "System/Xml/Linq/LoadOptions.hpp"
#include "System/Xml/Linq/SaveOptions.hpp"
#include "System/Xml/Linq/XAttribute.hpp"
#include "System/Xml/Linq/XContainer.hpp"
#include "System/Xml/Linq/XName.hpp"

namespace System::Xml {
    class XmlWriter;
}

namespace System::Xml::Linq {

    class XNamespace;

    namespace detail {
        class NamespaceScope;
    }


    /**
     * @brief Represents an XML element with a name, attributes, and content (a mix of child
     * elements, text, CDATA, comments, and processing instructions).
     *
     * C++ counterpart of .NET System.Xml.Linq.XElement.
     */
    class XElement : public XContainer {
        XName name_;
        std::vector<std::shared_ptr<XAttribute>> attributes_;

        void RelinkAttributes();

    public:
        /** Constructs an empty element with the given XName (plain strings convert implicitly via XName). */
        explicit XElement(const XName& name) : name_(name) {}

        /** Constructs an element with an XName and an initial text value (a single XText child; empty strings add no child). */
        XElement(const XName& name, const std::string& value) : name_(name) {
            if (!value.empty()) setValueProperty(value);
        }

        /**
         * @brief Destroys this element, first clearing the parent link **and** the next-sibling
         * link of every attribute it still owns.
         *
         * Attributes are held by `std::shared_ptr`, so an attribute a caller retained outlives
         * this element. `XAttribute` carries **two** unguarded borrowed links, not one: the
         * inherited `XObject::parent_` (whose dereference is a virtual call, so a retained
         * attribute's `getParentProperty()`, `getPreviousAttributeProperty()` and `Remove()` all
         * aborted the process), and the intrusive `next_` sibling pointer, which dangles
         * independently — a retained attribute returned a freed sibling from
         * `getNextAttributeProperty()` and reading its name silently succeeded. Both are cleared
         * here, matching what `RemoveAttributes()`/`RemoveAttribute()` already do.
         *
         * Only attributes whose parent link still names *this* element are touched, and the link
         * is only ever compared, never dereferenced. The loop runs before `attributes_` is
         * released, so every attribute is still fully alive; `~XContainer` then runs and clears
         * the child-node side. Non-throwing and allocation-free, so it is safe during exception
         * unwinding and after a partially constructed derived object.
         */
        ~XElement() override {
            for (const auto& attr : attributes_) {
                if (!attr) continue;
                // XElement is a friend of XAttribute, so both links are reachable directly. That
                // deliberately avoids depending on the public setNextAttributeProperty(), which is
                // itself proposed for removal by ticket #1892.
                if (attr->parent_ != this) continue;
                attr->parent_ = nullptr;
                attr->next_ = nullptr;
            }
        }

        using XContainer::Add;
        using XContainer::AddFirst;

        [[nodiscard]] System::Xml::XmlNodeType getNodeTypeProperty() const override { return System::Xml::XmlNodeType::Element; }

        /** @return The qualified name of the element. */
        [[nodiscard]] const XName& getNameProperty() const { return name_; }
        /**
         * @brief Sets the qualified name of the element.
         *
         * Raises a **Name** change pair on this element and every ancestor (#2199), matching
         * .NET's `XElement.Name` setter (`XElement.cs:276-278`).
         */
        void setNameProperty(const XName& name) {
            const bool notify = NotifyChanging(this, XObjectChangeEventArgs::Name);
            name_ = name;
            if (notify) NotifyChanged(this, XObjectChangeEventArgs::Name);
        }

        /**
         * @return The text contents of this element. If there is text content interspersed with
         * nodes (mixed content), the text is concatenated (matches .NET's XElement.Value getter).
         */
        [[nodiscard]] std::string getValueProperty() const;
        /** @brief Replaces all content of this element with a single text node containing @p v (a no-op child-wise if @p v is empty). */
        void setValueProperty(const std::string& v);

        // --- Attributes ----------------------------------------------------------------

        /**
         * @brief Appends @p attr to this element's attributes.
         * If @p attr already belongs to another element, it is moved here (see XContainer's doc-comment).
         * @throws System::InvalidOperationException if this element already has an attribute with the same name.
         */
        void Add(std::shared_ptr<XAttribute> attr);

        /** @brief Appends a text child containing @p text. Convenience for `Add(std::make_shared<XText>(text))`. */
        void Add(const std::string& text);

        /** Returns the first attribute matching @p name, or nullptr. */
        [[nodiscard]] std::shared_ptr<XAttribute> Attribute(const XName& name) const {
            for (auto& a : attributes_)
                if (a->getNameProperty() == name) return a;
            return nullptr;
        }

        /**
         * @return All attributes of this element, in document order.
         *
         * @warning **Borrowed view.** The reference names this element's live attribute store.
         * - **Precondition:** this element outlives every use of the reference.
         * - **Invalidation:** by this element's destruction, and by nothing else — in particular
         *   **not** by `Add`/`RemoveAttribute` reallocating the vector, because the reference
         *   names the vector *object*, which lives inside the element.
         * - **Owning alternative:** `Attributes()`, below, returns the same content by value; its
         *   `shared_ptr`s keep the attributes alive after this element is destroyed.
         *
         * This is the ordinary C++ reference-lifetime contract that `std::vector::front()` and
         * `std::string::c_str()` also have; it is stated here because the borrow is easy to keep
         * (`docs/OwnedTreeLifetimeContractPlan.md` §42.2).
         */
        [[nodiscard]] const std::vector<std::shared_ptr<XAttribute>>& getAttributesProperty() const { return attributes_; }
        /**
         * @return All attributes of this element, in document order (.NET-style method name; same
         * content as getAttributesProperty()).
         *
         * Unlike `getAttributesProperty()` this returns **owning** handles by value, so the result
         * stays valid after this element is destroyed — the attributes are then detached, exactly
         * as `RemoveAttributes()` leaves them. Prefer it whenever the result outlives the call.
         */
        [[nodiscard]] std::vector<std::shared_ptr<XAttribute>> Attributes() const { return attributes_; }

        /** @return The first attribute of this element, or nullptr. */
        [[nodiscard]] std::shared_ptr<XAttribute> getFirstAttributeProperty() const { return attributes_.empty() ? nullptr : attributes_.front(); }
        /** @return The last attribute of this element, or nullptr. */
        [[nodiscard]] std::shared_ptr<XAttribute> getLastAttributeProperty() const { return attributes_.empty() ? nullptr : attributes_.back(); }
        /** @return true if this element has at least one attribute. */
        [[nodiscard]] bool getHasAttributesProperty() const { return !attributes_.empty(); }
        /** @return true if this element has at least one child element. */
        [[nodiscard]] bool getHasElementsProperty() const;
        /** @return true if this element has no content (no attributes are required to be empty — matches .NET, which defines IsEmpty in terms of content only). */
        [[nodiscard]] bool getIsEmptyProperty() const { return children_.empty(); }

        /** @brief Removes all attributes from this element. */
        void RemoveAttributes();
        /** @brief Detaches @p attr from this element (used internally by XAttribute::Remove()). No-op if @p attr does not belong to this element. */
        void RemoveAttribute(XAttribute* attr);

        /** @brief Removes all attributes and content from this element. */
        void RemoveAll();

        /** Returns the value of the attribute named @p name, or std::nullopt if absent. */
        [[nodiscard]] std::optional<std::string> getAttributeValue(const std::string& name) const {
            auto a = Attribute(name);
            if (a) return a->getValueProperty();
            return std::nullopt;
        }

        // --- Namespaces ------------------------------------------------------------------

        /**
         * @brief Returns the default namespace in scope for this element.
         *
         * Resolved from the `xmlns="..."` declarations this element and its ancestor elements
         * carry as attributes, innermost first; `XNamespace::None` when none is in scope or the
         * innermost declaration is the undeclaration `xmlns=""`.
         *
         * C++ counterpart of .NET XElement.GetDefaultNamespace().
         */
        [[nodiscard]] XNamespace GetDefaultNamespace() const;

        /**
         * @brief Returns the prefix currently bound to @p ns for this element, or "" if none is.
         *
         * Resolved from the `xmlns:prefix="..."` declarations this element and its ancestor
         * elements carry as attributes, innermost first, skipping any prefix a nearer
         * declaration has rebound to a different URI. The reserved `xml` prefix always resolves,
         * whether or not it was declared.
         *
         * C++ counterpart of .NET XElement.GetPrefixOfNamespace(XNamespace).
         */
        [[nodiscard]] std::string GetPrefixOfNamespace(const XNamespace& ns) const;

        // --- Serialization ---------------------------------------------------------------

        void WriteTo(System::Xml::XmlWriter& writer) const override;

        /** @brief Saves this element to @p fileName (formatted, unless @p options disables it). */
        void Save(const std::string& fileName, SaveOptions options = SaveOptions::None) const;
        /** @brief Writes this element to @p writer (no start/end-document wrapping). */
        void Save(System::Xml::XmlWriter& writer) const;

        /**
         * @brief Parses @p xml into an XElement.
         * @param xml XML source text; must contain exactly one root element.
         * @param options Controls whitespace handling (SetBaseUri/SetLineInfo have no effect — see LoadOptions).
         * Only affects text nodes that mix whitespace with real content — the vendored tinyxml2
         * parser never surfaces pure-whitespace-only runs immediately adjacent to element tags as
         * text nodes at all (verified directly), so LoadOptions::PreserveWhitespace has no
         * observable effect for that specific case (a limitation inherited from the parsing
         * backend, already documented at the classic-DOM layer on XmlDocument::PreserveWhitespace).
         * @throws System::Xml::XmlException on malformed XML.
         */
        [[nodiscard]] static std::shared_ptr<XElement> Parse(const std::string& xml, LoadOptions options = LoadOptions::None);

        /**
         * @brief Loads and parses the XML file at @p filePath into an XElement.
         * @throws System::Xml::XmlException if the file is missing or malformed.
         */
        [[nodiscard]] static std::shared_ptr<XElement> Load(const std::string& filePath, LoadOptions options = LoadOptions::None);

        [[nodiscard]] SharpRuntime::intcs GetDeepHashCode() const override;

    protected:
        void SerializeTo(std::ostream& os, int depth, bool indent) const override;
        [[nodiscard]] bool DeepEqualsCore(const XNode& other) const override;

        /**
         * @brief Rejects adding an XDocument or XDocumentType as a child element.
         * @throws System::ArgumentException if @p node is an XDocument or XDocumentType.
         */
        void ValidateNode(const XNode& node) const override;

    private:
        void AppendTextValue(std::string& sb) const;

        /**
         * @brief Seeds @p scope with every namespace declaration in scope from this element's
         * ANCESTORS, outermost first (ticket #2197).
         *
         * Walks `getParentProperty()` upward, so it stops at an `XDocument` and touches only
         * live elements. Nothing is cached: the scope is rebuilt on each serialization entry
         * point, which is exactly what keeps this repair object-layout compatible.
         */
        void CollectInheritedScope(detail::NamespaceScope& scope) const;

        /** @brief Adds this element's own `xmlns`/`xmlns:prefix` attributes to @p scope. */
        void DeclareOwnNamespaces(detail::NamespaceScope& scope) const;

        /**
         * @brief Renders this element's start tag into @p qualifiedName plus the attribute
         * strings @p attributes, declaring whatever @p scope does not already bind.
         *
         * @p scope is updated with every declaration emitted, so the caller can pass it down to
         * this element's children.
         */
        void ResolveStartTag(detail::NamespaceScope& scope, std::string& qualifiedName,
                             std::vector<std::pair<std::string, std::string>>& attributes) const;

        /** @brief The recursive body of SerializeTo, carrying the namespace scope down. */
        void SerializeElementTo(std::ostream& os, int depth, bool indent,
                                detail::NamespaceScope scope) const;

        /** @brief The recursive body of WriteTo, carrying the namespace scope down. */
        void WriteElementTo(System::Xml::XmlWriter& writer, detail::NamespaceScope scope) const;
    };

} // namespace System::Xml::Linq
