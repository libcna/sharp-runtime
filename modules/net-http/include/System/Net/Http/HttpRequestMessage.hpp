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
#include <atomic>
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
    /**
     * @brief Whether an `HttpClient` has already sent this message.
     *
     * Ticket #2067 (SR-AUD-314, CCF-019). .NET throws `InvalidOperationException` when one
     * `HttpRequestMessage` is sent twice; this port had no such state, and a counting handler
     * received the exact same object twice. That is not merely untidy: the second send reuses a
     * content object the first send may already have consumed, and both sends share one headers
     * map that the first one's handler may have mutated.
     *
     * .NET's flag is `_sendStatus`, set with an interlocked compare-and-exchange
     * (`HttpRequestMessage.cs:26,173`) so two concurrent sends of one message cannot both win.
     * `std::atomic_flag`'s `test_and_set` is the same operation, so this port gets the same
     * guarantee rather than a racy approximation of it.
     *
     * Landed under `docs/StandingApprovals.md` SA-3: a private member, no vtable, base-class,
     * signature or `noexcept` change, `sizeof` pinned by the layout probe.
     */
    std::atomic_flag sent_ = ATOMIC_FLAG_INIT;

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

    /**
     * @brief Claims this message for one send, returning false if it was already claimed.
     *
     * Ticket #2067. The counterpart of .NET's `MarkAsSent()`
     * (`HttpRequestMessage.cs:173`), which is an interlocked compare-and-exchange for the same
     * reason this is a `test_and_set`: two threads sending one message must not both succeed.
     *
     * `HttpClient::Send` calls it and raises `InvalidOperationException` with .NET's own message
     * when it returns false. It is public rather than private because a custom
     * `HttpMessageHandler` invoked directly, without an `HttpClient`, is a legitimate caller.
     */
    bool MarkAsSent() noexcept { return !sent_.test_and_set(std::memory_order_acq_rel); }

    /** @return Whether an `HttpClient` has already sent this message (#2067). */
    [[nodiscard]] bool getWasSentProperty() const noexcept {
        // test() is const-correct on the flag's value without claiming it.
        return sent_.test(std::memory_order_acquire);
    }

    /** Gets the per-request option collection. */
    [[nodiscard]] HttpRequestOptions& getOptionsProperty() noexcept { return options_; }
    [[nodiscard]] const HttpRequestOptions& getOptionsProperty() const noexcept { return options_; }
};

} // namespace System::Net::Http
