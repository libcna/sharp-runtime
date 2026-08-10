// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

// IMPLEMENTATION-ONLY header, beside the sources rather than under include/, so it never becomes
// part of this component's public surface. Same placement and plain relative include as
// `HttpDateParser.hpp` and as `modules/xml/src`'s `XPathAstInternal.hpp`.
//
// Ticket #2126 (SR-AUD-320, cause NH-H, docs/SystemNetHttpHeadersNamespaceReviewPlan.md §4.2).
//
// The module carried **seven** list splitters and six of them toggled a `bool` on every `"` with
// no notion of a quoted-pair, so a legal RFC 9110 parameter whose quoted-string contains an
// **escaped quote** followed by the delimiter was split in the middle of its own value and then
// rejected:
//
//     text/plain; p="a\";b"      -> rejected
//     text/plain; p="a;b"       -> accepted   <- the control
//
// The control is what makes the premise precise, and §4.2 already sharpened it: the splitters are
// not quote-blind, they are **escape**-blind. This is therefore a **widening** — text that is
// valid per RFC 9110 starts being accepted — not a narrowing.
//
// **The seventh splitter is worse than escape-blind, and no document says so.**
// `NameValueWithParametersHeaderValue::TryParse` splits on a bare `input.find(';')` with no
// quote tracking at all, so even the control case — an *unescaped* `;` inside a quoted parameter —
// was split there. It is folded into this one body with the rest.
//
// The repair target already existed one file away: `NameValueHeaderValue::isValidQuotedString`
// consumes quoted-pairs correctly and always has. The module contained both a correct scanner and
// six incorrect ones — the same shape as #2103, #2101, #2111 and #2114.

#include <string>
#include <vector>

namespace System::Net::Http::Headers::detail {

    /**
     * @brief Splits @p input on @p delim, ignoring delimiters inside a quoted-string.
     *
     * A backslash inside a quoted-string escapes the next character (RFC 9110 `quoted-pair`), so
     * `\"` does **not** end the string. Outside a quoted-string a backslash is an ordinary
     * character, because `quoted-pair` occurs only inside `quoted-string` and `comment`.
     *
     * The splitter deliberately does **not** validate: an unterminated quote or a trailing
     * backslash produces one final segment that the caller's own grammar check then rejects,
     * exactly as before #2126. Splitting and validating stay separate.
     *
     * @return At least one segment; segments are returned with their quoting intact and are not
     * trimmed, matching every call site's existing expectation.
     */
    [[nodiscard]] inline std::vector<std::string> SplitTopLevel(const std::string& input, char delim) {
        std::vector<std::string> parts;
        std::string current;
        bool inQuotes = false;
        for (size_t i = 0; i < input.size(); ++i) {
            const char c = input[i];
            if (inQuotes && c == '\\' && i + 1 < input.size()) {
                // A quoted-pair: the escapee is consumed verbatim and can be a quote or the
                // delimiter itself without ending the string or splitting the segment.
                current += c;
                current += input[i + 1];
                ++i;
                continue;
            }
            if (c == '"') {
                inQuotes = !inQuotes;
                current += c;
            } else if (c == delim && !inQuotes) {
                parts.push_back(current);
                current.clear();
            } else {
                current += c;
            }
        }
        parts.push_back(current);
        return parts;
    }

} // namespace System::Net::Http::Headers::detail
