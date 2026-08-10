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

} // namespace System::Xml::detail
