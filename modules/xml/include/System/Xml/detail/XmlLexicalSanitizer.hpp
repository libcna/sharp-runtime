// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/detail/Utf8Scalar.hpp"
#include <cstdint>
#include <cstdio>

namespace System::Xml::detail {

    /**
     * @file
     * @brief The three well-formedness self-healing transforms shared by every XML serializer
     * in this runtime.
     *
     * These are **not** part of the ported .NET surface — they live in `detail` for the same
     * reason `System::Collections::detail::MutationCounter` does: they must be reachable from
     * more than one component, and having one definition is the point.
     *
     * They match `XmlEncodedRawTextWriter.WriteCommentOrPi` / `WriteCDataSection`: real .NET
     * never throws for a comment containing `--`, a processing instruction containing `?>`, or
     * a CDATA section containing `]]>`. It silently inserts a protective character — or, for
     * CDATA, splits into adjacent sections — so the emitted markup stays well-formed while the
     * original content survives a read-back.
     *
     * **Why they moved here (ticket #2196, SR-AUD-335).** They were file-local statics in
     * `XmlWriter.cpp`, so `System::Xml::XmlWriter` self-healed while
     * `System::Xml::Linq`'s direct `SerializeTo` serializers — the ones behind `ToString()` and
     * `Save(fileName)` — emitted the raw delimiters and produced text that this runtime's own
     * parser rejects or silently re-reads as different content. Copying the transforms into the
     * Linq component would have created a second definition free to drift from the first; a
     * single shared definition is what makes "both doors emit the same text" a property rather
     * than a coincidence.
     *
     * All three are pure, allocate only their result, and never throw except for
     * `std::bad_alloc` from that result.
     *
     * **The fourth entry is a selector, not a transform (ticket #2084).** A DOCTYPE
     * `ExternalID`'s `PubidLiteral`/`SystemLiteral` has **no** escape mechanism -- this
     * runtime's own DOCTYPE reader (`ParseDoctype`'s `readQuoted` in `XmlDocument.cpp`)
     * ends a literal at the first occurrence of its opening quote and never un-escapes
     * anything, so writing `&quot;` inside one would store the six literal characters
     * rather than a quote. Self-healing is therefore impossible for this input class; the
     * only well-formed choices are to re-delimit or to reject. `SelectExternalIdDelimiter`
     * makes that choice and stays pure and non-throwing like its three neighbours, leaving
     * the rejection to its caller.
     */

    /**
     * @brief Protects a comment's content so it cannot terminate its own `<!--  -->` wrapper.
     *
     * A space is inserted after any `-` that is followed by another `-`, and after a trailing
     * `-` that would otherwise abut the closing `-->`.
     */
    [[nodiscard]] inline std::string SanitizeCommentText(const std::string& text) {
        std::string out;
        out.reserve(text.size());
        for (size_t i = 0; i < text.size(); ++i) {
            out += text[i];
            if (text[i] == '-' && (i + 1 == text.size() || text[i + 1] == '-'))
                out += ' ';
        }
        return out;
    }

    /**
     * @brief Protects a processing instruction's data so it cannot terminate its own `<? ?>`
     * wrapper.
     *
     * A space is inserted between any `?` and a following `>`. A trailing `?` needs no
     * protection: the emitted `?>` terminator makes the first `?>` in `…??>` land on the
     * terminator, so the data still reads back as `…?`.
     */
    [[nodiscard]] inline std::string SanitizeProcessingInstructionText(const std::string& text) {
        std::string out;
        out.reserve(text.size());
        for (size_t i = 0; i < text.size(); ++i) {
            out += text[i];
            if (text[i] == '?' && i + 1 < text.size() && text[i + 1] == '>')
                out += ' ';
        }
        return out;
    }

    /**
     * @brief Splits a CDATA value across adjacent sections wherever it contains `]]>`.
     *
     * The section is closed immediately before the embedded terminator and reopened
     * immediately after its first character, so `left]]>right` becomes
     * `left]]]]><![CDATA[>right` inside the wrapper — two adjacent CDATA sections whose
     * concatenated content is the original value.
     */
    [[nodiscard]] inline std::string SanitizeCDataText(const std::string& text) {
        std::string out;
        out.reserve(text.size());
        size_t i = 0;
        while (i < text.size()) {
            if (i + 2 < text.size() && text[i] == ']' && text[i + 1] == ']' && text[i + 2] == '>') {
                out += "]]]]><![CDATA[>";
                i += 3;
            } else {
                out += text[i];
                ++i;
            }
        }
        return out;
    }

    /**
     * @brief Chooses the quote character that can delimit @p literal inside a DOCTYPE
     * `ExternalID`, or reports that no such character exists.
     *
     * XML gives a `SystemLiteral`/`PubidLiteral` exactly two possible delimiters and no way
     * to escape either one inside the literal, so a value is representable only if it omits
     * at least one of them. This runtime's own DOCTYPE reader accepts both delimiters
     * (`XmlDocument.cpp`'s `readQuoted` tests for `"` and `'` and then scans to the matching
     * quote), which is what makes re-delimiting a read-back-preserving repair rather than a
     * guess.
     *
     * `"` is preferred so that every value not containing one keeps its existing byte-for-byte
     * output.
     *
     * @return `'"'` when @p literal contains no double quote; `'\''` when it contains a double
     *         quote but no apostrophe; `'\0'` when it contains both and is therefore
     *         unrepresentable.
     */
    [[nodiscard]] inline char SelectExternalIdDelimiter(const std::string& literal) {
        if (literal.find('"') == std::string::npos) return '"';
        return literal.find('\'') == std::string::npos ? '\'' : '\0';
    }

    /**
     * @brief Reports whether @p literal would terminate the DOCTYPE declaration that contains
     * it, regardless of which delimiter quotes it.
     *
     * This runtime represents a DOCTYPE as a single `>`-terminated node (tinyxml2 has no
     * DOCTYPE type, so `XmlDocument` stores one as an `XMLUnknown` and `ParseDoctype` reads it
     * back), and that scan is **not** quote-aware. A `>` inside an `ExternalID` literal
     * therefore ends the declaration early even though XML itself permits it there, leaving
     * the remainder of the declaration to be re-read as document markup.
     *
     * Rejecting it costs nothing legitimate: a system identifier is a URI reference, and
     * RFC 3986 excludes `>` from every URI production -- a real one carries `%3E` instead.
     * `XmlConvert::VerifyPublicId` already rejects `>` for the public identifier, because `>`
     * is not a `PubidChar`, so only the system identifier needs this test.
     *
     * The same limitation applies to a DOCTYPE internal subset, where it is **not** repairable
     * this way: an ordinary subset such as `<!ENTITY a "b">` contains a `>` that XML requires,
     * so the declaration is lost on read-back with no well-formed alternative spelling. That
     * half is tracked separately and is documented on `XmlDocumentType`.
     */
    [[nodiscard]] inline bool ExternalIdLiteralTerminatesDeclaration(const std::string& literal) {
        return literal.find('>') != std::string::npos;
    }

    /**
     * @brief Reports whether @p text contains a NUL byte, which no XML document can carry.
     *
     * Every writer body in this runtime hands `std::string::c_str()` to the tinyxml2
     * substrate, whose API is `const char*`, so the byte count is lost at that boundary and a
     * value is silently truncated at its first NUL. That is a **length** boundary, not a
     * missing validator, which is why it gets its own predicate rather than reusing
     * `XmlConvert::VerifyXmlChars`.
     *
     * NUL is the one character for which rejection is not a policy choice. The XML `Char`
     * production excludes U+0000 outright and a character reference must itself match `Char`,
     * so there is **no** spelling -- literal or escaped -- that carries a NUL through a
     * document; "write it in full" is not an implementable branch. This runtime's own parser
     * agrees, rejecting an embedded NUL in text with `XML_ERROR_PARSING_TEXT`.
     *
     * Other characters outside `Char` (0x01, 0x0C, ...) were a **different** question, settled by
     * ticket #2349 -- see FindNonCharCodePoint below.
     */
    [[nodiscard]] inline bool ContainsNul(const std::string& text) {
        return text.find('\0') != std::string::npos;
    }

    /**
     * @brief Finds the first code point in @p text outside the XML 1.0 `Char` production.
     *
     * Ticket #2349 (2026-08-18), split from #2085. Characters outside `Char` other than NUL --
     * `0x01`, `0x0C`, `0x0E`-`0x1F`, `U+FFFE`, `U+FFFF` -- were **emitted raw** at every writer
     * content door and at every Xml.Linq direct door, so the emitted document was not
     * well-formed XML. Measured: 28 of the 29 non-`Char` bytes in `0x00`-`0x1F` went through.
     *
     * **THE TICKET RECORDED THIS AS A USER DECISION WITH FIVE PRICED OPTIONS, AND THE REFERENCE
     * COLLAPSES THEM TO ONE.** `XmlWriterSettings.CheckCharacters` defaults to `true`
     * (`XmlWriterSettings.cs:513`) and is enforced: `XmlEncodedRawTextWriter.InvalidXmlChar`
     * throws `XmlConvert.CreateInvalidCharException` when it is set and entitizes when it is not
     * (`:1630-1654`). So .NET rejects by default, and the flag is what turns that off.
     *
     * **Both of the ticket's pricing complications are dissolved rather than accepted.**
     *
     *   1. It priced enforcement on `XmlConvert::VerifyXmlChars`, which iterates `char` and so
     *      checks **bytes**: it accepts `U+FFFE`, `U+FFFF` and a lone surrogate encoded in UTF-8.
     *      Ticket **#2354** moved a code-point decoder into `Core.Base` earlier the same day, and
     *      both `modules/xml` and `modules/xml-linq` already depend on it -- so a code-point
     *      correct check costs one call and no new component edge. The ticket priced this when
     *      that decoder was not there.
     *   2. It priced option B as "not a same-shaped change on both sides", because the Xml.Linq
     *      direct doors take no settings. **.NET's do not either**: `XNode.GetXmlWriterSettings`
     *      constructs a default `XmlWriterSettings` and touches only `Indent` and
     *      `NamespaceHandling` (`XNode.cs:681-687`), inheriting `CheckCharacters = true`. So the
     *      Linq side needs no new settings channel; it checks, exactly as a default-settings
     *      writer does, and the two doors agree by construction.
     *
     * An **ill-formed UTF-8 sequence** is rejected too, and that is not an extension: .NET works
     * in UTF-16 and a lone surrogate is outside `Char` there as surely as it is here.
     *
     * @return The offending code point's byte offset, or `std::string::npos` when every code
     *         point is a `Char`.
     */
    [[nodiscard]] inline std::size_t FindNonCharCodePoint(const std::string& text) {
        std::size_t i = 0;
        while (i < text.size()) {
            std::uint32_t cp  = 0;
            std::size_t   len = 0;
            if (!System::detail::TryDecodeUtf8Scalar(text, i, cp, len)) return i;
            // Char ::= #x9 | #xA | #xD | [#x20-#xD7FF] | [#xE000-#xFFFD] | [#x10000-#x10FFFF].
            // The decoder has already refused a surrogate and anything above U+10FFFF, so what is
            // left to exclude is the C0 controls other than tab/LF/CR, and the two non-characters
            // at the end of the BMP.
            const bool isChar = cp == 0x09 || cp == 0x0A || cp == 0x0D ||
                                (cp >= 0x20 && cp <= 0xD7FF) ||
                                (cp >= 0xE000 && cp <= 0xFFFD) ||
                                (cp >= 0x10000 && cp <= 0x10FFFF);
            if (!isChar) return i;
            i += len;
        }
        return std::string::npos;
    }

    /** @brief `.NET`'s `SR.Xml_InvalidCharacter`, formatted for @p codePoint. */
    [[nodiscard]] inline std::string InvalidCharacterMessage(std::uint32_t codePoint) {
        char hex[16] = {};
        std::snprintf(hex, sizeof(hex), "0x%02X", codePoint);
        std::string rendered;
        if (codePoint >= 0x20 && codePoint < 0x7F) rendered.push_back(static_cast<char>(codePoint));
        return "'" + rendered + "', hexadecimal value " + hex + ", is an invalid character.";
    }

} // namespace System::Xml::detail
