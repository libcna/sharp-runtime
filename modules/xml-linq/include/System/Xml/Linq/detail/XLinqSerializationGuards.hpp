// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>

#include "System/Xml/XmlException.hpp"
#include "System/Xml/detail/XmlLexicalSanitizer.hpp"

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
     * **What this deliberately does NOT decide.** Characters outside `Char` other than NUL
     * (0x01, 0x0C, 0x0E–0x1F) are a different question: both doors emit them faithfully rather
     * than losing them, this runtime's own reader accepts them, and both "reject" and "emit" are
     * implementable — so whether they are rejected is a `XmlWriterSettings::CheckCharacters`
     * decision tracked separately. A flag can only govern a choice whose branches both exist.
     * This guard tests for a NUL and nothing else.
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
    }

} // namespace System::Xml::Linq::detail
