// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/Xml/Linq/XNode.hpp"

namespace System::Xml::Linq {

    /**
     * @brief Represents an XML Document Type Definition (DTD).
     *
     * C++ counterpart of .NET System.Xml.Linq.XDocumentType.
     *
     * @note InternalSubset is always "" when parsed from XML — this runtime's tinyxml2-backed
     * DOM layer does not parse the internal DTD subset (see System::Xml::XmlDocumentType's doc
     * comment); settable/gettable here for API completeness and manual construction.
     *
     * @note **Validation happens at serialization, not at construction** (ticket #2200, the
     * Xml.Linq half of #2084). Both doors — `WriteTo(XmlWriter&)`, which delegates to
     * `XmlWriter::WriteDocType`, and `SerializeTo(ostream&)` behind `ToString()`/`Save()` —
     * reject a name that is not an XML name, a public identifier containing a non-`PubidChar`
     * (which includes `"`), and a system identifier that contains `>` or both quote characters.
     * A system identifier containing only `"` is re-delimited with `'` rather than escaped: a
     * DOCTYPE `SystemLiteral` has no escape mechanism, and this runtime's own DOCTYPE reader
     * never un-escapes one. The constructor and the setters deliberately accept anything, the
     * same boundary XProcessingInstruction records for its target.
     *
     * @note The internal subset is **not** validated or repaired at either door. This runtime
     * stores a DOCTYPE as a single `>`-terminated node, so an ordinary subset such as
     * `<!ENTITY a "b">` is lost on read-back — a pre-existing limitation with no well-formed
     * alternative spelling, tracked separately from #2200.
     */
    class XDocumentType : public XNode {
        std::string name_;
        std::string publicId_;
        std::string systemId_;
        std::string internalSubset_;

    public:
        /** @brief Initializes a new document type declaration. */
        XDocumentType(const std::string& name, const std::string& publicId,
                      const std::string& systemId, const std::string& internalSubset)
            : name_(name), publicId_(publicId), systemId_(systemId), internalSubset_(internalSubset) {}

        [[nodiscard]] System::Xml::XmlNodeType getNodeTypeProperty() const override { return System::Xml::XmlNodeType::DocumentType; }

        /** @return The name of this DTD. */
        [[nodiscard]] const std::string& getNameProperty() const { return name_; }
        /** @brief Sets the name of this DTD. */
        /** @brief Raises a **Name** pair (#2199), matching .NET (XDocumentType.cs:84-86). */
        void setNameProperty(const std::string& name) {
            const bool notify = NotifyChanging(this, XObjectChangeEventArgs::Name);
            name_ = name;
            if (notify) NotifyChanged(this, XObjectChangeEventArgs::Name);
        }

        /** @return The public identifier, or "" if absent. */
        [[nodiscard]] const std::string& getPublicIdProperty() const { return publicId_; }
        /** @brief Sets the public identifier. */
        /** @brief Raises a **Value** pair (#2199), matching .NET (XDocumentType.cs:115-117). */
        void setPublicIdProperty(const std::string& publicId) {
            const bool notify = NotifyChanging(this, XObjectChangeEventArgs::Value);
            publicId_ = publicId;
            if (notify) NotifyChanged(this, XObjectChangeEventArgs::Value);
        }

        /** @return The system identifier, or "" if absent. */
        [[nodiscard]] const std::string& getSystemIdProperty() const { return systemId_; }
        /** @brief Sets the system identifier. */
        /** @brief Raises a **Value** pair (#2199), matching .NET (XDocumentType.cs:132-134). */
        void setSystemIdProperty(const std::string& systemId) {
            const bool notify = NotifyChanging(this, XObjectChangeEventArgs::Value);
            systemId_ = systemId;
            if (notify) NotifyChanged(this, XObjectChangeEventArgs::Value);
        }

        /** @return The internal subset, or "" if absent/not parsed (see class doc-comment). */
        [[nodiscard]] const std::string& getInternalSubsetProperty() const { return internalSubset_; }
        /** @brief Sets the internal subset. */
        /** @brief Raises a **Value** pair (#2199), matching .NET (XDocumentType.cs:66-68). */
        void setInternalSubsetProperty(const std::string& internalSubset) {
            const bool notify = NotifyChanging(this, XObjectChangeEventArgs::Value);
            internalSubset_ = internalSubset;
            if (notify) NotifyChanged(this, XObjectChangeEventArgs::Value);
        }

        void WriteTo(System::Xml::XmlWriter& writer) const override;
        [[nodiscard]] SharpRuntime::intcs GetDeepHashCode() const override {
            return static_cast<SharpRuntime::intcs>(std::hash<std::string>{}(name_)) ^
                   static_cast<SharpRuntime::intcs>(std::hash<std::string>{}(publicId_)) ^
                   static_cast<SharpRuntime::intcs>(std::hash<std::string>{}(systemId_)) ^
                   static_cast<SharpRuntime::intcs>(std::hash<std::string>{}(internalSubset_));
        }

    protected:
        void SerializeTo(std::ostream& os, int depth, bool indent) const override;
        [[nodiscard]] bool DeepEqualsCore(const XNode& other) const override {
            const auto& o = static_cast<const XDocumentType&>(other);
            return name_ == o.name_ && publicId_ == o.publicId_ && systemId_ == o.systemId_ && internalSubset_ == o.internalSubset_;
        }
    };

} // namespace System::Xml::Linq
