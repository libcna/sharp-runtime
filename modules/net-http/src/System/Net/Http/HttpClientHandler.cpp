// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)

// Platform-specific socket includes — must not appear in the public header. Moved verbatim
// from HttpClient.cpp when the socket-level send/receive logic was extracted into this
// dedicated HttpMessageHandler implementation (ticket 1496).
#if defined(__EMSCRIPTEN__)
#  include "System/PlatformNotSupportedException.hpp"
#elif defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <winsock2.h>
#  include <ws2tcpip.h>
#else // POSIX
#  include <sys/socket.h>
#  include <netdb.h>
#  include <unistd.h>
#endif

#include "System/Net/Http/HttpClientHandler.hpp"
#include "System/Net/Http/ByteArrayContent.hpp"
#include "System/Net/Http/HttpClient.hpp"
#include "System/Net/Http/HttpRequestException.hpp"
#include "System/Net/Http/detail/HttpFieldValidation.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/Uri.hpp"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <sstream>
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
// Socket helpers (POSIX + Windows; not compiled on Emscripten)
// ---------------------------------------------------------------------------

#if !defined(__EMSCRIPTEN__)

// Owns the connected descriptor for the rest of Send().
//
// Before ticket #2065 the descriptor was a bare local and `platformClose(fd)` was reached at
// exactly ONE point -- after the whole response body had been read -- so every throw between
// connect and that line leaked it. All four of those throws are chosen by the REMOTE PEER: a
// garbled status line, a malformed Content-Length, a malformed chunk size, and a body shorter
// than the declared Content-Length. Measured, 20 requests in each of those four modes leaked
// 20 descriptors, so a server answering ~1,024 requests with a garbled status line exhausts a
// default RLIMIT_NOFILE. The success path leaked none.
//
// SR-AUD-318's leak half; see docs/SystemNetHttpNamespaceReviewPlan.md §4.7. Note that this
// deliberately keeps the ORIGINAL close point on the success path -- Close() is still called
// exactly where `platformClose(fd)` used to be -- so the only behaviour that changes is that
// a failing path now closes too. `Close()` is idempotent, so the destructor is a no-op after
// it has run.
class SocketGuard {
    SocketFd fd_;

public:
    explicit SocketGuard(SocketFd fd) noexcept : fd_(fd) {}
    ~SocketGuard() { Close(); }

    SocketGuard(const SocketGuard&)            = delete;
    SocketGuard& operator=(const SocketGuard&) = delete;
    SocketGuard(SocketGuard&&)                 = delete;
    SocketGuard& operator=(SocketGuard&&)      = delete;

    [[nodiscard]] SocketFd get() const noexcept { return fd_; }

    void Close() noexcept {
        if (fd_ == kInvalidSocket) return;
        platformClose(fd_);
        fd_ = kInvalidSocket;
    }
};

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
//
// If the connection closes (recv() returns <= 0) before `count` bytes have arrived, this
// throws rather than returning the truncated data collected so far -- a dropped connection
// mid-response (e.g. server crash, network failure while streaming a Content-Length-bounded or
// chunked body) must surface as a catchable error, not a "successful" but incomplete response.
// Real .NET surfaces this as an IOException with HttpRequestError.ResponseEnded ("The response
// ended prematurely."); mirrored here via the matching HttpRequestException overload for
// consistency with every other malformed-response failure path in this file.
// #2071: the ceiling every accumulating read is checked against. Raised as
// HttpRequestError::ConfigurationLimitExceeded, which is the error .NET raises for the same
// condition (HttpContent.cs:780).
[[noreturn]] static void throwResponseTooLarge(SharpRuntime::longcs limit) {
    throw HttpRequestException(
        HttpRequestError::ConfigurationLimitExceeded,
        "HttpClient: the response body exceeds MaxResponseContentBufferSize (" +
            std::to_string(limit) + " bytes).");
}

static std::vector<uint8_t> recvExact(SocketFd fd, std::string& buf, size_t count,
                                       SharpRuntime::longcs limit) {
    // #2071: checked BEFORE a single byte is read, so a Content-Length or chunk-size header
    // claiming more than the ceiling costs nothing to reject. That matters: the old code would
    // happily begin accumulating toward a number the server never had to back with real bytes.
    if (count > static_cast<size_t>(limit)) throwResponseTooLarge(limit);
    while (buf.size() < count) {
        char tmp[4096];
        size_t want = std::min(sizeof(tmp), count - buf.size());
        int n = static_cast<int>(::recv(fd, tmp, static_cast<int>(want), 0));
        if (n <= 0) {
            throw HttpRequestException(HttpRequestError::ResponseEnded,
                "HttpClient: connection closed before the full response body was received.");
        }
        buf.append(tmp, static_cast<size_t>(n));
    }
    size_t take = std::min(buf.size(), count);
    std::vector<uint8_t> result(buf.begin(), buf.begin() + static_cast<long>(take));
    buf = buf.substr(take);
    return result;
}

// Read until connection closes.
static std::vector<uint8_t> recvAll(SocketFd fd, std::string& buf, SharpRuntime::longcs limit) {
    char tmp[4096];
    for (;;) {
        int n = static_cast<int>(::recv(fd, tmp, sizeof(tmp), 0));
        if (n <= 0) break;
        buf.append(tmp, static_cast<size_t>(n));
        // #2071: a response with no Content-Length and no chunking is read until the peer hangs
        // up, so this is the one path with no declared size at all -- the ceiling is checked per
        // chunk read rather than up front.
        if (buf.size() > static_cast<size_t>(limit)) throwResponseTooLarge(limit);
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

#endif // !defined(__EMSCRIPTEN__)

// ---------------------------------------------------------------------------
// HttpClientHandler
// ---------------------------------------------------------------------------

HttpClientHandler::HttpClientHandler() {
#if defined(_WIN32) && !defined(__EMSCRIPTEN__)
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
}

HttpClientHandler::~HttpClientHandler() {
    Dispose();
}

void HttpClientHandler::Dispose() {
    if (disposed_) return;
    disposed_ = true;
#if defined(_WIN32) && !defined(__EMSCRIPTEN__)
    WSACleanup();
#endif
}

std::shared_ptr<HttpResponseMessage> HttpClientHandler::Send(std::shared_ptr<HttpRequestMessage> request) {
    ThrowIfDisposed();
    System::ArgumentNullException::ThrowIfNull(request.get(), "request");
#if defined(__EMSCRIPTEN__)
    (void)request;
    throw System::PlatformNotSupportedException(
        "HttpClient is not supported on Emscripten (no TCP sockets in single-threaded Wasm).");
#else
    const std::string& absoluteUrl = request->getRequestUriProperty();
    HttpClient::ParsedUrl purl = HttpClient::parseUrl(absoluteUrl);

    // Cookie support (ticket 1498): attach a Cookie request header built from the container
    // for this URI, unless the caller already set one explicitly.
    std::unordered_map<std::string, std::string> reqHeaders = request->getHeadersProperty();
    std::unique_ptr<System::Uri> uri;
    if (useCookies_ && cookieContainer_ && reqHeaders.find("Cookie") == reqHeaders.end()) {
        uri = std::make_unique<System::Uri>(absoluteUrl);
        std::string cookieHeader = cookieContainer_->GetCookieHeader(*uri);
        // Ticket #2063 (SR-AUD-313, cause NH-B). Every other entry of reqHeaders came through
        // HttpRequestMessage::setHeader or HttpClient::setDefaultHeader and is already
        // validated; this one is synthesised HERE out of cookie state that a previous
        // response's Set-Cookie put there, so it is the one request header value on this path
        // with no public door in front of it. CookieContainer lives in another component with
        // its own open findings, so this guard does not assume anything about what it stores.
        detail::ThrowIfControlCharacter(cookieHeader, "Cookie header value");
        if (!cookieHeader.empty()) reqHeaders["Cookie"] = cookieHeader;
    }

    // #2071: read once, before any I/O, so the ceiling in force for this exchange cannot move
    // underneath it if another thread reconfigures the handler mid-request.
    const SharpRuntime::longcs maxBody = getMaxResponseContentBufferSizeProperty();

    // The guard, not the control flow, owns this descriptor from here on (#2065).
    SocketGuard socket(connectToHost(purl.host, purl.port));
    const SocketFd fd = socket.get();

    // Build request body
    std::shared_ptr<HttpContent> content = request->getContentProperty();
    std::string body;
    if (content) body = content->ReadAsString();

    // Build request headers
    std::string method = request->getMethodProperty().getMethodProperty();
    std::ostringstream req;
    req << method << " " << purl.path << " HTTP/1.1\r\n";

    // Ticket #2068, the half the finding does not name. These four fields were written
    // UNCONDITIONALLY, before the caller's map, so a caller who set `Host` sent TWO Host fields
    // in one request -- which is request-smuggling-adjacent, because two intermediaries may pick
    // different ones. Each is now a DEFAULT: emitted only when the caller supplied nothing with
    // that name, compared case-insensitively. That comparison is why this half belongs to this
    // ticket rather than to a compatible one -- it needs the lookup this ticket introduces.
    //
    // .NET does the same: HttpConnection.WriteHeadersAsync writes its own Host only when the
    // request carries none.
    if (!detail::HasField(reqHeaders, "Host")) {
        req << "Host: " << purl.host;
        if (purl.port != 80) req << ":" << purl.port;
        req << "\r\n";
    }
    if (!detail::HasField(reqHeaders, "User-Agent")) {
        req << "User-Agent: SharpRuntime-HttpClient/1.0\r\n";
    }
    if (!detail::HasField(reqHeaders, "Accept")) req << "Accept: */*\r\n";
    if (!detail::HasField(reqHeaders, "Connection")) req << "Connection: close\r\n";

    // Ticket #2128. RFC 9112 6.1 requires Content-Length to be ignored, or the message rejected,
    // when Transfer-Encoding is present -- a request carrying both is the exact shape a
    // request-smuggling chain relies on two intermediaries disagreeing about. The header
    // COLLECTIONS cannot enforce this and .NET's do not try: Transfer-Encoding is a request
    // header and Content-Length a content header, they live in two collections, and neither can
    // see the other. .NET resolves it where the message is written, so this is where the port
    // resolves it too.
    //
    // Two rules, both applied to this handler's OWN Content-Length line, which is derived from
    // the body it is about to send:
    //   - it is suppressed when the caller declared Transfer-Encoding;
    //   - it is suppressed when the caller already supplied a Content-Length, which would
    //     otherwise emit the field twice in one message.
    // #2068 replaced this file's local `namesHeader` lambda with the shared
    // detail::FieldNamesMatch, so the case-insensitive rule has ONE definition rather than one
    // per site -- which is what let the Host/User-Agent/Accept/Connection defaults above reuse it.
    const bool callerDeclaredTransferEncoding = detail::HasField(reqHeaders, "Transfer-Encoding");
    const bool callerDeclaredContentLength    = detail::HasField(reqHeaders, "Content-Length");

    for (const auto& [k, v] : reqHeaders) req << k << ": " << v << "\r\n";

    if (content && !body.empty()) {
        std::string ct = content->getContentTypeProperty();
        std::string cs = content->getCharSetProperty();
        if (!ct.empty()) {
            req << "Content-Type: " << ct;
            if (!cs.empty()) req << "; charset=" << cs;
            req << "\r\n";
        }
        if (!callerDeclaredTransferEncoding && !callerDeclaredContentLength) {
            req << "Content-Length: " << body.size() << "\r\n";
        }
    }
    req << "\r\n";
    if (!body.empty()) req << body;

    sendAll(fd, req.str());

    // Parse response
    std::string buf;

    // Status line: HTTP/1.1 200 OK
    std::string statusLine = recvLine(fd, buf);
    HttpClient::ParsedStatusLine parsedStatus = HttpClient::parseStatusLine(statusLine);
    int statusCode = parsedStatus.statusCode;

    auto resp = std::make_shared<HttpResponseMessage>(
        static_cast<System::Net::HttpStatusCode>(statusCode));
    resp->setReasonPhraseProperty(parsedStatus.reason);

    // Response headers
    std::string transferEncoding;
    long long   contentLength = -1;
    std::vector<std::string> setCookieValues;

    for (;;) {
        std::string line = recvLine(fd, buf);
        if (line.empty()) break;
        // Ticket #2063 (SR-AUD-313, cause NH-B). recvLine() has already consumed the
        // terminating CRLF, so a CR, LF or NUL still inside the line means the server sent a
        // malformed header field. HttpResponseMessage::setHeader would reject it below with
        // System::FormatException; rejecting it here instead keeps a malformed RESPONSE
        // reported as this module's response-error type, exactly like a malformed
        // Content-Length or chunk size. The line is not echoed -- it is remote-controlled and
        // this message may be logged.
        if (detail::ContainsProtocolControlCharacter(line))
            throw HttpRequestException(
                "HttpClient: a response header line contains a carriage return, a line feed or "
                "a NUL character.");
        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string name  = line.substr(0, colon);
        std::string value = trimValue(line.substr(colon + 1));
        resp->setHeader(name, value);

        std::string nameLower = strToLower(name);
        if (nameLower == "transfer-encoding") transferEncoding = strToLower(value);
        if (nameLower == "set-cookie") {
            // A response may carry several Set-Cookie headers; HttpResponseMessage's header
            // map (like real .NET's dictionary-of-string) stores one value per name, so the
            // last one would otherwise silently overwrite the rest. Collect every occurrence
            // here specifically for cookie extraction below, in addition to the single-value
            // resp->setHeader() call above (kept for the general getHeader("Set-Cookie") case).
            setCookieValues.push_back(value);
        }
        if (nameLower == "content-length") {
            // Unguarded std::stoll() on a server-controlled value -- a malformed or
            // out-of-range Content-Length (e.g. non-numeric, or too large for long long) threw
            // a raw std::invalid_argument/std::out_of_range straight out of Send()/GetAsync()/
            // etc., invisible to code catching System::Exception&/HttpRequestException&. Same
            // bug class parseUrl()/parseStatusLine() were already fixed for; this one was
            // missed.
            try {
                contentLength = std::stoll(value);
            } catch (...) {
                throw HttpRequestException("HttpClient: malformed Content-Length header: '" + value + "'");
            }
        }
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
                // Same unguarded-std::sto*-on-server-controlled-input bug as the Content-Length
                // header above: a malformed (non-hex) or out-of-range chunk-size line threw a
                // raw std::invalid_argument/std::out_of_range instead of a clean
                // HttpRequestException.
                size_t chunkSize;
                try {
                    // #2071: std::stoul accepts up to SIZE_MAX, so "FFFFFFFFFFFFFFFF" used to
                    // become a request to accumulate 18 exabytes. recvExact rejects it against
                    // the ceiling below, but the parse itself is bounded here too so a value no
                    // size_t can hold raises the same FormatException path as any other
                    // malformed chunk size rather than throwing std::out_of_range.
                    chunkSize = std::stoul(chunkLine, nullptr, 16);
                } catch (...) {
                    throw HttpRequestException("HttpClient: malformed chunk size: '" + chunkLine + "'");
                }
                if (chunkSize == 0) { recvLine(fd, buf); break; } // trailing CRLF
                auto chunk = recvExact(fd, buf, chunkSize, maxBody);
                bodyBytes.insert(bodyBytes.end(), chunk.begin(), chunk.end());
                recvLine(fd, buf); // chunk-trailing CRLF
            }
        } else if (contentLength >= 0) {
            bodyBytes = recvExact(fd, buf, static_cast<size_t>(contentLength), maxBody);
        } else {
            bodyBytes = recvAll(fd, buf, maxBody);
        }
    }

    socket.Close();

    if (!bodyBytes.empty())
        resp->setContentProperty(
            std::make_shared<ByteArrayContent>(
                std::vector<SharpRuntime::bytecs>(bodyBytes.begin(), bodyBytes.end())));

    if (useCookies_ && cookieContainer_ && !setCookieValues.empty()) {
        if (!uri) uri = std::make_unique<System::Uri>(absoluteUrl);
        for (const auto& sc : setCookieValues) {
            try {
                cookieContainer_->SetCookies(*uri, sc);
            } catch (const System::Net::CookieException&) {
                // A malformed Set-Cookie header from the server is ignored, matching real
                // .NET's lenient CookieContainer.SetCookies behavior -- it must not fail the
                // whole request just because one cookie attribute string was garbled.
            }
        }
    }

    return resp;
#endif
}

} // namespace System::Net::Http
