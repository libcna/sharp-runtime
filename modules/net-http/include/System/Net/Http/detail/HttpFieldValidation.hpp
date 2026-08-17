// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <unordered_map>

#include <string>

#include "System/ArgumentException.hpp"
#include "System/FormatException.hpp"
#include "System/Net/detail/ProtocolFieldValidation.hpp"

namespace System::Net::Http::detail {

/**
 * @brief The three characters that terminate a field in an HTTP/1.1 or MIME frame.
 *
 * Carriage return (`\r`) and line feed (`\n`) end a request line, a status line, a header
 * field and a MIME part header; NUL (`\0`) truncates every C-string-shaped consumer
 * downstream of the wire. None of them may appear inside a value that this port
 * concatenates into such a frame.
 *
 * This deliberately does **not** reject the other C0 controls, `"`, `;` or any other
 * character: they do not terminate a field, and widening the rejected set beyond what the
 * frame grammar requires would narrow the accepted input further than the evidence in this
 * repository supports (see `docs/SystemNetHttpNamespaceReviewPlan.md` §15). A space and a
 * horizontal tab are legal inside a header value and stay accepted.
 *
 * @note Ticket #2089 found the identical defect on the RFC 6455 WebSocket upgrade — which is
 *       an HTTP/1.1 request framed by the same three characters — so the **body** of this
 *       predicate moved down to `System::Net::detail::ContainsProtocolFieldTerminator`, in
 *       the component both protocol modules already depend on. This name is kept as a
 *       forwarder so that no `System::Net::Http` call site, exception type or message
 *       changed, and so that the repository contains exactly one copy of the rule rather
 *       than a second, quietly diverging one.
 */
[[nodiscard]] inline bool ContainsProtocolControlCharacter(const std::string& text) noexcept {
    return System::Net::detail::ContainsProtocolFieldTerminator(text);
}

/**
 * @brief Rejects a protocol field whose text would terminate the frame it is written into.
 *
 * Used by the public doors that accept a *protocol field* — a header name, a header value,
 * a reason phrase, a media type, a charset. The failure is a malformed value rather than a
 * malformed argument, so it is a @c System::FormatException, matching the type this port
 * already throws from `HttpClient::parseUrl` (`System::UriFormatException` **is** a
 * `FormatException`).
 *
 * @param text      The candidate field text.
 * @param fieldName The name of the field, quoted into the message so a caller can tell
 *                  which door rejected the value.
 *
 * @note The offending text is deliberately **not** echoed into the message. It is
 *       attacker-controlled by construction and the message is very likely to be logged;
 *       echoing a CR/LF-bearing value into a log re-creates exactly the injection the
 *       rejection exists to prevent.
 *
 * @throws System::FormatException if @p text contains CR, LF or NUL.
 */
inline void ThrowIfControlCharacter(const std::string& text, const char* fieldName) {
    if (ContainsProtocolControlCharacter(text)) {
        throw System::FormatException(
            std::string("System::Net::Http: the value of '") + fieldName +
            "' must not contain a carriage return, a line feed or a NUL character.");
    }
}

/**
 * @brief The @c System::ArgumentException-flavoured counterpart of ThrowIfControlCharacter.
 *
 * Used by the constructor and `Add` doors whose *other* validation of the same parameter
 * already throws `System::ArgumentException` — `MultipartContent`'s subtype (whose boundary
 * sibling throws `ArgumentException`) and `MultipartFormDataContent::Add`'s name and file
 * name (which already call `ArgumentException::ThrowIfNullOrWhiteSpace`). Reporting one
 * defect in a parameter as an argument error and another defect in the *same* parameter as
 * a format error would be arbitrary.
 *
 * @param text      The candidate parameter text.
 * @param paramName The parameter name, carried on the exception.
 *
 * @throws System::ArgumentException if @p text contains CR, LF or NUL.
 */
inline void ThrowIfControlCharacterArgument(const std::string& text, const char* paramName) {
    if (ContainsProtocolControlCharacter(text)) {
        throw System::ArgumentException(
            std::string("Value must not contain a carriage return, a line feed or a NUL "
                        "character."),
            std::string(paramName));
    }
}


/**
 * @brief Compares two HTTP field names the way RFC 9110 §5.1 says they compare: case-insensitively.
 *
 * Ticket #2068. `HttpRequestMessage`, `HttpResponseMessage` and `HttpClient`'s default headers all
 * store their fields in a plain `std::unordered_map<std::string, std::string>`, whose key equality
 * is byte equality. Measured, that made `Content-Type` and `content-type` **two different
 * headers**: setting both left two entries and both went on the wire, and
 * `getHeader("content-type")` returned `""` after `setHeader("Content-Type", …)`.
 *
 * .NET compares field names case-insensitively everywhere — `HeaderDescriptor` is built on
 * `StringComparer.OrdinalIgnoreCase` — so this is parity, not a house rule.
 *
 * @note The **map type stays exactly as it was.** Giving it a case-insensitive hash and equality
 *       would change `getHeadersProperty()`'s return type, which is a public source break for
 *       every caller that names it, for no observable gain: the collections hold a handful of
 *       entries, so a linear case-insensitive scan is free in practice and free of SA-2's
 *       machinery in principle.
 */
inline bool FieldNamesMatch(const std::string& a, const std::string& b) noexcept {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        const unsigned char lhs = static_cast<unsigned char>(a[i]);
        const unsigned char rhs = static_cast<unsigned char>(b[i]);
        // Deliberately ASCII-only and locale-free. A field name is a token (RFC 9110 §5.6.2),
        // which is ASCII by definition, and std::tolower with a non-"C" locale would fold bytes
        // a token can never contain.
        const unsigned char lo = (lhs >= 'A' && lhs <= 'Z') ? static_cast<unsigned char>(lhs + 32) : lhs;
        const unsigned char ro = (rhs >= 'A' && rhs <= 'Z') ? static_cast<unsigned char>(rhs + 32) : rhs;
        if (lo != ro) return false;
    }
    return true;
}

/**
 * @brief Stores @p value under @p name, replacing any entry whose name matches case-insensitively.
 *
 * Ticket #2068. Without the erase, `setHeader("Content-Type", …)` followed by
 * `setHeader("content-type", …)` left both entries in the map and emitted both on the wire.
 * The **caller's** spelling of the name is kept, matching .NET, which preserves the string it
 * was given rather than canonicalising it.
 */
inline void SetFieldReplacingCaseVariants(std::unordered_map<std::string, std::string>& fields,
                                          const std::string& name, const std::string& value) {
    for (auto it = fields.begin(); it != fields.end(); ++it) {
        if (FieldNamesMatch(it->first, name)) {
            fields.erase(it);
            break;   // at most one can match, precisely because this function is the only door
        }
    }
    fields[name] = value;
}

/** @brief Case-insensitive lookup; returns an empty string when the field is absent. */
[[nodiscard]] inline std::string GetFieldIgnoringCase(
    const std::unordered_map<std::string, std::string>& fields, const std::string& name) {
    for (const auto& [key, value] : fields) {
        if (FieldNamesMatch(key, name)) return value;
    }
    return "";
}

/** @brief Whether any field in @p fields has the name @p name, compared case-insensitively. */
[[nodiscard]] inline bool HasField(const std::unordered_map<std::string, std::string>& fields,
                                   const std::string& name) {
    for (const auto& [key, value] : fields) {
        (void)value;
        if (FieldNamesMatch(key, name)) return true;
    }
    return false;
}

} // namespace System::Net::Http::detail
