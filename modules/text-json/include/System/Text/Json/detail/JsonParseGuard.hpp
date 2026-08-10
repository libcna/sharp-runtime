// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>

#include "System/Text/Json/JsonException.hpp"

namespace System::Text::Json::detail {

/**
 * @brief Rejects document text containing an embedded NUL, before it reaches the parser.
 *
 * Ticket #2112 (cause TJ-G, `docs/SystemTextJsonNamespaceReviewPlan.md` §7.2). The vendored
 * parser treats a NUL byte at a value boundary as end-of-input, so
 * `{"a":1}` `NUL` `{"b":2}` was **accepted** and silently parsed as `{"a":1}` — the second
 * object discarded without a diagnostic. **The control that makes this a defect rather than
 * general leniency**: the identical document with a space instead of the NUL *is* rejected as
 * trailing junk. Two parties parsing the same bytes could therefore disagree about what the
 * document says.
 *
 * Every public entry point that hands caller text to the parser goes through this one function,
 * so the three doors cannot drift apart.
 *
 * The rejection — a `JsonException` — is **this port's choice**, recorded as such in the plan's
 * §17: the reference tree is absent, so no evidence pins what .NET raises here. It is taken on
 * the strength of the space control, not on a guess about .NET.
 */
inline void RejectEmbeddedNul(const std::string& json) {
    const std::string::size_type nul = json.find('\0');
    if (nul != std::string::npos) {
        throw JsonException(
            "'The JSON document contains an embedded NUL character at position " +
            std::to_string(nul) +
            ", which would silently truncate the document. Remove it or escape it as \\u0000.'.");
    }
}

} // namespace System::Text::Json::detail
