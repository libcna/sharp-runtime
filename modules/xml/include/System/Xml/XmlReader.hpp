// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include <string>
#include <vector>
#include <utility>

#include "System/Xml/ReadState.hpp"
#include "System/Xml/XmlNodeType.hpp"

namespace System::Xml {

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

        /** @brief Returns the type of the current node, or @c XmlNodeType::None when the
         *  reader is on no node — before the first @c Read(), past the last one, or after
         *  @c Close(). */
        [[nodiscard]] XmlNodeType getNodeTypeProperty() const;

        /** @brief Returns the qualified name of the current node, or @c "" when the reader
         *  is on no node (including after @c Close()). */
        [[nodiscard]] std::string getNameProperty() const;

        /** @brief Returns the text value of the current node (Text/CDATA/Comment), or @c ""
         *  when the reader is on no node (including after @c Close()). */
        [[nodiscard]] std::string getValueProperty() const;

        /** @brief Returns @c true if the current element was written in the empty-tag
         *  (self-closing, e.g. @c &lt;foo/&gt;) syntax; @c false for @c &lt;foo&gt;&lt;/foo&gt;,
         *  even when it has no children. */
        [[nodiscard]] bool getIsEmptyElementProperty() const;

        /** @brief Returns the current read state. */
        [[nodiscard]] ReadState getReadStateProperty() const;

        /**
         * @brief Advances the reader to the next node.
         *
         * @return @c true if a node was read; @c false at end of document, and @c false once
         *         @c Close() has been called — a closed reader does not advance, and
         *         @c getReadStateProperty() stays @c ReadState::Closed across any number of
         *         further calls, exactly as @c ReadState::EndOfFile already behaves.
         */
        bool Read();

        /**
         * @brief Moves the cursor back to the element node after iterating attributes.
         *
         * @return @c true if the reader is positioned on an element; @c false when the
         *         reader is on no node, including after @c Close().
         */
        bool MoveToElement();

        /**
         * @brief Moves to the next attribute of the current element.
         *
         * @return @c true if there was a next attribute to move to; @c false when the
         *         reader is on no node, including after @c Close().
         */
        bool MoveToNextAttribute();

        /**
         * @brief Returns the value of an attribute by name on the current element.
         *
         * @param name Attribute local name.
         * @return Attribute value, or empty string if not found or if the reader is on no
         *         node, including after @c Close().
         */
        [[nodiscard]] std::string GetAttribute(const std::string& name) const;

        /**
         * @brief Reads the text content of the current element and advances past its end-element.
         *
         * @return The concatenated text content, or @c "" once @c Close() has been called.
         */
        std::string ReadElementContentAsString();

        /**
         * @brief Verifies that the current node is an element and advances the reader.
         *
         * @throws XmlException if the current node is not an element, which includes a
         *         reader that has been closed — a closed reader is on no node.
         */
        void ReadStartElement();

        /**
         * @brief Verifies that the current node is an end-element and advances the reader.
         *
         * @throws XmlException if the current node is not an end-element, which includes a
         *         reader that has been closed — a closed reader is on no node.
         */
        void ReadEndElement();

        /**
         * @brief Closes the reader and releases resources.
         *
         * Terminal and idempotent. Afterwards @c getReadStateProperty() is permanently
         * @c ReadState::Closed, the cursor never advances again, and every accessor reports
         * the reader's own "no current node" answer. Before this contract was enforced,
         * @c Read() both advanced past a closed reader and overwrote the closed state with
         * @c ReadState::Interactive.
         */
        void Close();

        /**
         * @brief Creates an XmlReader that reads from a file path or raw XML content.
         *
         * If @p inputUri (after trimming leading whitespace) starts with '<' it is always
         * treated as raw XML text; otherwise, if it looks like a file path (contains '/' or
         * '\' or ends with ".xml") the file is loaded, and it is treated as raw XML text
         * otherwise.
         *
         * @param inputUri  File path or raw XML text.
         * @return Heap-allocated XmlReader; caller owns the pointer.
         * @throws XmlException on parse error.
         */
        static XmlReader* Create(const std::string& inputUri);

        /**
         * @brief Creates an XmlReader that parses @p xmlContent as raw XML.
         *
         * @param xmlContent  Well-formed XML text.
         * @return Heap-allocated XmlReader; caller owns the pointer.
         * @throws XmlException on parse error.
         */
        static XmlReader* CreateFromString(const std::string& xmlContent);
    };

} // namespace System::Xml
