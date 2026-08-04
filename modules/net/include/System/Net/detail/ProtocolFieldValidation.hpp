// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <string>

namespace System::Net::detail {

/**
 * @brief The three characters that terminate a field in an HTTP/1.1 or MIME frame.
 *
 * Carriage return (`\r`) and line feed (`\n`) end a request line, a status line, a header
 * field and a MIME part header; NUL (`\0`) truncates every C-string-shaped consumer
 * downstream of the wire. None of them may appear inside a value that this runtime
 * concatenates into such a frame.
 *
 * This deliberately does **not** reject the other C0 controls, `"`, `;` or any other
 * character: they do not terminate a field, and widening the rejected set beyond what the
 * frame grammar requires would narrow the accepted input further than the evidence in this
 * repository supports (see `docs/SystemNetHttpNamespaceReviewPlan.md` §15). A space and a
 * horizontal tab are legal inside a header value and stay accepted.
 *
 * @par Why this lives in `System::Net` rather than in `System::Net::Http`
 * Ticket #2063 established the rule and applied it to ten `System::Net::Http` doors. Ticket
 * #2089 then found the identical defect on a second protocol — the RFC 6455 WebSocket
 * upgrade, which **is** an HTTP/1.1 request and is framed by the same three characters. The
 * predicate therefore has exactly one body, here, in the component both protocol modules
 * already depend on; `System::Net::Http::detail::ContainsProtocolControlCharacter` forwards
 * to it under its original name so no `System::Net::Http` call site changed.
 *
 * Sharing the predicate is **not** the CCF-021 promotion: that family is minted from the
 * `Net.Http.Headers` review, which still holds two of its four findings
 * (`docs/SystemNetWebSocketsNamespaceReviewPlan.md` §8.2). This is only the mechanism that
 * keeps a second, divergent copy of the rule from being written in the meantime.
 *
 * @par The companion rule that has no code
 * A door that rejects on this predicate must **not** echo the offending text into its
 * exception message. The text is attacker-controlled by construction and exception messages
 * are very likely to be logged; echoing a CR/LF-bearing value into a log re-creates exactly
 * the injection the rejection exists to prevent.
 *
 * @param text The candidate field text.
 * @return true if @p text contains CR, LF or NUL.
 */
[[nodiscard]] inline bool ContainsProtocolFieldTerminator(const std::string& text) noexcept {
    return text.find('\r') != std::string::npos
        || text.find('\n') != std::string::npos
        || text.find('\0') != std::string::npos;
}

} // namespace System::Net::detail
