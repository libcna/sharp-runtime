// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>

#include "System/Xml/XmlException.hpp"
#include "System/Xml/detail/XmlLexicalSanitizer.hpp"
#include "System/ArgumentException.hpp"

namespace System::Xml::Linq::detail {

    /**
     * @file
     * @brief The guard every `System::Xml::Linq` direct serializer applies before it writes a
     * value into XML text (ticket #2201).
     *
     * **Why this exists.** Every `X*` node kind has two serialization doors: `WriteTo(XmlWriter&)`,
     * which delegates to `System::Xml::XmlWriter`, and `SerializeTo(std::ostream&)` — the door
     * behind `ToString()`, `ToString(SaveOptions)` and `Save(fileName)` — which builds the text
     * itself. Ticket #2085 made the writer door reject an embedded NUL, because it reached
     * tinyxml2's `const char*` API and silently **truncated** the value there. The direct door
     * has no `const char*` boundary, so it did the mirror-image thing instead: it **emitted** the
     * NUL, producing bytes that this runtime's own reader then rejects with
     * `XML_ERROR_PARSING_TEXT` / `_ATTRIBUTE` / `_CDATA` / `_COMMENT` (measured,
     * `build-probe/2201_probe1_doors.log`). Lost one way, unreadable the other; one policy now
     * answers both.
     *
     * **Why rejection is not a policy choice here.** The XML 1.0 `Char` production excludes
     * U+0000 outright, and a character reference must itself match `Char`, so there is no
     * spelling — literal or escaped — that carries a NUL through a document. "Write it in full"
     * is not an implementable branch, which is why this needs no `XmlWriterSettings` flag and
     * why `System::Xml::detail::ContainsNul` (the single detector, shared with the writer door)
     * says so in its own doc-comment.
     *
     * **The other non-`Char` characters, settled by #2349.** They were a different question when
     * this comment was written; the reference settles it. .NET's Xml.Linq save path builds a
     * DEFAULT `XmlWriterSettings` and touches only `Indent` and `NamespaceHandling`
     * (`XNode.cs:681-687`), so it inherits `CheckCharacters = true` and checks exactly as any
     * default-settings writer does. That is why this door needs no settings channel of its own,
     * and why the two doors agree by construction rather than by coordination.
     *
     * The diagnostic deliberately mirrors `XmlWriter.cpp`'s file-local `ThrowIfContainsNul`, so
     * the two doors of one node kind report the same cause under different member names.
     */

    /**
     * @brief Throws if @p text contains a NUL byte, naming the door and the value that carried it.
     * @param text  The value about to be serialized.
     * @param member  The fully qualified member being served, e.g. `"XText::SerializeTo"`.
     * @param what  What the value is, e.g. `"text"` or `"attribute value"`.
     * @throws System::Xml::XmlException if @p text contains a NUL.
     */
    inline void ThrowIfContainsNul(const std::string& text, const char* member, const char* what) {
        if (System::Xml::detail::ContainsNul(text))
            throw System::Xml::XmlException(std::string(member) + ": the " + what +
                                            " contains a NUL character, which cannot be "
                                            "represented in XML.");

        // #2349, the same check the writer door runs and for the same reason. The exception TYPE
        // is .NET's ArgumentException, not this door's XmlException: the NUL case above is this
        // port's own truncation guard (#2085) while this one transcribes
        // XmlConvert.CreateInvalidCharException (XmlConvert.cs:1614-1622).
        const std::size_t bad = System::Xml::detail::FindNonCharCodePoint(text);
        if (bad != std::string::npos) {
            std::uint32_t cp = 0;
            std::size_t   len = 0;
            if (!System::detail::TryDecodeUtf8Scalar(text, bad, cp, len))
                cp = static_cast<unsigned char>(text[bad]);
            throw System::ArgumentException(System::Xml::detail::InvalidCharacterMessage(cp));
        }
    }

} // namespace System::Xml::Linq::detail
