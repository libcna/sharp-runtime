// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Net/Http/HttpContent.hpp"
#include "System/Net/Http/HttpRequestException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
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
    /**
     * @brief `0 <= code <= 999`, exactly .NET's bound.
     *
     * Ticket #2069 (SR-AUD-316's status-code half). Measured before it, `-1`, `0`, `1000` and
     * `99999` all constructed, and `getIsSuccessStatusCodeProperty()` answered false for each --
     * so a nonsense code was indistinguishable from a real failure. .NET validates in both the
     * constructor and the setter, with the same two checks in the same order
     * (`HttpResponseMessage.cs:152-159` and `:65-76`):
     *
     * ```csharp
     * ArgumentOutOfRangeException.ThrowIfNegative((int)value, nameof(value));
     * ArgumentOutOfRangeException.ThrowIfGreaterThan((int)value, 999, nameof(value));
     * ```
     *
     * The upper bound is **999**, not 599: RFC 9112 §4 makes a status code three digits, and
     * .NET accepts every three-digit value rather than only the registered ranges. `0` is
     * accepted too, and that is .NET's choice, not an oversight here.
     */
    static void throwIfStatusCodeOutOfRange(System::Net::HttpStatusCode value) {
        const int code = static_cast<int>(value);
        if (code < 0) {
            throw System::ArgumentOutOfRangeException("value", "The status code must not be negative.");
        }
        if (code > 999) {
            throw System::ArgumentOutOfRangeException("value", "The status code must not exceed 999.");
        }
    }

public:
    /**
     * @brief Constructs a response with @p statusCode.
     * @throws System::ArgumentOutOfRangeException if @p statusCode is negative or above 999.
     * @see throwIfStatusCodeOutOfRange
     */
    explicit HttpResponseMessage(
        System::Net::HttpStatusCode statusCode = System::Net::HttpStatusCode::OK)
        : statusCode_(statusCode) {
        throwIfStatusCodeOutOfRange(statusCode);
    }

    [[nodiscard]] System::Net::HttpStatusCode getStatusCodeProperty() const { return statusCode_; }
    /**
     * @brief Sets the status code.
     * @throws System::ArgumentOutOfRangeException if @p v is negative or above 999 (#2069).
     */
    void setStatusCodeProperty(System::Net::HttpStatusCode v) {
        throwIfStatusCodeOutOfRange(v);
        statusCode_ = v;
    }

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
