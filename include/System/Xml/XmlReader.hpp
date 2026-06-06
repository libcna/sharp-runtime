// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/NotImplementedException.hpp"

// NOTE: Full implementation requires an XML library such as tinyxml2 or pugixml.
// To implement:
//   - tinyxml2 (MIT, single .cpp + .h): https://github.com/leethomason/tinyxml2
//     Use tinyxml2::XMLDocument / XMLElement / XMLAttribute.
//   - pugixml (MIT): https://pugixml.org/
//     Use pugi::xml_document / xml_node / xml_attribute.
// XNA content pipeline uses XML for XNB descriptor files; this is the entry point for that feature.

namespace System::Xml {

    /** @brief Specifies the state of the XmlReader. */
    enum class ReadState { Initial = 0, Interactive = 1, Error = 2, EndOfFile = 3, Closed = 4 };

    /** @brief Specifies the type of the current XML node. */
    enum class XmlNodeType {
        None = 0, Element = 1, Attribute = 2, Text = 3, CDATA = 4,
        Comment = 8, Document = 9, EndElement = 15, EndEntity = 16,
        XmlDeclaration = 17
    };

    /**
     * @brief Represents a reader that provides fast, non-cached, forward-only access to XML data.
     *
     * Partial C++ counterpart of .NET System.Xml.XmlReader.
     *
     * @note Status: Stub — all methods throw NotImplementedException.
     *   See comment at top of this file for integration with tinyxml2 or pugixml.
     */
    class XmlReader {
    public:
        virtual ~XmlReader() = default;

        [[nodiscard]] virtual XmlNodeType getNodeTypeProperty() const {
            throw NotImplementedException("XmlReader requires tinyxml2 or pugixml. See header for details.");
        }
        [[nodiscard]] virtual std::string getNameProperty() const {
            throw NotImplementedException("XmlReader requires tinyxml2 or pugixml.");
        }
        [[nodiscard]] virtual std::string getValueProperty() const {
            throw NotImplementedException("XmlReader requires tinyxml2 or pugixml.");
        }
        [[nodiscard]] virtual bool getIsEmptyElementProperty() const {
            throw NotImplementedException("XmlReader requires tinyxml2 or pugixml.");
        }

        virtual bool Read() {
            throw NotImplementedException("XmlReader requires tinyxml2 or pugixml.");
        }
        virtual void Close() {}

        virtual bool MoveToElement() {
            throw NotImplementedException("XmlReader requires tinyxml2 or pugixml.");
        }
        virtual bool MoveToNextAttribute() {
            throw NotImplementedException("XmlReader requires tinyxml2 or pugixml.");
        }

        [[nodiscard]] virtual std::string GetAttribute(const std::string&) const {
            throw NotImplementedException("XmlReader requires tinyxml2 or pugixml.");
        }
        [[nodiscard]] virtual std::string ReadElementContentAsString() {
            throw NotImplementedException("XmlReader requires tinyxml2 or pugixml.");
        }
        virtual void ReadStartElement() {
            throw NotImplementedException("XmlReader requires tinyxml2 or pugixml.");
        }
        virtual void ReadEndElement() {
            throw NotImplementedException("XmlReader requires tinyxml2 or pugixml.");
        }

        static XmlReader* Create(const std::string& /*inputUri*/) {
            throw NotImplementedException("XmlReader::Create requires tinyxml2 or pugixml.");
        }
    };

} // namespace System::Xml
