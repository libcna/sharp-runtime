// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Net/Http/Headers/NameValueHeaderValue.hpp"
#include "System/ArgumentException.hpp"
#include "System/FormatException.hpp"
#include "System/Net/detail/ProtocolFieldValidation.hpp"
#include <algorithm>
#include <cctype>
#include <cstring>

namespace System::Net::Http::Headers {

    namespace {
        bool isHttpTokenChar(unsigned char c) {
            static constexpr const char* extras = "!#$%&'*+-.^_`|~";
            return c != 0 && (std::isalnum(c) || std::strchr(extras, static_cast<char>(c)) != nullptr);
        }

        bool isToken(const std::string& s) {
            return !s.empty() && std::all_of(s.begin(), s.end(), [](char c) { return isHttpTokenChar(static_cast<unsigned char>(c)); });
        }

        bool equalsIgnoreCase(const std::string& a, const std::string& b) {
            if (a.size() != b.size()) return false;
            return std::equal(a.begin(), a.end(), b.begin(), [](unsigned char x, unsigned char y) {
                return std::tolower(x) == std::tolower(y);
            });
        }

        SharpRuntime::intcs hashIgnoreCase(const std::string& s) {
            std::string lower = s;
            std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return static_cast<SharpRuntime::intcs>(std::hash<std::string>{}(lower));
        }
    }

    bool NameValueHeaderValue::isValidQuotedString(const std::string& value) {
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

    void NameValueHeaderValue::checkValidToken(const std::string& value, const std::string& paramName) {
        System::ArgumentException::ThrowIfNullOrEmpty(value, paramName);
        if (!isToken(value)) {
            throw System::FormatException("The value is not a valid HTTP token: " + value);
        }
    }

    // Ticket #2124 (SR-AUD-319, cause NH-H, docs/SystemNetHttpHeadersNamespaceReviewPlan.md §4.1).
    //
    // Before #2124 the field-terminator check sat in the `else` branch below, so it ran only for an
    // UNQUOTED value. A value that began with '"' was handed to isValidQuotedString(), which checks
    // the quoting grammar and nothing else -- so `NameValueHeaderValue("p", "\"a\r\nX-Injected: yes\"")`
    // was accepted and ToString() emitted the terminator verbatim. Every parameter in this module is
    // a NameValueHeaderValue, and five types (MediaTypeHeaderValue, TransferCodingHeaderValue,
    // ContentDispositionHeaderValue, CacheControlHeaderValue and NameValueWithParametersHeaderValue)
    // hand out their parameter vector by MUTABLE reference, so this is the only place the rule can
    // sit and still cover them all.
    //
    // The check is now flat and applies to the whole value, quoted or not, which is also what makes
    // a quoted-PAIR terminator (`"a\<CR>b"` -- an escape sequence whose escapee is a raw CR) a
    // rejection: the backslash is not a wire-level escape, the CR still ends the field.
    //
    // The predicate is System::Net::detail::ContainsProtocolFieldTerminator -- the family's single
    // body, shared with the ten System::Net::Http doors of #2063 and the three
    // System::Net::WebSockets doors of #2089. A fourth copy of it here would BE cause NH-H.
    void NameValueHeaderValue::checkValueFormat(const std::string& value) {
        if (value.empty()) return;

        // Checked first and separately: the offending text is deliberately NOT echoed, per the
        // companion rule in ProtocolFieldValidation.hpp -- a CR/LF-bearing value copied into an
        // exception message and then logged re-creates the injection the rejection prevents.
        if (System::Net::detail::ContainsProtocolFieldTerminator(value)) {
            throw System::FormatException("The value contains invalid CR, LF, or NUL characters.");
        }

        if (value.front() == ' ' || value.front() == '\t' || value.back() == ' ' || value.back() == '\t') {
            throw System::FormatException("The value is not valid: " + value);
        }

        if (value.front() == '"' && !isValidQuotedString(value)) {
            throw System::FormatException("The value is not a valid quoted-string: " + value);
        }
    }

    NameValueHeaderValue::NameValueHeaderValue(const std::string& name) : name_(name) {
        checkValidToken(name, "name");
    }

    NameValueHeaderValue::NameValueHeaderValue(const std::string& name, const std::string& value)
        : name_(name), value_(value) {
        checkValidToken(name, "name");
        checkValueFormat(value);
    }

    void NameValueHeaderValue::setValueProperty(const std::string& value) {
        checkValueFormat(value);
        value_ = value;
    }

    bool NameValueHeaderValue::Equals(const NameValueHeaderValue& other) const {
        if (!equalsIgnoreCase(name_, other.name_)) return false;

        if (value_.empty()) return other.value_.empty();
        if (other.value_.empty()) return false;

        if (value_.front() == '"') {
            return value_ == other.value_;
        }
        return equalsIgnoreCase(value_, other.value_);
    }

    SharpRuntime::intcs NameValueHeaderValue::GetHashCode() const {
        SharpRuntime::intcs nameHash = hashIgnoreCase(name_);
        if (value_.empty()) return nameHash;

        if (value_.front() == '"') {
            return nameHash ^ static_cast<SharpRuntime::intcs>(std::hash<std::string>{}(value_));
        }
        return nameHash ^ hashIgnoreCase(value_);
    }

    std::string NameValueHeaderValue::ToString() const {
        if (!value_.empty()) return name_ + "=" + value_;
        return name_;
    }

    bool NameValueHeaderValue::TryParse(const std::string& input, NameValueHeaderValue& parsedValue) {
        // #2124: TryParse assigns name_/value_ directly and so never reaches checkValueFormat.
        // A field terminator anywhere in the input means the text is not one header field value,
        // whatever the rest of the grammar says, so it is rejected before the grammar runs. This
        // is also the door MediaTypeHeaderValue, TransferCodingHeaderValue, CacheControlHeaderValue
        // and ContentDispositionHeaderValue reach their parameters through.
        if (System::Net::detail::ContainsProtocolFieldTerminator(input)) return false;

        size_t start = input.find_first_not_of(" \t");
        if (start == std::string::npos) return false;
        size_t end = input.find_last_not_of(" \t");
        std::string trimmed = input.substr(start, end - start + 1);

        size_t eq = trimmed.find('=');
        std::string name = (eq == std::string::npos) ? trimmed : trimmed.substr(0, eq);

        size_t nameEnd = name.find_last_not_of(" \t");
        if (nameEnd == std::string::npos) return false;
        name = name.substr(0, nameEnd + 1);
        if (!isToken(name)) return false;

        std::string value;
        if (eq != std::string::npos) {
            value = trimmed.substr(eq + 1);
            size_t valueStart = value.find_first_not_of(" \t");
            // Real .NET's GetValueLength requires a non-empty token or quoted-string after '=';
            // "name=" (nothing, or only whitespace, after the delimiter) is an invalid value and
            // must be rejected, not silently treated as a name-only value.
            if (valueStart == std::string::npos) return false;
            value = value.substr(valueStart);
            // trimmed has no trailing whitespace (see initial trim above), so value doesn't either.

            // Real .NET's TryParse (GetValueLength -> GetTokenLength or GetQuotedStringLength,
            // with the caller requiring the whole input to be consumed) only accepts an unquoted
            // value that is ENTIRELY token characters, or a well-formed quoted-string -- unlike
            // the looser checkValueFormat() used by the public constructor/setValueProperty
            // (which only rejects leading/trailing whitespace and embedded CR/LF/NUL, matching
            // real .NET's own looser CheckValueFormat for that path). A naive find('=')-based
            // split previously let an unquoted value containing '=' (e.g. "a=b=c") or other
            // non-token characters through TryParse, which real .NET's stricter grammar rejects.
            if (value.front() == '"') {
                if (!isValidQuotedString(value)) return false;
            } else if (!isToken(value)) {
                return false;
            }
        }

        NameValueHeaderValue result;
        result.name_ = name;
        result.value_ = value;
        parsedValue = result;
        return true;
    }

    NameValueHeaderValue NameValueHeaderValue::Parse(const std::string& input) {
        NameValueHeaderValue result;
        if (!TryParse(input, result)) {
            throw System::FormatException("The header value is not valid: " + input);
        }
        return result;
    }

} // namespace System::Net::Http::Headers
