// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Net/Http/Headers/HttpContentHeaders.hpp"
#include "HttpDateParser.hpp"
#include "System/Convert.hpp"
#include "System/TimeSpan.hpp"
#include <array>
#include <cstdio>
#include <cstring>

namespace System::Net::Http::Headers {

    namespace {
        // Parses the RFC 1123 format that DateTimeOffset::ToString("r") produces (see
        // ContentDispositionHeaderValue.cpp for the same helper and why it's needed).
        // Ticket #2125 (SR-AUD-321, cause NH-H): this was one of SEVEN byte-identical copies of a
        // non-consuming sscanf HTTP-date parser. They are now one body, in
        // detail/HttpDateParser.hpp, which additionally requires the whole value to be consumed.
        inline bool tryParseRfc1123(const std::string& s, System::DateTimeOffset& result) {
            return System::Net::Http::Headers::detail::TryParseHttpDate(s, result);
        }
    }

    std::vector<std::string> HttpContentHeaders::getAllowProperty() const {
        std::vector<std::string> values;
        (void)TryGetValues("Allow", values);
        return values;
    }

    void HttpContentHeaders::AddAllow(const std::string& method) {
        Add("Allow", method);
    }

    std::optional<ContentDispositionHeaderValue> HttpContentHeaders::getContentDispositionProperty() const {
        std::string raw = getRawValue("Content-Disposition");
        if (raw.empty()) return std::nullopt;
        ContentDispositionHeaderValue result("x");
        if (!ContentDispositionHeaderValue::TryParse(raw, result)) return std::nullopt;
        return result;
    }

    void HttpContentHeaders::setContentDispositionProperty(const std::optional<ContentDispositionHeaderValue>& value) {
        setRawValue("Content-Disposition", value.has_value() ? value->ToString() : "");
    }

    std::vector<std::string> HttpContentHeaders::getContentEncodingProperty() const {
        std::vector<std::string> values;
        (void)TryGetValues("Content-Encoding", values);
        return values;
    }

    void HttpContentHeaders::AddContentEncoding(const std::string& encoding) {
        Add("Content-Encoding", encoding);
    }

    std::vector<std::string> HttpContentHeaders::getContentLanguageProperty() const {
        std::vector<std::string> values;
        (void)TryGetValues("Content-Language", values);
        return values;
    }

    void HttpContentHeaders::AddContentLanguage(const std::string& language) {
        Add("Content-Language", language);
    }

    std::optional<SharpRuntime::longcs> HttpContentHeaders::getContentLengthProperty() const {
        std::string raw = getRawValue("Content-Length");
        if (raw.empty()) return std::nullopt;
        try {
            size_t pos = 0;
            long long parsed = std::stoll(raw, &pos);
            if (pos != raw.size() || parsed < 0) return std::nullopt;
            return static_cast<SharpRuntime::longcs>(parsed);
        } catch (...) {
            return std::nullopt;
        }
    }

    void HttpContentHeaders::setContentLengthProperty(std::optional<SharpRuntime::longcs> value) {
        setRawValue("Content-Length", value.has_value() ? std::to_string(*value) : "");
    }

    std::optional<System::Uri> HttpContentHeaders::getContentLocationProperty() const {
        std::string raw = getRawValue("Content-Location");
        if (raw.empty()) return std::nullopt;
        try {
            return System::Uri(raw);
        } catch (...) {
            return std::nullopt;
        }
    }

    void HttpContentHeaders::setContentLocationProperty(const std::optional<System::Uri>& value) {
        setRawValue("Content-Location", value.has_value() ? value->ToString() : "");
    }

    std::optional<std::vector<SharpRuntime::bytecs>> HttpContentHeaders::getContentMD5Property() const {
        std::string raw = getRawValue("Content-MD5");
        if (raw.empty()) return std::nullopt;
        try {
            return System::Convert::FromBase64String(raw);
        } catch (...) {
            return std::nullopt;
        }
    }

    void HttpContentHeaders::setContentMD5Property(const std::optional<std::vector<SharpRuntime::bytecs>>& value) {
        setRawValue("Content-MD5", value.has_value() ? System::Convert::ToBase64String(*value) : "");
    }

    std::optional<ContentRangeHeaderValue> HttpContentHeaders::getContentRangeProperty() const {
        std::string raw = getRawValue("Content-Range");
        if (raw.empty()) return std::nullopt;
        ContentRangeHeaderValue result(0);
        if (!ContentRangeHeaderValue::TryParse(raw, result)) return std::nullopt;
        return result;
    }

    void HttpContentHeaders::setContentRangeProperty(const std::optional<ContentRangeHeaderValue>& value) {
        setRawValue("Content-Range", value.has_value() ? value->ToString() : "");
    }

    std::optional<MediaTypeHeaderValue> HttpContentHeaders::getContentTypeProperty() const {
        std::string raw = getRawValue("Content-Type");
        if (raw.empty()) return std::nullopt;
        MediaTypeHeaderValue result("x/x");
        if (!MediaTypeHeaderValue::TryParse(raw, result)) return std::nullopt;
        return result;
    }

    void HttpContentHeaders::setContentTypeProperty(const std::optional<MediaTypeHeaderValue>& value) {
        setRawValue("Content-Type", value.has_value() ? value->ToString() : "");
    }

    std::optional<System::DateTimeOffset> HttpContentHeaders::getExpiresProperty() const {
        std::string raw = getRawValue("Expires");
        if (raw.empty()) return std::nullopt;
        System::DateTimeOffset result;
        if (!tryParseRfc1123(raw, result)) return std::nullopt;
        return result;
    }

    void HttpContentHeaders::setExpiresProperty(const std::optional<System::DateTimeOffset>& value) {
        setRawValue("Expires", value.has_value() ? value->ToString("r") : "");
    }

    std::optional<System::DateTimeOffset> HttpContentHeaders::getLastModifiedProperty() const {
        std::string raw = getRawValue("Last-Modified");
        if (raw.empty()) return std::nullopt;
        System::DateTimeOffset result;
        if (!tryParseRfc1123(raw, result)) return std::nullopt;
        return result;
    }

    void HttpContentHeaders::setLastModifiedProperty(const std::optional<System::DateTimeOffset>& value) {
        setRawValue("Last-Modified", value.has_value() ? value->ToString("r") : "");
    }

} // namespace System::Net::Http::Headers
