// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include <sstream>
#include <string>
#include "System/Xml/Linq/XElement.hpp"

namespace System::Xml::Linq {

    class XDeclaration {
        std::string version_;
        std::string encoding_;
        std::string standalone_;
    public:
        XDeclaration(const std::string& version, const std::string& encoding, const std::string& standalone)
            : version_(version), encoding_(encoding), standalone_(standalone) {}

        [[nodiscard]] const std::string& getVersionProperty()    const { return version_; }
        [[nodiscard]] const std::string& getEncodingProperty()   const { return encoding_; }
        [[nodiscard]] const std::string& getStandaloneProperty() const { return standalone_; }

        [[nodiscard]] std::string ToString() const {
            return "<?xml version=\"" + version_ + "\" encoding=\"" + encoding_ + "\"?>";
        }
    };

    class XDocument {
        std::shared_ptr<XDeclaration> declaration_;
        std::shared_ptr<XElement> root_;

    public:
        XDocument() = default;
        explicit XDocument(std::shared_ptr<XElement> root) : root_(std::move(root)) {}
        XDocument(std::shared_ptr<XDeclaration> decl, std::shared_ptr<XElement> root)
            : declaration_(std::move(decl)), root_(std::move(root)) {}

        [[nodiscard]] std::shared_ptr<XDeclaration> getDeclarationProperty() const { return declaration_; }
        [[nodiscard]] std::shared_ptr<XElement>     getRootProperty()        const { return root_; }

        void setDeclarationProperty(std::shared_ptr<XDeclaration> d) { declaration_ = std::move(d); }
        void setRootProperty(std::shared_ptr<XElement> r)            { root_ = std::move(r); }

        [[nodiscard]] std::shared_ptr<XElement> Element(const std::string& name) const {
            if (root_ && root_->getNameProperty().getLocalNameProperty() == name) return root_;
            return nullptr;
        }

        [[nodiscard]] std::string ToString(bool indent = false) const {
            std::ostringstream oss;
            if (declaration_) oss << declaration_->ToString() << "\n";
            if (root_)        oss << root_->ToString(indent);
            return oss.str();
        }

        static std::shared_ptr<XDocument> Parse(const std::string& /*xml*/) {
            // Stub — requires XML parser backend (tinyxml2/pugixml)
            return std::make_shared<XDocument>(std::make_shared<XElement>("root"));
        }

        static std::shared_ptr<XDocument> Load(const std::string& /*filePath*/) {
            // Stub — requires XML parser backend
            return std::make_shared<XDocument>(std::make_shared<XElement>("root"));
        }

        void Save(const std::string& filePath) const;
    };

} // namespace System::Xml::Linq
