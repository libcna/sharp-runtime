// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>

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

} // namespace System::Xml::detail
