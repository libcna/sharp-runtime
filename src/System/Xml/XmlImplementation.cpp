// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Xml/XmlImplementation.hpp"

#include "System/Xml/XmlDocument.hpp"

namespace System::Xml {

    XmlImplementation::XmlImplementation() : nameTable_(std::make_shared<NameTable>()) {}

    XmlImplementation::XmlImplementation(std::shared_ptr<XmlNameTable> nt) : nameTable_(std::move(nt)) {}

    bool XmlImplementation::HasFeature(const std::string& /*strFeature*/, const std::string& /*strVersion*/) const {
        return false;
    }

    std::unique_ptr<XmlDocument> XmlImplementation::CreateDocument() const {
        return std::make_unique<XmlDocument>(nameTable_);
    }

} // namespace System::Xml
