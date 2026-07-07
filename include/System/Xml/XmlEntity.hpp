// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include "System/Xml/XmlNode.hpp"

namespace System::Xml {

    /**
     * @brief Represents an entity declaration, e.g. `<!ENTITY ...>`.
     *
     * C++ counterpart of .NET System.Xml.XmlEntity. This runtime does not parse DTD internal
     * subsets (see XmlDocumentType), so this type is never populated by parsing — it exists for
     * API completeness and stands detached from the tinyxml2 tree.
     */
    class XmlEntity : public XmlNode {
        std::string name_;
        std::string publicId_;
        std::string systemId_;
        std::string notationName_;

    public:
        XmlEntity(XmlDocument* ownerDocument, std::string name, std::string publicId,
                  std::string systemId, std::string notationName)
            : name_(std::move(name)), publicId_(std::move(publicId)),
              systemId_(std::move(systemId)), notationName_(std::move(notationName)) {
            ownerDocument_ = ownerDocument;
        }

        [[nodiscard]] XmlNodeType getNodeTypeProperty() const override { return XmlNodeType::Entity; }
        [[nodiscard]] std::string getNameProperty() const override { return name_; }
        [[nodiscard]] std::string getLocalNameProperty() const override { return name_; }

        /** @return The public identifier, or "" if absent. */
        [[nodiscard]] const std::string& getPublicIdProperty() const { return publicId_; }
        /** @return The system identifier, or "" if absent. */
        [[nodiscard]] const std::string& getSystemIdProperty() const { return systemId_; }
        /** @return The name of the associated notation for an unparsed entity, or "" if none. */
        [[nodiscard]] const std::string& getNotationNameProperty() const { return notationName_; }
    };

} // namespace System::Xml
