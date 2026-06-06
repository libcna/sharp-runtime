// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/NotImplementedException.hpp"

// NOTE: Full implementation requires tinyxml2 or pugixml — see XmlReader.hpp for details.

namespace System::Xml {

    /**
     * @brief Represents a writer that provides a fast, non-cached, forward-only way to generate XML.
     *
     * Partial C++ counterpart of .NET System.Xml.XmlWriter.
     *
     * @note Status: Stub — all methods throw NotImplementedException.
     *   See include/System/Xml/XmlReader.hpp for integration notes.
     */
    class XmlWriter {
    public:
        virtual ~XmlWriter() = default;

        virtual void WriteStartDocument() {
            throw NotImplementedException("XmlWriter requires tinyxml2 or pugixml. See XmlReader.hpp.");
        }
        virtual void WriteEndDocument() {
            throw NotImplementedException("XmlWriter requires tinyxml2 or pugixml.");
        }
        virtual void WriteStartElement(const std::string& /*localName*/) {
            throw NotImplementedException("XmlWriter requires tinyxml2 or pugixml.");
        }
        virtual void WriteEndElement() {
            throw NotImplementedException("XmlWriter requires tinyxml2 or pugixml.");
        }
        virtual void WriteAttributeString(const std::string& /*name*/, const std::string& /*value*/) {
            throw NotImplementedException("XmlWriter requires tinyxml2 or pugixml.");
        }
        virtual void WriteString(const std::string& /*text*/) {
            throw NotImplementedException("XmlWriter requires tinyxml2 or pugixml.");
        }
        virtual void WriteElementString(const std::string& /*name*/, const std::string& /*value*/) {
            throw NotImplementedException("XmlWriter requires tinyxml2 or pugixml.");
        }
        virtual void Flush() {}
        virtual void Close() {}

        static XmlWriter* Create(const std::string& /*outputFileName*/) {
            throw NotImplementedException("XmlWriter::Create requires tinyxml2 or pugixml.");
        }
    };

} // namespace System::Xml
