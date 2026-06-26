// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Net/Http/HttpMethod.hpp"
#include "System/Net/Http/HttpContent.hpp"
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
 */
class HttpClient {
public:
    HttpClient();
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
        int         port = 80;
        std::string path;
    };

    /**
     * Parses an absolute HTTP URL into its components.
     * Throws std::invalid_argument for malformed or non-HTTP URLs.
     */
    static ParsedUrl parseUrl(const std::string& url);

private:
    std::string                                  baseAddress_;
    std::unordered_map<std::string, std::string> defaultHeaders_;
};

} // namespace System::Net::Http
