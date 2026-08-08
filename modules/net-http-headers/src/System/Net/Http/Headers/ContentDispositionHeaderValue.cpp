// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Net/Http/Headers/ContentDispositionHeaderValue.hpp"
#include "HeaderFieldSplitter.hpp"
#include "System/Net/detail/ProtocolFieldValidation.hpp"
#include "HttpDateParser.hpp"
#include "System/ArgumentException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/FormatException.hpp"
#include "System/HashCode.hpp"
#include "System/TimeSpan.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string_view>

namespace System::Net::Http::Headers {

    namespace {
        std::string trim(const std::string& s) {
            size_t start = s.find_first_not_of(" \t");
            if (start == std::string::npos) return "";
            size_t end = s.find_last_not_of(" \t");
            return s.substr(start, end - start + 1);
        }

        bool isHttpTokenChar(unsigned char c) {
            static constexpr std::string_view extras = "!#$%&'*+-.^_`|~";
            return c != 0 && (std::isalnum(c) || extras.find(static_cast<char>(c)) != std::string_view::npos);
        }

        bool isToken(const std::string& s) {
            return !s.empty() && std::all_of(s.begin(), s.end(), [](char c) { return isHttpTokenChar(static_cast<unsigned char>(c)); });
        }

        void checkValidToken(const std::string& value, const std::string& paramName) {
            System::ArgumentException::ThrowIfNullOrEmpty(value, paramName);
            if (!isToken(value)) {
                throw System::FormatException("The value is not a valid HTTP token: " + value);
            }
        }

        bool equalsIgnoreCase(const std::string& a, const std::string& b) {
            if (a.size() != b.size()) return false;
            return std::equal(a.begin(), a.end(), b.begin(), [](unsigned char x, unsigned char y) {
                return std::tolower(x) == std::tolower(y);
            });
        }

        std::string toLowerAscii(const std::string& s) {
            std::string out = s;
            std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return out;
        }

        bool isQuoted(const std::string& s) {
            return s.size() >= 2 && s.front() == '"' && s.back() == '"';
        }

        std::string stripQuotes(const std::string& s) {
            return isQuoted(s) ? s.substr(1, s.size() - 2) : s;
        }

        std::string quote(const std::string& s) {
            return "\"" + s + "\"";
        }

        // Splits a ';'-separated list at the top level, ignoring ';' inside a quoted string.
        // Ticket #2126 (SR-AUD-320, cause NH-H): this was one of SEVEN list splitters that toggled
        // on every quote with no notion of a quoted-pair, so a legal escaped quote followed by the
        // delimiter split a parameter in the middle of its own value. They are now one body, in
        // HeaderFieldSplitter.hpp. This is a WIDENING: valid RFC 9110 text starts being accepted.
        inline std::vector<std::string> splitTopLevel(const std::string& input, char delim) {
            return System::Net::Http::Headers::detail::SplitTopLevel(input, delim);
        }

        bool isRfc5987AttrChar(unsigned char c) {
            static constexpr std::string_view extras = "!#$&+-.^_`|~";
            return c != 0 && (std::isalnum(c) || extras.find(static_cast<char>(c)) != std::string_view::npos);
        }

        std::string encode5987(const std::string& value) {
            std::string out = "UTF-8''";
            char buf[4];
            for (unsigned char c : value) {
                if (isRfc5987AttrChar(c)) {
                    out += static_cast<char>(c);
                } else {
                    std::snprintf(buf, sizeof(buf), "%%%02X", c);
                    out += buf;
                }
            }
            return out;
        }

        // Parses the RFC 1123 format that DateTimeOffset::ToString("r") produces
        // (e.g. "Wed, 21 Oct 2015 07:28:00 GMT"). DateTimeOffset::TryParse doesn't accept this
        // format (only ISO-8601-style strings), so Content-Disposition's date parameters —
        // which the HTTP spec mandates in this RFC 822/1123 family of formats — need their own
        // parser here to round-trip what ToString("r") emits.
        // Ticket #2125 (SR-AUD-321, cause NH-H): this was one of SEVEN byte-identical copies of a
        // non-consuming sscanf HTTP-date parser. They are now one body, in
        // detail/HttpDateParser.hpp, which additionally requires the whole value to be consumed.
        inline bool tryParseRfc1123(const std::string& s, System::DateTimeOffset& result) {
            return System::Net::Http::Headers::detail::TryParseHttpDate(s, result);
        }

        // Ticket #2127 (SR-AUD-323, cause NH-J, docs/SystemNetHttpHeadersNamespaceReviewPlan.md
        // §4.5). RFC 5987 §3.2.1 gives the value the form
        //
        //     charset "'" [ language ] "'" value-chars
        //
        // and the charset label is normative: it says how the percent-decoded octets are to be
        // read. Before #2127 the label was parsed only far enough to find the delimiters and then
        // **discarded**, so `filename*=iso-8859-1''foo-%E4.html` produced a 10-byte string with a
        // raw 0xE4 -- not text in this port's UTF-8 `std::string` world, just a stray byte -- and
        // `filename*=bogus''x` was accepted as though the label meant nothing.
        //
        // RFC 5987 §3.2.1 names exactly two charsets a recipient must handle, `UTF-8` and
        // `ISO-8859-1`, and requires it to reject a value whose charset it does not support.
        // That is what this now does. Anything else, including an EMPTY label, is rejected.
        //
        // **Recorded as this port's choice.** `/rv/tmp/runtime/` is absent, so whether .NET
        // rejects the parameter or drops it silently could not be established from repository
        // evidence (plan §10). Rejection is chosen because it is what the RFC requires and because
        // it matches the failure mode this decoder already had for malformed input: the getter
        // reports the parameter as absent rather than handing back text of unknown encoding.
        bool isSupportedCharset(const std::string& label, bool& isLatin1) {
            auto equalsIgnoreCase = [](const std::string& a, const char* b) {
                size_t i = 0;
                for (; i < a.size() && b[i] != '\0'; ++i) {
                    if (std::tolower(static_cast<unsigned char>(a[i]))
                        != std::tolower(static_cast<unsigned char>(b[i]))) return false;
                }
                return i == a.size() && b[i] == '\0';
            };
            if (equalsIgnoreCase(label, "utf-8")) { isLatin1 = false; return true; }
            if (equalsIgnoreCase(label, "iso-8859-1")) { isLatin1 = true; return true; }
            return false;
        }

        bool tryDecode5987(const std::string& value, std::string& result) {
            size_t firstQuote = value.find('\'');
            if (firstQuote == std::string::npos) return false;
            size_t secondQuote = value.find('\'', firstQuote + 1);
            if (secondQuote == std::string::npos) return false;
            // Real .NET's TryDecode5987 requires *exactly* two single quotes in the whole
            // string (the charset'language'value delimiters):
            // `quoteIndex == lastQuoteIndex || input.IndexOf('\'', quoteIndex + 1) !=
            // lastQuoteIndex` rejects both "fewer than 2" and "more than 2". This previously
            // only checked for the first two, silently accepting a third (or more) quote as
            // part of the value -- e.g. "UTF-8'en'a'b" would decode to "a'b" instead of being
            // rejected as malformed, since a well-formed encoder must percent-encode any
            // literal apostrophe in the value (isRfc5987AttrChar doesn't allow a raw `'`).
            if (value.find('\'', secondQuote + 1) != std::string::npos) return false;

            // #2127: the charset label is normative, not decoration.
            bool isLatin1 = false;
            if (!isSupportedCharset(value.substr(0, firstQuote), isLatin1)) return false;

            std::string encoded = value.substr(secondQuote + 1);
            std::string decoded;
            for (size_t i = 0; i < encoded.size(); ++i) {
                if (encoded[i] == '%') {
                    // #2127, and a defect the finding does not name: the bound used to be
                    // `i + 2 < encoded.size()`, so a TRUNCATED escape at the end of the value
                    // ("a%C", "a%") fell through to the else branch and was kept as literal text
                    // -- silently turning malformed input into a plausible-looking file name.
                    // A truncated escape is malformed, and this decoder already rejected a
                    // non-hex escape, so it is rejected for the same reason.
                    if (i + 2 >= encoded.size()) return false;
                    auto hexVal = [](char c) -> int {
                        if (c >= '0' && c <= '9') return c - '0';
                        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                        return -1;
                    };
                    int hi = hexVal(encoded[i + 1]);
                    int lo = hexVal(encoded[i + 2]);
                    if (hi < 0 || lo < 0) return false;
                    const unsigned char octet = static_cast<unsigned char>((hi << 4) | lo);
                    if (isLatin1 && octet >= 0x80) {
                        // ISO-8859-1 octet 0x80..0xFF IS the code point, so it becomes two UTF-8
                        // bytes here. Without this, `iso-8859-1''foo-%E4.html` produced a raw 0xE4
                        // in a std::string that the rest of this runtime reads as UTF-8.
                        decoded += static_cast<char>(0xC0 | (octet >> 6));
                        decoded += static_cast<char>(0x80 | (octet & 0x3F));
                    } else {
                        decoded += static_cast<char>(octet);
                    }
                    i += 2;
                } else if (isLatin1 && static_cast<unsigned char>(encoded[i]) >= 0x80) {
                    // A literal high byte is not legal in an RFC 5987 value (attr-char is ASCII),
                    // but if one arrives under an ISO-8859-1 label it is a Latin-1 code point by
                    // the same rule as the percent-encoded case, and must not be copied raw into
                    // a UTF-8 string.
                    const unsigned char octet = static_cast<unsigned char>(encoded[i]);
                    decoded += static_cast<char>(0xC0 | (octet >> 6));
                    decoded += static_cast<char>(0x80 | (octet & 0x3F));
                } else {
                    decoded += encoded[i];
                }
            }
            // Ticket #2129 (post-audit, no SR-AUD identifier, cause NH-K, plan §4.5 last row and
            // §8.3). `filename*=UTF-8''a%0D%0Ab` decoded to a string containing a RAW CR/LF and
            // handed it to the caller. `ToString()` re-encodes it percent-escaped, so it does not
            // inject on serialization -- but any consumer that puts this value into another
            // header, a log line, a file name, or a `Content-Disposition` it builds itself does.
            //
            // **This is deliberately NOT a CCF-021 member**, and it is the family's closest call.
            // The family's guarantee is "reject the field terminator at the door before any byte
            // reaches the wire"; here the terminator travels INWARD, to the caller, and never
            // appears in serialized text. Different guarantee, different cause. It uses the same
            // predicate because the same three characters are the hazard, not because it is the
            // same family -- and the scope stays exactly those three: a decoded TAB or ESC is not
            // a framing hazard and is still returned (pinned).
            if (System::Net::detail::ContainsProtocolFieldTerminator(decoded)) return false;

            result = decoded;
            return true;
        }
    }

    const NameValueHeaderValue* ContentDispositionHeaderValue::findParameter(const std::string& name) const {
        for (const auto& p : parameters_) {
            if (equalsIgnoreCase(p.getNameProperty(), name)) return &p;
        }
        return nullptr;
    }

    void ContentDispositionHeaderValue::removeParameter(const std::string& name) {
        parameters_.erase(std::remove_if(parameters_.begin(), parameters_.end(),
            [&](const NameValueHeaderValue& p) { return equalsIgnoreCase(p.getNameProperty(), name); }), parameters_.end());
    }

    void ContentDispositionHeaderValue::setOrAddParameter(const std::string& name, const std::string& value) {
        for (auto& p : parameters_) {
            if (equalsIgnoreCase(p.getNameProperty(), name)) {
                p.setValueProperty(value);
                return;
            }
        }
        parameters_.push_back(NameValueHeaderValue(name, value));
    }

    ContentDispositionHeaderValue::ContentDispositionHeaderValue(const std::string& dispositionType) {
        checkValidToken(dispositionType, "dispositionType");
        dispositionType_ = dispositionType;
    }

    void ContentDispositionHeaderValue::setDispositionTypeProperty(const std::string& value) {
        checkValidToken(value, "value");
        dispositionType_ = value;
    }

    std::string ContentDispositionHeaderValue::getNameProperty() const {
        const auto* p = findParameter("name");
        return p ? stripQuotes(p->getValueProperty()) : "";
    }

    void ContentDispositionHeaderValue::setNameProperty(const std::string& value) {
        if (value.empty()) { removeParameter("name"); return; }
        setOrAddParameter("name", quote(value));
    }

    std::string ContentDispositionHeaderValue::getFileNameProperty() const {
        const auto* p = findParameter("filename");
        return p ? stripQuotes(p->getValueProperty()) : "";
    }

    void ContentDispositionHeaderValue::setFileNameProperty(const std::string& value) {
        if (value.empty()) { removeParameter("filename"); return; }
        setOrAddParameter("filename", quote(value));
    }

    std::string ContentDispositionHeaderValue::getFileNameStarProperty() const {
        const auto* p = findParameter("filename*");
        if (!p) return "";
        std::string result;
        return tryDecode5987(p->getValueProperty(), result) ? result : "";
    }

    void ContentDispositionHeaderValue::setFileNameStarProperty(const std::string& value) {
        if (value.empty()) { removeParameter("filename*"); return; }
        setOrAddParameter("filename*", encode5987(value));
    }

    std::optional<System::DateTimeOffset> ContentDispositionHeaderValue::getCreationDateProperty() const {
        const auto* p = findParameter("creation-date");
        if (!p) return std::nullopt;
        System::DateTimeOffset result;
        if (tryParseRfc1123(stripQuotes(p->getValueProperty()), result)) return result;
        return std::nullopt;
    }

    void ContentDispositionHeaderValue::setCreationDateProperty(std::optional<System::DateTimeOffset> value) {
        if (!value.has_value()) { removeParameter("creation-date"); return; }
        setOrAddParameter("creation-date", quote(value->ToString("r")));
    }

    std::optional<System::DateTimeOffset> ContentDispositionHeaderValue::getModificationDateProperty() const {
        const auto* p = findParameter("modification-date");
        if (!p) return std::nullopt;
        System::DateTimeOffset result;
        if (tryParseRfc1123(stripQuotes(p->getValueProperty()), result)) return result;
        return std::nullopt;
    }

    void ContentDispositionHeaderValue::setModificationDateProperty(std::optional<System::DateTimeOffset> value) {
        if (!value.has_value()) { removeParameter("modification-date"); return; }
        setOrAddParameter("modification-date", quote(value->ToString("r")));
    }

    std::optional<System::DateTimeOffset> ContentDispositionHeaderValue::getReadDateProperty() const {
        const auto* p = findParameter("read-date");
        if (!p) return std::nullopt;
        System::DateTimeOffset result;
        if (tryParseRfc1123(stripQuotes(p->getValueProperty()), result)) return result;
        return std::nullopt;
    }

    void ContentDispositionHeaderValue::setReadDateProperty(std::optional<System::DateTimeOffset> value) {
        if (!value.has_value()) { removeParameter("read-date"); return; }
        setOrAddParameter("read-date", quote(value->ToString("r")));
    }

    std::optional<SharpRuntime::longcs> ContentDispositionHeaderValue::getSizeProperty() const {
        const auto* p = findParameter("size");
        if (!p) return std::nullopt;
        try {
            size_t pos = 0;
            long long parsed = std::stoll(p->getValueProperty(), &pos);
            if (pos != p->getValueProperty().size() || parsed < 0) return std::nullopt;
            return static_cast<SharpRuntime::longcs>(parsed);
        } catch (...) {
            return std::nullopt;
        }
    }

    void ContentDispositionHeaderValue::setSizeProperty(std::optional<SharpRuntime::longcs> value) {
        if (!value.has_value()) { removeParameter("size"); return; }
        if (*value < 0) throw System::ArgumentOutOfRangeException("value", "size must not be negative.");
        setOrAddParameter("size", std::to_string(*value));
    }

    std::string ContentDispositionHeaderValue::ToString() const {
        std::string result = dispositionType_;
        for (const auto& p : parameters_) {
            result += "; " + p.ToString();
        }
        return result;
    }

    bool ContentDispositionHeaderValue::Equals(const ContentDispositionHeaderValue& other) const {
        if (!equalsIgnoreCase(dispositionType_, other.dispositionType_)) return false;
        if (parameters_.size() != other.parameters_.size()) return false;

        std::vector<bool> matched(other.parameters_.size(), false);
        for (const auto& p : parameters_) {
            bool found = false;
            for (size_t i = 0; i < other.parameters_.size(); ++i) {
                if (!matched[i] && p == other.parameters_[i]) { matched[i] = true; found = true; break; }
            }
            if (!found) return false;
        }
        return true;
    }

    SharpRuntime::intcs ContentDispositionHeaderValue::GetHashCode() const {
        SharpRuntime::intcs result = static_cast<SharpRuntime::intcs>(std::hash<std::string>{}(toLowerAscii(dispositionType_)));
        for (const auto& p : parameters_) result ^= p.GetHashCode();
        return result;
    }

    bool ContentDispositionHeaderValue::TryParse(const std::string& input, ContentDispositionHeaderValue& parsedValue) {
        auto segments = splitTopLevel(input, ';');
        std::string dispositionType = trim(segments[0]);
        if (!isToken(dispositionType)) return false;

        ContentDispositionHeaderValue result(dispositionType);
        for (size_t i = 1; i < segments.size(); ++i) {
            std::string segment = trim(segments[i]);
            if (segment.empty()) return false;
            try {
                result.parameters_.push_back(NameValueHeaderValue::Parse(segment));
            } catch (...) {
                return false;
            }
        }

        parsedValue = result;
        return true;
    }

    ContentDispositionHeaderValue ContentDispositionHeaderValue::Parse(const std::string& input) {
        ContentDispositionHeaderValue result("x");
        if (!TryParse(input, result)) {
            throw System::FormatException("The Content-Disposition header value is not valid: " + input);
        }
        return result;
    }

} // namespace System::Net::Http::Headers
