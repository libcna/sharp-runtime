// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Net/Http/Headers/HttpHeaders.hpp"
#include <cctype>
#include <cstring>
#include "System/ArgumentException.hpp"
#include "System/FormatException.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/Net/detail/ProtocolFieldValidation.hpp"
#include <algorithm>
#include <cctype>
#include <string_view>

namespace System::Net::Http::Headers {

    namespace {
        bool isHttpTokenChar(unsigned char c) {
            static constexpr std::string_view extras = "!#$%&'*+-.^_`|~";
            return c != 0 && (std::isalnum(c) || extras.find(static_cast<char>(c)) != std::string_view::npos);
        }

        bool isToken(const std::string& s) {
            return !s.empty() && std::all_of(s.begin(), s.end(), [](char c) { return isHttpTokenChar(static_cast<unsigned char>(c)); });
        }

        // Real .NET's System.Net.Http.Headers.HttpHeaders validates values via
        // HttpRuleParser.ContainsNewLineOrNull -- a FLAT rejection of any '\r', '\n', or '\0'
        // anywhere in the value, citing RFC 9110 section 5.5-5 ("Field values containing CR, LF,
        // or NUL characters are invalid and dangerous"). This is deliberately stricter than the
        // separate, older System.Net.WebHeaderCollection.CheckBadHeaderValueChars (ported as
        // WebHeaderCollection::CheckBadHeaderValueChars in this codebase), which still tolerates
        // a "\r\n " / "\r\n\t" obs-fold sequence for HttpWebRequest-era backward compatibility --
        // real .NET's own comment on that older helper says so explicitly ("we want to be
        // permissive in what we accept... it would be a breaking change to reject this"). An
        // earlier version of this function copied that older, more permissive state machine
        // instead of this type's own stricter reference behavior, which would have let an
        // embedded "\r\n evil: header" obs-fold sequence through Add() -- a genuine HTTP header
        // injection vector once ToString() serializes it back out verbatim.
        //
        // Ticket #2124 routed this through System::Net::detail::ContainsProtocolFieldTerminator,
        // the family's single body, rather than leaving a hand-written fourth copy of the same
        // three characters in this module. The accepted/rejected set is byte-for-byte identical;
        // only the number of places the rule is written down changed.
        /**
         * The header names .NET treats as single-value, derived from the reference rather than
         * chosen (ticket #2128, SR-AUD-... none -- post-audit P1).
         *
         * `HttpHeaders.AddParsedValue` rejects a second value with
         * `FormatException(SR.net_http_headers_single_value_header)` whenever
         * `HeaderStoreItemInfo.CanAddParsedValue` says no (`HttpHeaders.cs:1107-1111`,
         * `:1365-1380`), and that answer is `parser.SupportsMultipleValues`. The set below is
         * every entry of `KnownHeaders.cs` whose parser is constructed with
         * `supportsMultipleValues: false` -- the explicitly single-value `GenericHeaderParser`
         * fields, plus `Int64NumberHeaderParser`, `Int32NumberHeaderParser`, `DateHeaderParser`,
         * `TimeSpanHeaderParser` and `UriHeaderParser`, all of which pass `false` to their base.
         *
         * Twenty-two names. `Content-Length` and `Host` are the two the finding named; the other
         * twenty come with them because they are the same rule, and leaving them out would make
         * this port's single-value set a subset nobody could justify.
         */
        bool isSingleValueHeader(const std::string& name) {
            static const char* const kSingleValue[] = {
                "Age", "Authorization", "Content-Disposition", "Content-Length",
                "Content-Location", "Content-Range", "Content-Type", "Date", "ETag", "Expires",
                "From", "Host", "If-Modified-Since", "If-Range", "If-Unmodified-Since",
                "Last-Modified", "Location", "Max-Forwards", "Proxy-Authorization", "Range",
                "Referer", "Retry-After",
            };
            for (const char* candidate : kSingleValue) {
                if (name.size() != std::strlen(candidate)) continue;
                bool equal = true;
                for (std::size_t i = 0; i < name.size(); ++i) {
                    if (std::tolower(static_cast<unsigned char>(name[i])) !=
                        std::tolower(static_cast<unsigned char>(candidate[i]))) { equal = false; break; }
                }
                if (equal) return true;
            }
            return false;
        }

        void checkValueChars(const std::string& value) {
            if (System::Net::detail::ContainsProtocolFieldTerminator(value)) {
                throw System::FormatException("The value contains invalid CR, LF, or NUL characters.");
            }
        }
    }

    // Ticket #2128. A second value for a single-value header is rejected, not comma-joined.
    // Before this, Add("Content-Length", "10") followed by Add("Content-Length", "20")
    // serialized `Content-Length: 10,20`, and two Hosts serialized `Host: a.example,b.example`.
    // A comma-joined Content-Length is precisely the message a request-smuggling chain relies on
    // two intermediaries disagreeing about, so the collection is where it has to stop.
    //
    // .NET does the same, and this is transcribed rather than invented: AddParsedValue throws
    // `FormatException(SR.Format(SR.net_http_headers_single_value_header, descriptor.Name))`
    // when CanAddParsedValue says no (HttpHeaders.cs:1107-1111), and that answer is the header
    // parser's SupportsMultipleValues. The message is "Cannot add value because header '{0}'
    // does not support multiple values." (System.Net.Http/src/Resources/Strings.resx:135).
    //
    // TryAddWithoutValidation deliberately does NOT get this check: .NET's bypasses the parser
    // entirely and stores the raw value, so the raw store can still hold two. "Without
    // validation" means what it says, and narrowing it would be a divergence.
    void HttpHeaders::Add(const std::string& name, const std::string& value) {
        System::ArgumentException::ThrowIfNullOrEmpty(name, "name");
        if (!isToken(name)) {
            throw System::FormatException("The header name is not a valid HTTP token: " + name);
        }
        checkValueChars(value);
        if (isSingleValueHeader(name) && Contains(name)) {
            throw System::FormatException("Cannot add value because header '" + name +
                                          "' does not support multiple values.");
        }
        headers_.Add(name, value);
    }

    void HttpHeaders::Add(const std::string& name, const std::vector<std::string>& values) {
        for (const auto& v : values) Add(name, v);
    }

    // Ticket #2123 (SR-AUD-322, cause NH-I, docs/SystemNetHttpHeadersNamespaceReviewPlan.md §4.4).
    // "Without validation" governs the VALUE, never the NAME. Before #2123 these two overloads
    // rejected only an empty name, so `TryAddWithoutValidation("X-Bad\r\nInjected: yes", "v")`
    // returned **true** and `ToString()` emitted
    //
    //     X-Bad\r\nInjected: yes: v\r\n
    //
    // -- two header fields where the caller supplied one, through a bool-returning API that
    // reported success. A NUL-bearing name and a name containing a space were accepted too.
    //
    // The repair target already existed in this file: `Add` above validates the name with
    // `isToken` and has done so all along. Both doors now share that one predicate, so they
    // cannot drift apart again. The VALUE deliberately stays unvalidated here -- that asymmetry
    // IS the contract, and it is what separates this door from `Add`.
    bool HttpHeaders::TryAddWithoutValidation(const std::string& name, const std::string& value) {
        if (!isToken(name)) return false;
        headers_.Add(name, value);
        return true;
    }

    bool HttpHeaders::TryAddWithoutValidation(const std::string& name, const std::vector<std::string>& values) {
        // The name is checked once, before anything is stored, so a rejected call leaves the
        // collection completely unchanged rather than partially populated.
        if (!isToken(name)) return false;
        for (const auto& v : values) headers_.Add(name, v);
        return true;
    }

    std::vector<std::string> HttpHeaders::GetValues(const std::string& name) const {
        std::vector<std::string> values;
        if (!TryGetValues(name, values)) {
            throw System::InvalidOperationException("The header '" + name + "' was not found.");
        }
        return values;
    }

    bool HttpHeaders::TryGetValues(const std::string& name, std::vector<std::string>& values) const {
        if (!Contains(name)) return false;
        values = headers_.GetValues(name);
        return true;
    }

    bool HttpHeaders::Contains(const std::string& name) const {
        return std::any_of(headers_.AllKeys().begin(), headers_.AllKeys().end(), [&](const std::string& k) {
            return k.size() == name.size() && std::equal(k.begin(), k.end(), name.begin(), [](unsigned char a, unsigned char b) {
                return std::tolower(a) == std::tolower(b);
            });
        });
    }

    bool HttpHeaders::Remove(const std::string& name) {
        if (!Contains(name)) return false;
        headers_.Remove(name);
        return true;
    }

    void HttpHeaders::Clear() {
        headers_.Clear();
    }

    SharpRuntime::intcs HttpHeaders::getCountProperty() const {
        return headers_.getCountProperty();
    }

    const std::vector<std::string>& HttpHeaders::getHeaderNamesProperty() const {
        return headers_.AllKeys();
    }

    std::string HttpHeaders::ToString() const {
        std::string result;
        for (const auto& key : headers_.AllKeys()) {
            result += key + ": " + headers_.Get(key) + "\r\n";
        }
        result += "\r\n";
        return result;
    }

    std::string HttpHeaders::getRawValue(const std::string& name) const {
        return Contains(name) ? headers_.Get(name) : "";
    }

    void HttpHeaders::setRawValue(const std::string& name, const std::string& value) {
        if (value.empty()) {
            Remove(name);
        } else {
            headers_.Set(name, value);
        }
    }

} // namespace System::Net::Http::Headers
