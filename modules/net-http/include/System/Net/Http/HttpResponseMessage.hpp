// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Net/Http/HttpContent.hpp"
#include "System/Net/Http/HttpRequestException.hpp"
#include "System/Net/Http/detail/HttpFieldValidation.hpp"
#include "System/Net/HttpStatusCode.hpp"
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace System::Net::Http {

/** Represents an HTTP response, including status code, headers, and body content. */
class HttpResponseMessage {
    System::Net::HttpStatusCode                    statusCode_;
    std::string                                    reasonPhrase_;
    std::shared_ptr<HttpContent>                   content_;
    std::unordered_map<std::string, std::string>   headers_;
public:
    explicit HttpResponseMessage(
        System::Net::HttpStatusCode statusCode = System::Net::HttpStatusCode::OK)
        : statusCode_(statusCode) {}

    [[nodiscard]] System::Net::HttpStatusCode getStatusCodeProperty() const { return statusCode_; }
    void setStatusCodeProperty(System::Net::HttpStatusCode v)               { statusCode_ = v; }

    [[nodiscard]] const std::string& getReasonPhraseProperty() const { return reasonPhrase_; }

    /**
     * @brief Sets the reason phrase that accompanies the status code.
     *
     * @throws System::FormatException if @p v contains a carriage return, a line feed or a
     * NUL character.
     *
     * @note **Narrowing since ticket #2063** (SR-AUD-316's reason half, cause NH-B). A reason
     * phrase is a protocol field; `"OK\r\nX-Injected: yes"` used to be accepted verbatim.
     * The status-code *domain* is a separate, unapproved question — this constructor still
     * accepts `-1` and `1000` (blocked ticket #2069, pinned by this ticket's suite).
     * See `docs/Migration-HttpControlCharacterRejection.md`.
     */
    void setReasonPhraseProperty(const std::string& v) {
        detail::ThrowIfControlCharacter(v, "reason phrase");
        reasonPhrase_ = v;
    }

    [[nodiscard]] std::shared_ptr<HttpContent> getContentProperty() const { return content_; }
    void setContentProperty(std::shared_ptr<HttpContent> v)               { content_ = std::move(v); }

    /** True when StatusCode is in the 200–299 range. */
    [[nodiscard]] bool getIsSuccessStatusCodeProperty() const {
        int c = static_cast<int>(statusCode_);
        return c >= 200 && c < 300;
    }

    /**
     * @brief Throws HttpRequestException if the response indicates failure.
     *
     * C++ counterpart of .NET HttpResponseMessage.EnsureSuccessStatusCode(), which returns
     * `this` to allow fluent chaining (e.g. `response.EnsureSuccessStatusCode().Content...`);
     * returns a const reference to self here for the same purpose, matching this method's
     * existing const-qualification. All existing call sites in this codebase already ignore the
     * return value, so this is a purely additive change.
     * @return *this.
     */
    const HttpResponseMessage& EnsureSuccessStatusCode() const {
        if (getIsSuccessStatusCodeProperty()) return *this;

        size_t firstNonSpace = reasonPhrase_.find_first_not_of(" \t\r\n");
        std::string message = "Response status code does not indicate success: " +
            std::to_string(static_cast<int>(statusCode_));
        message += (firstNonSpace == std::string::npos) ? "." : " (" + reasonPhrase_ + ").";

        throw HttpRequestException(HttpRequestError::Unknown, message, nullptr, statusCode_);
    }

    /**
     * @brief Adds or replaces a response header.
     *
     * @throws System::FormatException if @p name or @p value contains a carriage return, a
     * line feed or a NUL character.
     *
     * @note **Narrowing since ticket #2063** (SR-AUD-313, cause NH-B), applied here for the
     * same reason as on `HttpRequestMessage::setHeader`: leaving one of the two symmetric
     * header doors unvalidated would be arbitrary. `HttpClientHandler` never reaches this
     * throw for a server response — it rejects a control-bearing response header line itself,
     * with `HttpRequestException`, so a malformed *response* still surfaces as this module's
     * response-error type rather than as a format error.
     */
    void setHeader(const std::string& name, const std::string& value) {
        detail::ThrowIfControlCharacter(name, "header name");
        detail::ThrowIfControlCharacter(value, "header value");
        // Ticket #2068: case-insensitive, as RFC 9110 5.1 requires.
        detail::SetFieldReplacingCaseVariants(headers_, name, value);
    }

    /** @return The value of the named header, or "" if not present. */
    [[nodiscard]] std::string getHeader(const std::string& name) const {
        // Ticket #2068: this used to be a byte-exact find(), so getHeader("content-type")
        // returned "" after setHeader("Content-Type", ...).
        return detail::GetFieldIgnoringCase(headers_, name);
    }

    /** @return The map of all response headers. */
    [[nodiscard]] const std::unordered_map<std::string, std::string>& getHeadersProperty() const {
        return headers_;
    }
};

} // namespace System::Net::Http
