// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <memory>
#include <string>

#include "System/Xml/NameTable.hpp"

namespace System::Xml {

    class XmlDocument;

    /**
     * @brief Provides methods for performing operations independent of any particular document
     * instance (DOM Level 1 IDOMImplementation).
     *
     * C++ counterpart of .NET System.Xml.XmlImplementation.
     */
    class XmlImplementation {
        std::shared_ptr<XmlNameTable> nameTable_;

    public:
        XmlImplementation();
        explicit XmlImplementation(std::shared_ptr<XmlNameTable> nt);

        /** @return Always false; DOM Level 2 feature testing is not implemented. */
        [[nodiscard]] bool HasFeature(const std::string& strFeature, const std::string& strVersion) const;

        /** @return A new, empty XmlDocument sharing this implementation's NameTable. */
        [[nodiscard]] virtual std::unique_ptr<XmlDocument> CreateDocument() const;
    };

} // namespace System::Xml
