// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/Xml/Linq/XName.hpp"
#include "System/Xml/Linq/XObject.hpp"

namespace System::Xml::Linq {

    class XElement;

    /**
     * @brief Represents an XML attribute (name/value pair) on an XElement.
     *
     * C++ counterpart of .NET System.Xml.Linq.XAttribute.
     *
     * @note `next_` is an intrusive next-sibling-attribute link, matching real .NET's internal
     * `XAttribute.next` design (there it's part of a circular list rooted at the owning
     * XElement's `lastAttr`; here it's simply kept in sync with the owning XElement's
     * `attributes_` vector order whenever attributes are added/removed).
     */
    class XAttribute : public XObject {
        friend class XElement;

        XName name_;
        std::string value_;
        XAttribute* next_ = nullptr;

    public:
        /**
         * @brief Constructs an attribute with an XName key and string value (plain strings
         * convert implicitly via XName).
         *
         * @throws System::ArgumentException if @p name/@p value form an invalid namespace
         * declaration (see @c ValidateAttribute in XAttribute.cpp for the exact rules).
         */
        XAttribute(const XName& name, const std::string& value);

        /** @brief Copies @p other's name/value. The clone starts detached (no parent, no next). */
        XAttribute(const XAttribute& other) : name_(other.name_), value_(other.value_) {}

        [[nodiscard]] System::Xml::XmlNodeType getNodeTypeProperty() const override { return System::Xml::XmlNodeType::Attribute; }

        /** @return The qualified name of the attribute. */
        [[nodiscard]] const XName&       getNameProperty()  const { return name_; }

        /** @return The attribute value string. */
        [[nodiscard]] const std::string& getValueProperty() const { return value_; }

        /**
         * @brief Sets the attribute value.
         * @throws System::ArgumentException if this attribute is a namespace declaration and
         * @p v would violate the namespace-declaration rules (see the constructor).
         */
        void setValueProperty(const std::string& v);

        /**
         * @return @c true if this attribute is a namespace declaration (@c xmlns="..." or
         * @c xmlns:prefix="...").
         */
        [[nodiscard]] bool getIsNamespaceDeclarationProperty() const;

        /** @return Pointer to the next sibling attribute (in owning-element order), or nullptr. */
        [[nodiscard]] XAttribute* getNextAttributeProperty() const { return next_; }

        /** Sets the next sibling attribute in the linked list. Exposed for API compatibility; normally maintained automatically by XElement::Add/RemoveAttributes. */
        void setNextAttributeProperty(XAttribute* n)               { next_ = n; }

        /** @return Pointer to the previous sibling attribute (in owning-element order), or nullptr. */
        [[nodiscard]] XAttribute* getPreviousAttributeProperty() const;

        /** @brief Removes this attribute from its owning element. @throws System::InvalidOperationException if this attribute has no parent. */
        void Remove();

        /**
         * @return The attribute serialised as `name="value"` (value is XML-escaped), with a
         * namespace prefix resolved for a qualified name.
         *
         * @note **Never** XName::ToString()'s Clark notation (`{namespace}local`) — `{` and `}`
         * are not legal in an XML Name production, so writing that as the attribute *name*
         * would produce unparseable text. Ticket #2197 replaced the previous fallback, the bare
         * local name, which was well-formed but silently dropped the namespace and could emit
         * two attributes with the same rendered name.
         *
         * Resolution order: a namespace declaration renders as itself (`xmlns:p="..."` or
         * `xmlns="..."`); an unqualified name renders as its local name; a qualified name uses
         * the prefix its owning element has in scope. **A qualified attribute whose namespace
         * has no prefix in scope — including every detached attribute — renders with a
         * generated prefix and its declaration appended**, e.g. `p1:x="1" xmlns:p1="urn:a"`, so
         * the result is self-contained and well-formed on its own. That last shape is this
         * port's choice: the reference source is unavailable here, so the *spelling* of the
         * generated prefix is not claimed to match .NET's, only the requirement that the text
         * be well-formed and lossless.
         *
         * XElement's own serialization does **not** call this — it renders attributes with the
         * whole element's namespace scope, which this single-attribute view cannot see.
         */
        [[nodiscard]] std::string ToString() const;

    private:
        [[nodiscard]] static std::string EscapeValue(const std::string& s);
    };

} // namespace System::Xml::Linq
