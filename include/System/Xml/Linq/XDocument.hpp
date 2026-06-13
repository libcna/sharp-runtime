// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include <sstream>
#include <string>
#include "System/Xml/Linq/XElement.hpp"

namespace System::Xml::Linq {

    /// Represents the XML declaration (<?xml version="..." encoding="..."?>).
    class XDeclaration {
        std::string version_;
        std::string encoding_;
        std::string standalone_;
    public:
        /// Constructs an XML declaration with the given version, encoding, and standalone value.
        XDeclaration(const std::string& version, const std::string& encoding, const std::string& standalone)
            : version_(version), encoding_(encoding), standalone_(standalone) {}

        /// @return The XML version string (e.g. "1.0").
        [[nodiscard]] const std::string& getVersionProperty()    const { return version_; }

        /// @return The encoding name (e.g. "utf-8").
        [[nodiscard]] const std::string& getEncodingProperty()   const { return encoding_; }

        /// @return The standalone declaration value (e.g. "yes" or "no").
        [[nodiscard]] const std::string& getStandaloneProperty() const { return standalone_; }

        /// @return The serialised processing instruction string.
        [[nodiscard]] std::string ToString() const {
            return "<?xml version=\"" + version_ + "\" encoding=\"" + encoding_ + "\"?>";
        }
    };

    /// Represents an XML document, optionally including a declaration and a root element.
    class XDocument {
        std::shared_ptr<XDeclaration> declaration_;
        std::shared_ptr<XElement> root_;

    public:
        /// Default constructor — creates an empty document.
        XDocument() = default;

        /// Constructs a document with only a root element.
        explicit XDocument(std::shared_ptr<XElement> root) : root_(std::move(root)) {}

        /// Constructs a document with an XML declaration and a root element.
        XDocument(std::shared_ptr<XDeclaration> decl, std::shared_ptr<XElement> root)
            : declaration_(std::move(decl)), root_(std::move(root)) {}

        /// @return The XML declaration, or nullptr if absent.
        [[nodiscard]] std::shared_ptr<XDeclaration> getDeclarationProperty() const { return declaration_; }

        /// @return The root element, or nullptr if absent.
        [[nodiscard]] std::shared_ptr<XElement>     getRootProperty()        const { return root_; }

        /// Sets the XML declaration.
        void setDeclarationProperty(std::shared_ptr<XDeclaration> d) { declaration_ = std::move(d); }

        /// Sets the root element.
        void setRootProperty(std::shared_ptr<XElement> r)            { root_ = std::move(r); }

        /// Returns the root element if its local name matches @p name, otherwise nullptr.
        [[nodiscard]] std::shared_ptr<XElement> Element(const std::string& name) const {
            if (root_ && root_->getNameProperty().getLocalNameProperty() == name) return root_;
            return nullptr;
        }

        /// Serialises the document to a string.
        /// @param indent If true, apply indentation.
        [[nodiscard]] std::string ToString(bool indent = false) const {
            std::ostringstream oss;
            if (declaration_) oss << declaration_->ToString() << "\n";
            if (root_)        oss << root_->ToString(indent);
            return oss.str();
        }

        /// Parses XML text into an XDocument (stub — requires XML parser backend).
        /// @param xml The XML source text (currently ignored).
        static std::shared_ptr<XDocument> Parse(const std::string& /*xml*/) {
            // Stub — requires XML parser backend (tinyxml2/pugixml)
            return std::make_shared<XDocument>(std::make_shared<XElement>("root"));
        }

        /// Loads an XML file into an XDocument (stub — requires XML parser backend).
        /// @param filePath Path to the XML file (currently ignored).
        static std::shared_ptr<XDocument> Load(const std::string& /*filePath*/) {
            // Stub — requires XML parser backend
            return std::make_shared<XDocument>(std::make_shared<XElement>("root"));
        }

        /// Saves the document to the given file path.
        void Save(const std::string& filePath) const;
    };

} // namespace System::Xml::Linq
