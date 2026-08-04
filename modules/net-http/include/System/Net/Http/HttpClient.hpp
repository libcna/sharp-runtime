// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Net/Http/HttpMethod.hpp"
#include "System/Net/Http/HttpContent.hpp"
#include "System/Net/Http/HttpMessageHandler.hpp"
#include "System/Net/Http/HttpRequestMessage.hpp"
#include "System/Net/Http/HttpResponseMessage.hpp"
#include "System/Threading/Tasks/Task.hpp"
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace System::Net::Http {

/**
 * Sends HTTP/1.1 requests and receives HTTP responses.
 *
 * Supports HTTP (plain TCP, port 80 by default). HTTPS is not currently supported.
 * On Emscripten, all methods throw System::PlatformNotSupportedException.
 *
 * @note Unlike real .NET's HttpClient (which follows 3xx redirects by default via
 * AllowAutoRedirect), this port never follows redirects -- Send() returns the raw 3xx response
 * with its Location header intact rather than transparently re-requesting it. Following
 * redirects correctly needs cross-host header/body semantics (which headers survive a redirect,
 * how each 301/302/303/307/308 status differs in whether the method/body carry over, loop
 * detection) that go beyond a single audit-pass fix; documented here as a known, disclosed gap
 * rather than a bug to silently work around.
 */
class HttpClient {
public:
    /** Constructs an HttpClient backed by a default HttpClientHandler (plain-socket HTTP/1.1). */
    HttpClient();

    /**
     * Constructs an HttpClient that sends every request through @p handler instead of the
     * default HttpClientHandler. C++ counterpart of .NET HttpClient(HttpMessageHandler).
     * @p handler is typically a DelegatingHandler chain (auth injection, logging, retry
     * policies, etc.) terminating in an HttpClientHandler that performs the actual socket I/O.
     */
    explicit HttpClient(std::shared_ptr<HttpMessageHandler> handler);

    ~HttpClient();

    // Non-copyable — sockets are not trivially copyable
    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;

    // ---- Synchronous API ----

    /** Sends the given request and returns the response. */
    std::shared_ptr<HttpResponseMessage> Send(std::shared_ptr<HttpRequestMessage> request);

    /** Sends a GET request to url. */
    std::shared_ptr<HttpResponseMessage> Get(const std::string& url);

    /** Sends a POST request with the given content. */
    std::shared_ptr<HttpResponseMessage> Post(const std::string& url,
                                              std::shared_ptr<HttpContent> content);

    /** GET and returns the response body as a UTF-8 string. Throws on non-2xx. */
    std::string GetString(const std::string& url);

    /** GET and returns the response body as raw bytes. Throws on non-2xx. */
    std::vector<SharpRuntime::bytecs> GetByteArray(const std::string& url);

    // ---- Async API (backed by TaskT / std::async on POSIX+Windows; throws on Emscripten) ----

    System::Threading::Tasks::TaskT<std::shared_ptr<HttpResponseMessage>>
        SendAsync(std::shared_ptr<HttpRequestMessage> request);

    System::Threading::Tasks::TaskT<std::shared_ptr<HttpResponseMessage>>
        GetAsync(const std::string& url);

    System::Threading::Tasks::TaskT<std::shared_ptr<HttpResponseMessage>>
        PostAsync(const std::string& url, std::shared_ptr<HttpContent> content);

    System::Threading::Tasks::TaskT<std::string>
        GetStringAsync(const std::string& url);

    System::Threading::Tasks::TaskT<std::vector<SharpRuntime::bytecs>>
        GetByteArrayAsync(const std::string& url);

    // ---- Configuration ----

    [[nodiscard]] const std::string& getBaseAddressProperty() const { return baseAddress_; }
    void setBaseAddressProperty(const std::string& v)               { baseAddress_ = v; }

    void setDefaultHeader(const std::string& name, const std::string& value);
    [[nodiscard]] std::string getDefaultHeader(const std::string& name) const;

    // ---- URL parser (public for testability) ----

    struct ParsedUrl {
        std::string scheme;
        std::string host;
        SharpRuntime::intcs port = 80;
        std::string path;
    };

    /**
     * @brief Parses an absolute HTTP URL into its components.
     *
     * The authority ends at the first `/`, `?` or `#`. A query that follows the authority
     * directly becomes the request target `"/?…"` rather than part of the host. A **fragment
     * is never part of a request target** (RFC 9110 §7.1 — it is client-side only) and is
     * dropped. The host is lowercased, as the scheme already was.
     *
     * `port` is a 16-bit TCP port: exactly one to five ASCII digits with no sign, no
     * surrounding space and no trailing text, in the range 0…65535.
     *
     * @throws System::UriFormatException for a malformed URL — a missing scheme, an empty
     *         host, an unterminated IPv6 literal, a port outside the rules above, or (since
     *         ticket #2063) a carriage return, line feed or NUL anywhere in @p url.
     * @throws System::NotSupportedException for a well-formed but non-HTTP scheme.
     *
     * @note **Narrowing and value changes since ticket #2064** (SR-AUD-311, cause NH-A /
     * CCF-002). `http://host:80abc` used to parse to port **80**, `http://host:-1` to **−1**
     * and `http://host:99999` to **99999**, because `std::stoi` accepts a valid prefix and
     * reports success. `http://host?q=1` used to parse to host **`host?q=1`** with path `/`,
     * so a query string reached DNS and the `Host:` header while the request line asked for
     * `/`. `HOST.EXAMPLE` and `host.example` used to be distinct hosts.
     */
    static ParsedUrl parseUrl(const std::string& url);

    struct ParsedStatusLine {
        SharpRuntime::intcs statusCode = 0;
        std::string reason;
    };

    /**
     * @brief Parses an HTTP response status line (e.g. `"HTTP/1.1 200 OK"`).
     *
     * The version token must be exactly `HTTP/<digit>.<digit>` (RFC 9112 §2.3) and the status
     * code exactly three ASCII digits (RFC 9112 §4). `HTTP/9.9` satisfies the grammar and is
     * **accepted** — a version this port does not speak is the server's behaviour to report,
     * not a parse error. `"HTTP/1.1 099 OK"` is likewise **accepted**, as the code 99; it is
     * three digits, which is all the grammar asks for. Neither choice is verified against the
     * .NET reference (absent from this container) and both are pinned by tests.
     *
     * @throws HttpRequestException if the version token is malformed, if the status code is
     *         not exactly three digits, or (since ticket #2063) if the line contains a
     *         carriage return, line feed or NUL.
     *
     * @note **Narrowing since ticket #2064** (SR-AUD-312, cause NH-A / CCF-002). The version
     * token was **never examined at all**, so `"GARBAGE 200 OK"` yielded 200 — a response
     * that is not HTTP reported as a successful HTTP response. `std::stoi` accepted a prefix,
     * a sign and any width, so `"HTTP/1.1 200trailer OK"` was 200, `"HTTP/1.1 2 OK"` was 2
     * and `"HTTP/1.1 -5 OK"` was **−5**, which the handler then cast into
     * `System::Net::HttpStatusCode` — a public enum holding a value no enumerator names.
     */
    static ParsedStatusLine parseStatusLine(const std::string& statusLine);

private:
    std::string                                  baseAddress_;
    std::unordered_map<std::string, std::string> defaultHeaders_;
    std::shared_ptr<HttpMessageHandler>           handler_;
};

} // namespace System::Net::Http
