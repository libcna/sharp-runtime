// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Net/Http/HttpMethod.hpp"
#include "System/Net/Http/HttpContent.hpp"
#include "System/Net/Http/HttpRequestOptions.hpp"
#include "System/Net/Http/detail/HttpFieldValidation.hpp"
#include <memory>
#include <string>
#include <unordered_map>

namespace System::Net::Http {

/**
 * @brief Represents an HTTP request message, mirroring .NET System.Net.Http.HttpRequestMessage.
 *
 * @note Deliberately simplified relative to real .NET: RequestUri is a plain string rather than
 * a System::Uri (avoiding a broad, unrequested refactor of this and other Net.Http call sites),
 * headers are a raw name/value map rather than the ported HttpRequestHeaders type (this
 * project's Net/Net.Http subsystems intentionally keep several independent, simplified
 * header-bag designs rather than unifying them -- see NEXT.md's "Do not do yet" section), and
 * Version/VersionPolicy are absent since this runtime's HttpClient only supports HTTP/1.1 over
 * plain TCP.
 */
class HttpRequestMessage {
    HttpMethod                                     method_;
    std::string                                    uri_;
    std::shared_ptr<HttpContent>                   content_;
    std::unordered_map<std::string, std::string>   headers_;
    HttpRequestOptions                              options_;
public:
    /** Constructs an HttpRequestMessage with the default GET method and an empty URI. */
    HttpRequestMessage() : method_(HttpMethod::Get()) {}

    /**
     * @brief Constructs an HttpRequestMessage with the given method and URI.
     * @param method HTTP method (verb).
     * @param uri    Request URI string.
     */
    HttpRequestMessage(const HttpMethod& method, const std::string& uri)
        : method_(method), uri_(uri) {}

    /** Returns the HTTP method of this request. */
    [[nodiscard]] const HttpMethod& getMethodProperty() const { return method_; }
    /** Sets the HTTP method of this request. */
    void setMethodProperty(const HttpMethod& v)               { method_ = v; }

    /** Returns the request URI string. */
    [[nodiscard]] const std::string& getRequestUriProperty() const { return uri_; }
    /** Sets the request URI string. */
    void setRequestUriProperty(const std::string& v)               { uri_ = v; }

    /** Returns the content body of this request (may be null). */
    [[nodiscard]] std::shared_ptr<HttpContent> getContentProperty() const { return content_; }
    /** Sets the content body of this request. */
    void setContentProperty(std::shared_ptr<HttpContent> v)               { content_ = std::move(v); }

    /**
     * @brief Adds or replaces a request header.
     * @param name  Header name.
     * @param value Header value.
     *
     * @throws System::FormatException if @p name or @p value contains a carriage return, a
     * line feed or a NUL character.
     *
     * @note **Narrowing since ticket #2063** (SR-AUD-313, cause NH-B). This map is written
     * straight onto the wire by `HttpClientHandler::Send` as `name + ": " + value + "\r\n"`,
     * so before #2063 a value of `"v\r\nX-Injected: yes"` emitted **two** header fields.
     * A space and a horizontal tab are legal in a header value and are still accepted, as is
     * an empty value. See `docs/Migration-HttpControlCharacterRejection.md`.
     */
    void setHeader(const std::string& name, const std::string& value) {
        detail::ThrowIfControlCharacter(name, "header name");
        detail::ThrowIfControlCharacter(value, "header value");
        // Ticket #2068. Field names are case-insensitive (RFC 9110 5.1), and .NET compares them
        // that way everywhere. Before this, `Content-Type` and `content-type` were two entries
        // and BOTH went on the wire.
        detail::SetFieldReplacingCaseVariants(headers_, name, value);
    }

    /**
     * @brief Returns this request's header value, or an empty string if it has none.
     *
     * Ticket #2068. Case-insensitive, as RFC 9110 5.1 requires: `getHeader("content-type")`
     * finds a header set as `Content-Type`.
     */
    [[nodiscard]] std::string getHeader(const std::string& name) const {
        return detail::GetFieldIgnoringCase(headers_, name);
    }

    /** Returns the map of all request headers. */
    [[nodiscard]] const std::unordered_map<std::string, std::string>& getHeadersProperty() const {
        return headers_;
    }

    /** Gets the per-request option collection. */
    [[nodiscard]] HttpRequestOptions& getOptionsProperty() noexcept { return options_; }
    [[nodiscard]] const HttpRequestOptions& getOptionsProperty() const noexcept { return options_; }
};

} // namespace System::Net::Http
