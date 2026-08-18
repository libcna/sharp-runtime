// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include <set>
#include <vector>
#include <cstddef>
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
/**
 * @brief Removes a comma that immediately precedes a closing `]` or `}` (ticket #2115).
 *
 * `AllowTrailingCommas = true` was **validated, stored and never consulted**: `[1,]` was rejected
 * whichever way the flag was set. nlohmann/json has no trailing-comma mode at all, so the text is
 * preprocessed -- and **only when the flag is set**, so the default path is byte-identical and
 * cannot regress.
 *
 * This is a scanner rather than a search, because a `,` inside a string literal is data and a `,`
 * inside a comment is nothing. Both are tracked. Comments are skipped only when the caller allows
 * them, so a `,]` hidden inside a block comment is left alone under `JsonCommentHandling::Disallow`
 * exactly as nlohmann would see it.
 *
 * @note It removes a comma before a closer and **nothing else**. `[1,,2]`, `[,1]` and `{,}` stay
 *       malformed, because .NET's `AllowTrailingCommas` permits exactly one trailing comma per
 *       container and not a missing element -- and a scanner that dropped every comma next to a
 *       bracket would silently accept documents .NET rejects.
 */
inline std::string StripTrailingCommas(const std::string& json, bool allowComments) {
    std::string out;
    out.reserve(json.size());

    const auto skipInsignificant = [&](std::size_t i) {
        // Returns the index of the next significant character at or after i.
        while (i < json.size()) {
            const char c = json[i];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') { ++i; continue; }
            if (allowComments && c == '/' && i + 1 < json.size()) {
                if (json[i + 1] == '/') {
                    i += 2;
                    while (i < json.size() && json[i] != '\n') ++i;
                    continue;
                }
                if (json[i + 1] == '*') {
                    i += 2;
                    while (i + 1 < json.size() && !(json[i] == '*' && json[i + 1] == '/')) ++i;
                    i = (i + 1 < json.size()) ? i + 2 : json.size();
                    continue;
                }
            }
            break;
        }
        return i;
    };

    for (std::size_t i = 0; i < json.size();) {
        const char c = json[i];

        if (c == '"') {                       // a string literal: copy it whole, escapes included
            out += c;
            ++i;
            while (i < json.size()) {
                out += json[i];
                if (json[i] == '\\' && i + 1 < json.size()) { out += json[i + 1]; i += 2; continue; }
                if (json[i] == '"') { ++i; break; }
                ++i;
            }
            continue;
        }

        if (allowComments && c == '/' && i + 1 < json.size() &&
            (json[i + 1] == '/' || json[i + 1] == '*')) {
            const std::size_t end = skipInsignificant(i);
            out.append(json, i, end - i);     // comments are copied through, not consumed
            i = end;
            continue;
        }

        if (c == ',') {
            const std::size_t next = skipInsignificant(i + 1);
            if (next < json.size() && (json[next] == ']' || json[next] == '}')) {
                // Drop the comma; keep everything between it and the closer, so a comment sitting
                // there survives.
                out.append(json, i + 1, next - (i + 1));
                i = next;
                continue;
            }
        }

        out += c;
        ++i;
    }
    return out;
}

/**
 * @brief Builds nlohmann's parser callback that rejects a duplicate property (ticket #2115).
 *
 * `AllowDuplicateProperties = false` was **validated, stored and never consulted**:
 * `{"x":1,"x":2}` was accepted as `x = 2` whichever way the flag was set, because nlohmann
 * silently overwrites. The callback is the one place a key can be seen *before* the overwrite
 * happens.
 *
 * The message is .NET's `SR.DuplicatePropertiesNotAllowed_NameSpan`, and the **15-character
 * truncation is .NET's too** (`ThrowHelper.Serialization.cs:361-364,373`) -- a document can carry
 * an arbitrarily long key, and echoing it whole into an exception message is what the reference
 * declines to do.
 *
 * @note The callback must always return `true`. Returning `false` tells nlohmann to DISCARD the
 *       element, which would silently drop data rather than diagnose it.
 */
inline nlohmann::ordered_json::parser_callback_t MakeDuplicateKeyRejector(
    std::shared_ptr<std::vector<std::set<std::string>>> seen) {
    return [seen](int /*depth*/, nlohmann::ordered_json::parse_event_t event,
                  nlohmann::ordered_json& parsed) -> bool {
        using Event = nlohmann::ordered_json::parse_event_t;
        if (event == Event::object_start) {
            seen->emplace_back();
        } else if (event == Event::object_end) {
            if (!seen->empty()) seen->pop_back();
        } else if (event == Event::key) {
            std::string name = parsed.get<std::string>();
            if (!seen->empty() && !seen->back().insert(name).second) {
                if (name.size() > 15) name = name.substr(0, 15) + "...";
                throw JsonException("Duplicate property '" + name +
                                    "' encountered during deserialization.");
            }
        }
        return true;
    };
}

inline nlohmann::ordered_json ParseDocumentText(const std::string& json,
                                                const JsonDocumentOptions& options) {
    std::string scratch;   // owns the rewritten text when AllowTrailingCommas is set
    options.Validate();
    RejectEmbeddedNul(json);

    const bool allowComments = options.CommentHandling != JsonCommentHandling::Disallow;

    // #2115: both flags were validated, stored and NEVER CONSULTED. Each is honoured only when
    // set, so the default path is byte-identical to what it was.
    const std::string& text = options.AllowTrailingCommas
                                  ? (scratch = StripTrailingCommas(json, allowComments))
                                  : json;

    nlohmann::ordered_json::parser_callback_t callback = nullptr;
    std::shared_ptr<std::vector<std::set<std::string>>> seenKeys;
    if (!options.AllowDuplicateProperties) {
        seenKeys = std::make_shared<std::vector<std::set<std::string>>>();
        callback = MakeDuplicateKeyRejector(seenKeys);
    }

    auto parsed = nlohmann::ordered_json::parse(
        text, callback, /*allow_exceptions=*/true,
        /*ignore_comments=*/allowComments);
    CheckMaxDepth(parsed, 1,
                  options.MaxDepth == 0 ? JsonDocumentOptions::DefaultMaxDepth : options.MaxDepth);
    return parsed;
}

} // namespace System::Text::Json::detail
