// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/Text/Json/JsonCommentHandling.hpp"
#include "System/Text/Json/JsonDocumentOptions.hpp"
#include "System/Text/Json/JsonException.hpp"
#include "System/Text/Json/detail/JsonParseGuard.hpp"
#include "nlohmann/json.hpp"

namespace System::Text::Json::detail {

/**
 * @brief Rejects a document nested deeper than @p maxDepth.
 *
 * Verified against `Utf8JsonReader.cs` (`ReadFirstToken`/`ConsumeNextTokenOrRollback`): real .NET
 * throws when about to open a container while already at the configured `MaxDepth` — i.e. at most
 * `MaxDepth` levels of nested containers are allowed. @p currentDepth is the nesting level of the
 * container being examined (1 for a root-level object or array).
 *
 * The recursion is **bounded by @p maxDepth** because it throws as soon as the depth is exceeded,
 * so it cannot itself overflow the stack.
 */
inline void CheckMaxDepth(const nlohmann::ordered_json& node, SharpRuntime::intcs currentDepth,
                          SharpRuntime::intcs maxDepth) {
    if (!node.is_object() && !node.is_array()) return;
    if (currentDepth > maxDepth) {
        throw JsonException("The maximum configured depth of " + std::to_string(maxDepth) +
                            " has been exceeded. Cannot read next JSON " +
                            (node.is_object() ? "object" : "array") + ".");
    }
    for (const auto& child : node) CheckMaxDepth(child, currentDepth + 1, maxDepth);
}

/**
 * @brief The single **document**-parse core: validate the options, reject an embedded NUL, parse,
 * and enforce the configured depth.
 *
 * Tickets #2116 and #2121 (`docs/SystemTextJsonNamespaceReviewPlan.md` §20.11–§20.13).
 * `JsonDocument::Parse` and `JsonSerializer::Deserialize` were two independent implementations of
 * "parse a document under document options", and they had drifted: the serializer discarded its
 * options entirely (SR-AUD-330) **and** never reached `RejectEmbeddedNul`, so an embedded NUL still
 * truncated a document through that door after #2112 had closed the others. Both now call this.
 *
 * @note **`nlohmann`'s own exception is deliberately allowed to escape**, so each caller keeps its
 * existing message wording — the same discipline #2111 applied when it widened three catch sites
 * to the base exception. Only `JsonException` (from the NUL guard and the depth check) is raised
 * here, and both callers want identical wording for those.
 *
 * @note **Not every parse door in the module belongs here, and that is deliberate.**
 * `JsonNode::Parse` is *not* routed through this function: ticket #1897 made it iterative and
 * deliberately applies **no depth bound**, a documented, `CLAUDE.md`-pinned deviation that only
 * the still-unapproved option A may reopen. `Utf8JsonWriter::WriteRawValue` validates a *fragment*
 * rather than a document under document options. Both keep their own `RejectEmbeddedNul` call and
 * their own base-exception catch.
 */
inline nlohmann::ordered_json ParseDocumentText(const std::string& json,
                                                const JsonDocumentOptions& options) {
    options.Validate();
    RejectEmbeddedNul(json);
    auto parsed = nlohmann::ordered_json::parse(
        json, /*callback=*/nullptr, /*allow_exceptions=*/true,
        /*ignore_comments=*/options.CommentHandling != JsonCommentHandling::Disallow);
    CheckMaxDepth(parsed, 1,
                  options.MaxDepth == 0 ? JsonDocumentOptions::DefaultMaxDepth : options.MaxDepth);
    return parsed;
}

} // namespace System::Text::Json::detail
