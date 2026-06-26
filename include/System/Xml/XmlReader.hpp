// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include <string>
#include <vector>
#include <utility>

namespace System::Xml {

    /** @brief Specifies the state of the XmlReader. */
    enum class ReadState { Initial = 0, Interactive = 1, Error = 2, EndOfFile = 3, Closed = 4 };

    /** @brief Specifies the type of the current XML node. */
    enum class XmlNodeType {
        None = 0, Element = 1, Attribute = 2, Text = 3, CDATA = 4,
        Comment = 8, Document = 9, EndElement = 15, EndEntity = 16,
        XmlDeclaration = 17
    };

    struct XmlReaderState; ///< Opaque tinyxml2 state; defined in XmlReader.cpp.

    /**
     * @brief Represents a reader that provides fast, non-cached, forward-only access to XML data.
     *
     * Implemented as a DOM-cursor over a tinyxml2 document tree.  The entire
     * document is parsed on creation; @c Read() advances a flat event list
     * built from the DOM.
     *
     * Partial C++ counterpart of .NET System.Xml.XmlReader.
     *
     * @note Status: IMPLEMENTED — backed by vendored tinyxml2.
     */
    class XmlReader {
        std::unique_ptr<XmlReaderState> state_;

    public:
        /** @brief Internal constructor used by factory methods; prefer @c Create() / @c CreateFromString(). */
        explicit XmlReader(std::unique_ptr<XmlReaderState> s);

        ~XmlReader();

        /** @brief Returns the type of the current node. */
        [[nodiscard]] XmlNodeType getNodeTypeProperty() const;

        /** @brief Returns the qualified name of the current node. */
        [[nodiscard]] std::string getNameProperty() const;

        /** @brief Returns the text value of the current node (Text/CDATA/Comment). */
        [[nodiscard]] std::string getValueProperty() const;

        /** @brief Returns @c true if the current element has no child nodes. */
        [[nodiscard]] bool getIsEmptyElementProperty() const;

        /** @brief Returns the current read state. */
        [[nodiscard]] ReadState getReadStateProperty() const;

        /**
         * @brief Advances the reader to the next node.
         *
         * @return @c true if a node was read; @c false at end of document.
         */
        bool Read();

        /**
         * @brief Moves the cursor back to the element node after iterating attributes.
         *
         * @return @c true if the reader is positioned on an element.
         */
        bool MoveToElement();

        /**
         * @brief Moves to the next attribute of the current element.
         *
         * @return @c true if there was a next attribute to move to.
         */
        bool MoveToNextAttribute();

        /**
         * @brief Returns the value of an attribute by name on the current element.
         *
         * @param name Attribute local name.
         * @return Attribute value, or empty string if not found.
         */
        [[nodiscard]] std::string GetAttribute(const std::string& name) const;

        /**
         * @brief Reads the text content of the current element and advances past its end-element.
         *
         * @return The concatenated text content.
         */
        std::string ReadElementContentAsString();

        /**
         * @brief Verifies that the current node is an element and advances the reader.
         *
         * @throws std::runtime_error if the current node is not an element.
         */
        void ReadStartElement();

        /**
         * @brief Verifies that the current node is an end-element and advances the reader.
         *
         * @throws std::runtime_error if the current node is not an end-element.
         */
        void ReadEndElement();

        /** @brief Closes the reader and releases resources. */
        void Close();

        /**
         * @brief Creates an XmlReader that reads from a file path or raw XML content.
         *
         * If @p inputUri looks like a file path (contains '/' or '\' or ends with
         * ".xml") the file is loaded; otherwise treated as raw XML text.
         *
         * @param inputUri  File path or raw XML text.
         * @return Heap-allocated XmlReader; caller owns the pointer.
         * @throws std::runtime_error on parse error.
         */
        static XmlReader* Create(const std::string& inputUri);

        /**
         * @brief Creates an XmlReader that parses @p xmlContent as raw XML.
         *
         * @param xmlContent  Well-formed XML text.
         * @return Heap-allocated XmlReader; caller owns the pointer.
         * @throws std::runtime_error on parse error.
         */
        static XmlReader* CreateFromString(const std::string& xmlContent);
    };

} // namespace System::Xml
