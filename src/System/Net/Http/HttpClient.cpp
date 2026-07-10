// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)

// Platform-specific socket includes — must not appear in the public header.
#if defined(__EMSCRIPTEN__)
#  include "System/PlatformNotSupportedException.hpp"
#elif defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  define NOMINMAX
#  include <winsock2.h>
#  include <ws2tcpip.h>
#else // POSIX
#  include <sys/socket.h>
#  include <netdb.h>
#  include <unistd.h>
#endif

#include "System/Net/Http/HttpClient.hpp"
#include "System/Net/Http/ByteArrayContent.hpp"
#include "System/Net/Http/HttpRequestException.hpp"
#include "System/Net/Http/StringContent.hpp"
#include "System/ArgumentNullException.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace System::Net::Http {

// ---------------------------------------------------------------------------
// Platform typedefs
// ---------------------------------------------------------------------------

#if !defined(__EMSCRIPTEN__)
#  if defined(_WIN32)
using SocketFd = SOCKET;
static constexpr SocketFd kInvalidSocket = INVALID_SOCKET;
static void platformClose(SocketFd fd) { closesocket(fd); }
#  else
using SocketFd = int;
static constexpr SocketFd kInvalidSocket = -1;
static void platformClose(SocketFd fd) { ::close(fd); }
#  endif
#endif // !__EMSCRIPTEN__

// ---------------------------------------------------------------------------
// URL parser
// ---------------------------------------------------------------------------

HttpClient::ParsedUrl HttpClient::parseUrl(const std::string& url) {
    ParsedUrl result;

    size_t schemeEnd = url.find("://");
    if (schemeEnd == std::string::npos)
        throw std::invalid_argument("HttpClient: missing scheme in URL: " + url);

    result.scheme = url.substr(0, schemeEnd);
    for (char& c : result.scheme)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    if (result.scheme != "http")
        throw std::invalid_argument(
            "HttpClient: only HTTP is supported (not '" + result.scheme + "'). "
            "HTTPS requires TLS which is not yet implemented.");

    size_t hostStart = schemeEnd + 3;
    size_t pathStart = url.find('/', hostStart);

    std::string hostPort;
    if (pathStart == std::string::npos) {
        hostPort    = url.substr(hostStart);
        result.path = "/";
    } else {
        hostPort    = url.substr(hostStart, pathStart - hostStart);
        result.path = url.substr(pathStart);
    }
    if (result.path.empty()) result.path = "/";

    // Strip query from host:port field (shouldn't happen, but guard)
    size_t colonPos = hostPort.rfind(':');
    if (colonPos != std::string::npos) {
        result.host = hostPort.substr(0, colonPos);
        try {
            result.port = std::stoi(hostPort.substr(colonPos + 1));
        } catch (...) {
            throw std::invalid_argument("HttpClient: invalid port in URL: " + url);
        }
    } else {
        result.host = hostPort;
        result.port = 80;
    }

    if (result.host.empty())
        throw std::invalid_argument("HttpClient: empty host in URL: " + url);

    return result;
}

// ---------------------------------------------------------------------------
// Socket helpers (POSIX + Windows; not compiled on Emscripten)
// ---------------------------------------------------------------------------

#if !defined(__EMSCRIPTEN__)

static SocketFd connectToHost(const std::string& host, int port) {
    struct addrinfo hints{};
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* res = nullptr;
    std::string portStr  = std::to_string(port);
    int rv = getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res);
    if (rv != 0)
        throw HttpRequestException("HttpClient: DNS resolution failed for '" + host + "'.");

    SocketFd fd = kInvalidSocket;
    for (auto* p = res; p != nullptr; p = p->ai_next) {
        fd = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd == kInvalidSocket) continue;
        if (::connect(fd, p->ai_addr, static_cast<int>(p->ai_addrlen)) == 0) break;
        platformClose(fd);
        fd = kInvalidSocket;
    }
    freeaddrinfo(res);

    if (fd == kInvalidSocket)
        throw HttpRequestException(
            "HttpClient: could not connect to " + host + ":" + std::to_string(port));
    return fd;
}

static void sendAll(SocketFd fd, const std::string& data) {
    size_t sent = 0;
    while (sent < data.size()) {
        int n = static_cast<int>(
            ::send(fd, data.c_str() + sent,
                   static_cast<int>(data.size() - sent), 0));
        if (n <= 0) throw HttpRequestException("HttpClient: send() failed.");
        sent += static_cast<size_t>(n);
    }
}

// Read one \r\n-terminated line from the socket, using buf as a carry buffer.
static std::string recvLine(SocketFd fd, std::string& buf) {
    while (true) {
        size_t pos = buf.find("\r\n");
        if (pos != std::string::npos) {
            std::string line = buf.substr(0, pos);
            buf = buf.substr(pos + 2);
            return line;
        }
        char tmp[2048];
        int n = static_cast<int>(::recv(fd, tmp, sizeof(tmp), 0));
        if (n <= 0) {
            std::string line = buf;
            buf.clear();
            return line;
        }
        buf.append(tmp, static_cast<size_t>(n));
    }
}

// Read exactly `count` bytes, drawing from buf first.
static std::vector<uint8_t> recvExact(SocketFd fd, std::string& buf, size_t count) {
    while (buf.size() < count) {
        char tmp[4096];
        size_t want = std::min(sizeof(tmp), count - buf.size());
        int n = static_cast<int>(::recv(fd, tmp, static_cast<int>(want), 0));
        if (n <= 0) break;
        buf.append(tmp, static_cast<size_t>(n));
    }
    size_t take = std::min(buf.size(), count);
    std::vector<uint8_t> result(buf.begin(), buf.begin() + static_cast<long>(take));
    buf = buf.substr(take);
    return result;
}

// Read until connection closes.
static std::vector<uint8_t> recvAll(SocketFd fd, std::string& buf) {
    char tmp[4096];
    for (;;) {
        int n = static_cast<int>(::recv(fd, tmp, sizeof(tmp), 0));
        if (n <= 0) break;
        buf.append(tmp, static_cast<size_t>(n));
    }
    std::vector<uint8_t> result(buf.begin(), buf.end());
    buf.clear();
    return result;
}

static std::string strToLower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

static std::string trimValue(const std::string& s) {
    size_t start = s.find_first_not_of(" \t");
    if (start == std::string::npos) return "";
    size_t end   = s.find_last_not_of(" \t");
    return s.substr(start, end - start + 1);
}

// Core HTTP/1.1 send+receive on an already-parsed URL.
static std::shared_ptr<HttpResponseMessage> performRequest(
    const HttpClient::ParsedUrl&                        purl,
    const std::string&                                  method,
    const std::unordered_map<std::string, std::string>& defaultHeaders,
    const std::unordered_map<std::string, std::string>& reqHeaders,
    const std::shared_ptr<HttpContent>&                 content)
{
    SocketFd fd = connectToHost(purl.host, purl.port);

    // Build request body
    std::string body;
    if (content) body = content->ReadAsString();

    // Build request headers
    std::ostringstream req;
    req << method << " " << purl.path << " HTTP/1.1\r\n";
    req << "Host: " << purl.host;
    if (purl.port != 80) req << ":" << purl.port;
    req << "\r\n";
    req << "User-Agent: SharpRuntime-HttpClient/1.0\r\n";
    req << "Accept: */*\r\n";
    req << "Connection: close\r\n";

    for (const auto& [k, v] : defaultHeaders) req << k << ": " << v << "\r\n";
    for (const auto& [k, v] : reqHeaders)     req << k << ": " << v << "\r\n";

    if (content && !body.empty()) {
        std::string ct = content->getContentTypeProperty();
        std::string cs = content->getCharSetProperty();
        if (!ct.empty()) {
            req << "Content-Type: " << ct;
            if (!cs.empty()) req << "; charset=" << cs;
            req << "\r\n";
        }
        req << "Content-Length: " << body.size() << "\r\n";
    }
    req << "\r\n";
    if (!body.empty()) req << body;

    sendAll(fd, req.str());

    // Parse response
    std::string buf;

    // Status line: HTTP/1.1 200 OK
    std::string statusLine = recvLine(fd, buf);
    int         statusCode = 200;
    std::string reason;
    {
        size_t sp1 = statusLine.find(' ');
        if (sp1 != std::string::npos) {
            size_t sp2 = statusLine.find(' ', sp1 + 1);
            if (sp2 != std::string::npos) {
                statusCode = std::stoi(statusLine.substr(sp1 + 1, sp2 - sp1 - 1));
                reason     = statusLine.substr(sp2 + 1);
            } else {
                statusCode = std::stoi(statusLine.substr(sp1 + 1));
            }
        }
    }

    auto resp = std::make_shared<HttpResponseMessage>(
        static_cast<System::Net::HttpStatusCode>(statusCode));
    resp->setReasonPhraseProperty(reason);

    // Response headers
    std::string transferEncoding;
    long long   contentLength = -1;

    for (;;) {
        std::string line = recvLine(fd, buf);
        if (line.empty()) break;
        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string name  = line.substr(0, colon);
        std::string value = trimValue(line.substr(colon + 1));
        resp->setHeader(name, value);

        std::string nameLower = strToLower(name);
        if (nameLower == "transfer-encoding") transferEncoding = strToLower(value);
        if (nameLower == "content-length")    contentLength    = std::stoll(value);
    }

    // Response body
    bool noBody = (method == "HEAD")
               || (statusCode >= 100 && statusCode < 200)
               || statusCode == 204
               || statusCode == 304;

    std::vector<uint8_t> bodyBytes;

    if (!noBody) {
        if (transferEncoding == "chunked") {
            for (;;) {
                std::string chunkLine = recvLine(fd, buf);
                // Strip optional chunk extensions (;...)
                size_t semi = chunkLine.find(';');
                if (semi != std::string::npos) chunkLine = chunkLine.substr(0, semi);
                if (chunkLine.empty()) break;
                size_t chunkSize = std::stoul(chunkLine, nullptr, 16);
                if (chunkSize == 0) { recvLine(fd, buf); break; } // trailing CRLF
                auto chunk = recvExact(fd, buf, chunkSize);
                bodyBytes.insert(bodyBytes.end(), chunk.begin(), chunk.end());
                recvLine(fd, buf); // chunk-trailing CRLF
            }
        } else if (contentLength >= 0) {
            bodyBytes = recvExact(fd, buf, static_cast<size_t>(contentLength));
        } else {
            bodyBytes = recvAll(fd, buf);
        }
    }

    platformClose(fd);

    if (!bodyBytes.empty())
        resp->setContentProperty(
            std::make_shared<ByteArrayContent>(
                std::vector<SharpRuntime::bytecs>(bodyBytes.begin(), bodyBytes.end())));

    return resp;
}

#endif // !__EMSCRIPTEN__

// ---------------------------------------------------------------------------
// HttpClient public methods
// ---------------------------------------------------------------------------

HttpClient::HttpClient() {
#if defined(_WIN32) && !defined(__EMSCRIPTEN__)
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
}

HttpClient::~HttpClient() {
#if defined(_WIN32) && !defined(__EMSCRIPTEN__)
    WSACleanup();
#endif
}

std::shared_ptr<HttpResponseMessage> HttpClient::Send(
    std::shared_ptr<HttpRequestMessage> request)
{
    // Verified against HttpClient.cs's CheckRequestBeforeSend: every Send/SendAsync overload
    // validates the request is non-null before touching it. Previously this dereferenced
    // request immediately with no check -- a null-pointer dereference (UB/crash) instead of
    // a catchable exception.
    System::ArgumentNullException::ThrowIfNull(request.get(), "request");
#if defined(__EMSCRIPTEN__)
    (void)request;
    throw System::PlatformNotSupportedException(
        "HttpClient is not supported on Emscripten (no TCP sockets in single-threaded Wasm).");
#else
    std::string url = request->getRequestUriProperty();
    if (url.empty()) url = baseAddress_;
    else if (url.find("://") == std::string::npos && !baseAddress_.empty())
        url = baseAddress_ + url;

    ParsedUrl purl = parseUrl(url);
    return performRequest(purl,
                          request->getMethodProperty().getMethodProperty(),
                          defaultHeaders_,
                          request->getHeadersProperty(),
                          request->getContentProperty());
#endif
}

std::shared_ptr<HttpResponseMessage> HttpClient::Get(const std::string& url) {
    auto req = std::make_shared<HttpRequestMessage>(
        HttpMethod::Get(), url.empty() ? baseAddress_ : url);
    return Send(req);
}

std::shared_ptr<HttpResponseMessage> HttpClient::Post(
    const std::string& url, std::shared_ptr<HttpContent> content)
{
    auto req = std::make_shared<HttpRequestMessage>(
        HttpMethod::Post(), url.empty() ? baseAddress_ : url);
    req->setContentProperty(std::move(content));
    return Send(req);
}

std::string HttpClient::GetString(const std::string& url) {
    auto resp = Get(url);
    resp->EnsureSuccessStatusCode();
    return resp->getContentProperty() ? resp->getContentProperty()->ReadAsString() : "";
}

std::vector<SharpRuntime::bytecs> HttpClient::GetByteArray(const std::string& url) {
    auto resp = Get(url);
    resp->EnsureSuccessStatusCode();
    return resp->getContentProperty()
        ? resp->getContentProperty()->ReadAsByteArray()
        : std::vector<SharpRuntime::bytecs>{};
}

System::Threading::Tasks::TaskT<std::shared_ptr<HttpResponseMessage>>
HttpClient::SendAsync(std::shared_ptr<HttpRequestMessage> request) {
    return System::Threading::Tasks::TaskT<std::shared_ptr<HttpResponseMessage>>(
        [this, request]() { return Send(request); });
}

System::Threading::Tasks::TaskT<std::shared_ptr<HttpResponseMessage>>
HttpClient::GetAsync(const std::string& url) {
    return System::Threading::Tasks::TaskT<std::shared_ptr<HttpResponseMessage>>(
        [this, url]() { return Get(url); });
}

System::Threading::Tasks::TaskT<std::shared_ptr<HttpResponseMessage>>
HttpClient::PostAsync(const std::string& url, std::shared_ptr<HttpContent> content) {
    return System::Threading::Tasks::TaskT<std::shared_ptr<HttpResponseMessage>>(
        [this, url, content]() { return Post(url, content); });
}

System::Threading::Tasks::TaskT<std::string>
HttpClient::GetStringAsync(const std::string& url) {
    return System::Threading::Tasks::TaskT<std::string>(
        [this, url]() { return GetString(url); });
}

System::Threading::Tasks::TaskT<std::vector<SharpRuntime::bytecs>>
HttpClient::GetByteArrayAsync(const std::string& url) {
    return System::Threading::Tasks::TaskT<std::vector<SharpRuntime::bytecs>>(
        [this, url]() { return GetByteArray(url); });
}

void HttpClient::setDefaultHeader(const std::string& name, const std::string& value) {
    defaultHeaders_[name] = value;
}

std::string HttpClient::getDefaultHeader(const std::string& name) const {
    auto it = defaultHeaders_.find(name);
    return it != defaultHeaders_.end() ? it->second : "";
}

} // namespace System::Net::Http
