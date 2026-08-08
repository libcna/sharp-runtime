// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Net/Http/Headers/EntityTagHeaderValue.hpp"
#include "System/FormatException.hpp"
#include "System/HashCode.hpp"
#include "System/Net/detail/ProtocolFieldValidation.hpp"
#include <algorithm>
#include <cctype>

namespace System::Net::Http::Headers {

    namespace {
        // #2124 (SR-AUD-319): the entity-tag is stored WITH its quotes and ToString() emits it
        // verbatim, so a CR/LF/NUL inside the quoted-string became an injected header field. The
        // quoting grammar below cannot see that -- it only tracks quotes and backslashes -- so the
        // family's single predicate runs first. This one helper also closes
        // EntityTagHeaderValue::Parse/TryParse and RangeConditionHeaderValue's string constructor,
        // which both funnel through here.
        bool isValidQuotedString(const std::string& value) {
            if (System::Net::detail::ContainsProtocolFieldTerminator(value)) return false;
            if (value.size() < 2 || value.front() != '"' || value.back() != '"') return false;
            for (size_t i = 1; i + 1 < value.size(); ) {
                if (value[i] == '\\') {
                    if (i + 1 >= value.size() - 1) return false;
                    i += 2;
                } else if (value[i] == '"') {
                    return false;
                } else {
                    ++i;
                }
            }
            return true;
        }
    }

    EntityTagHeaderValue::EntityTagHeaderValue(const std::string& tag) : EntityTagHeaderValue(tag, false) {}

    EntityTagHeaderValue::EntityTagHeaderValue(const std::string& tag, bool isWeak) : tag_(tag), isWeak_(isWeak) {
        // #2124: the terminator case is separated out only so the message does not echo the
        // offending text back to a caller that is very likely to log it.
        if (System::Net::detail::ContainsProtocolFieldTerminator(tag)) {
            throw System::FormatException("The entity-tag contains invalid CR, LF, or NUL characters.");
        }
        if (!isValidQuotedString(tag)) {
            throw System::FormatException("The entity-tag is not a valid quoted-string: " + tag);
        }
    }

    SharpRuntime::intcs EntityTagHeaderValue::GetHashCode() const {
        return System::HashCode::Combine(tag_, isWeak_);
    }

    const EntityTagHeaderValue& EntityTagHeaderValue::Any() {
        static const EntityTagHeaderValue any("*", false, false);
        return any;
    }

    bool EntityTagHeaderValue::TryParse(const std::string& input, EntityTagHeaderValue& parsedValue) {
        size_t start = input.find_first_not_of(" \t");
        if (start == std::string::npos) return false;
        size_t end = input.find_last_not_of(" \t");
        std::string trimmed = input.substr(start, end - start + 1);

        if (trimmed == "*") {
            parsedValue = Any();
            return true;
        }

        bool isWeak = false;
        std::string tag = trimmed;
        if (tag.size() >= 2 && (tag[0] == 'W' || tag[0] == 'w') && tag[1] == '/') {
            isWeak = true;
            tag = tag.substr(2);
            // Real .NET's GetEntityTagLength skips whitespace between the "W/" prefix and the
            // quoted-string tag (HttpRuleParser.GetWhitespaceLength after the '/'); a naive
            // substr(2) with no whitespace skip would reject an otherwise-valid weak tag like
            // "W/ \"abc\"".
            size_t tagStart = tag.find_first_not_of(" \t");
            tag = (tagStart == std::string::npos) ? "" : tag.substr(tagStart);
        }

        if (!isValidQuotedString(tag)) return false;

        parsedValue = EntityTagHeaderValue(tag, isWeak, false);
        return true;
    }

    EntityTagHeaderValue EntityTagHeaderValue::Parse(const std::string& input) {
        EntityTagHeaderValue result = Any();
        if (!TryParse(input, result)) {
            throw System::FormatException("The entity-tag is not valid: " + input);
        }
        return result;
    }

} // namespace System::Net::Http::Headers
