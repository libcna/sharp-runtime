// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)

#include "System/Net/Http/HttpClient.hpp"
#include <condition_variable>
#include <memory>
#include <mutex>
#include "System/Net/Http/ByteArrayContent.hpp"
#include "System/Net/Http/HttpClientHandler.hpp"
#include "System/Net/Http/HttpRequestException.hpp"
#include "System/Net/Http/StringContent.hpp"
#include "System/Net/Http/detail/HttpFieldValidation.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/NotSupportedException.hpp"
#include "System/UriFormatException.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace System::Net::Http {

namespace {

// Ticket #2064 (SR-AUD-311 + SR-AUD-312, cause NH-A / CCF-002). `std::sto*` stops at the
// first character it cannot use and reports SUCCESS for the prefix it did consume, and it
// additionally skips leading whitespace and accepts a sign. Measured before this ticket:
// "80abc" -> 80, " 80" -> 80, "+80" -> 80, "-1" -> -1, "99999" -> 99999. Nothing checked
// that the whole field had been consumed, which is exactly CCF-002's shape -- the family
// already closed in System::DateTime, TimeOnly and XmlConvert by routing through a
// full-consumption scanner.
//
// This is the module-local equivalent: the ENTIRE text must be one to `maxDigits` ASCII
// digits and nothing else, and the value must fall inside [0, `maxValue`]. No sign, no
// whitespace, no prefix, no tail. Accumulation is bounded by the digit cap and compared
// against `maxValue`, so it cannot overflow whatever `maxValue`'s type is.
[[nodiscard]] bool tryParseWholeUnsignedField(const std::string& text,
                                              size_t maxDigits,
                                              long long maxValue,
                                              long long& valueOut) {
    if (text.empty() || text.size() > maxDigits) return false;
    long long value = 0;
    for (unsigned char c : text) {
        if (c < '0' || c > '9') return false;
        value = value * 10 + (c - '0');
    }
    if (value > maxValue) return false;
    valueOut = value;
    return true;
}

// A DNS name and an IPv6 literal are both case-insensitive; the scheme was already
// lowercased here and the host was not, so `HOST.EXAMPLE` and `host.example` were two
// distinct hosts to every downstream consumer -- the cookie container, the `Host:` header a
// server compares, and any cache keyed on the parsed result. RFC 3986 §3.2.2. This is the
// review's premise correction 6.3.
[[nodiscard]] std::string toLowerAscii(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

} // namespace

// ---------------------------------------------------------------------------
// URL parser
// ---------------------------------------------------------------------------

// Verified against real .NET: a malformed request URI fails Uri construction with
// System.UriFormatException (a FormatException); requesting a scheme HttpClient can't
// dispatch (here, anything but "http" -- HTTPS/TLS is out of scope for this runtime, see
// CLAUDE.md's documented deviations) is a System.NotSupportedException-shaped failure, not a
// format error. Previously every failure path here threw std::invalid_argument -- an
// unrelated std:: exception type invisible to code catching System::Exception&/
// System::FormatException&, escaping straight out of a public API.
HttpClient::ParsedUrl HttpClient::parseUrl(const std::string& url) {
    ParsedUrl result;

    // Ticket #2063 (SR-AUD-313's third and fourth vectors, cause NH-B). The whole URL is
    // checked ONCE, here, before any splitting -- deliberately not per-component. Every
    // component this parser produces is concatenated into the request frame by
    // HttpClientHandler::Send: the host into `Host: `, the path into the request LINE. Both
    // were open before this check:
    //
    //   parseUrl("http://ho\r\nst/p")   -> host "ho\r\nst"   -> two header fields
    //   parseUrl("http://host/pa\r\nX: y") -> path "/pa\r\nX: y" -> a second REQUEST LINE
    //
    // The second of those is request smuggling, and the namespace review named only the
    // first; see docs/SystemNetHttpNamespaceReviewPlan.md §20.3. Validating the raw input
    // rather than the parsed pieces is also what makes ticket #2064's authority/query/fragment
    // re-split safe by construction: no later regrouping of these bytes can reintroduce a
    // control character the whole string does not contain.
    //
    // System::UriFormatException (which IS a System::FormatException) rather than a plain
    // FormatException, because that is the type this parser already documents and throws for
    // every other malformed URL.
    if (detail::ContainsProtocolControlCharacter(url))
        throw System::UriFormatException(
            "HttpClient: the request URI must not contain a carriage return, a line feed or a "
            "NUL character.");

    size_t schemeEnd = url.find("://");
    if (schemeEnd == std::string::npos)
        throw System::UriFormatException("HttpClient: missing scheme in URL: " + url);

    result.scheme = url.substr(0, schemeEnd);
    for (char& c : result.scheme)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    if (result.scheme != "http")
        throw System::NotSupportedException(
            "HttpClient: only HTTP is supported (not '" + result.scheme + "'). "
            "HTTPS requires TLS which is not yet implemented.");

    // Ticket #2064 (SR-AUD-311, cause NH-A). The authority ends at the FIRST of '/', '?' or
    // '#', not at the first '/'. Splitting only on '/' made a query string or a fragment part
    // of the host: measured, `http://host?q=1` parsed to host `host?q=1` with path `/`, so the
    // query reached DNS *and* the `Host:` header while the request line asked for `/`. A
    // caller writing `client.Get("http://api.example?key=secret")` therefore put the secret in
    // the Host header and requested the wrong resource. That is a VALUE change rather than a
    // rejection, which is what makes it the more dangerous half of this finding to leave
    // alone (review plan §4.2).
    const size_t hostStart    = schemeEnd + 3;
    const size_t authorityEnd = url.find_first_of("/?#", hostStart);

    std::string authority;
    std::string requestTarget;
    if (authorityEnd == std::string::npos) {
        authority     = url.substr(hostStart);
        requestTarget = "/";
    } else {
        authority = url.substr(hostStart, authorityEnd - hostStart);
        // A URL whose authority is followed directly by a query has an implicit empty path,
        // and the request target is "/" + the query. A fragment is NEVER part of a request
        // target (RFC 9110 §7.1: it is client-side only and is not sent), so it is dropped
        // here rather than concatenated into the request line -- for a fragment after a path
        // as well as for one directly after the authority, which is the same rule applied
        // consistently (plan §20.6).
        requestTarget = (url[authorityEnd] == '?') ? "/" + url.substr(authorityEnd)
                      : (url[authorityEnd] == '#') ? std::string("/")
                                                   : url.substr(authorityEnd);
        const size_t fragmentStart = requestTarget.find('#');
        if (fragmentStart != std::string::npos) requestTarget.resize(fragmentStart);
    }
    if (requestTarget.empty()) requestTarget = "/";
    result.path = requestTarget;

    std::string portText;
    bool        hasPort = false;

    if (!authority.empty() && authority[0] == '[') {
        // IPv6 literal in bracket notation, e.g. "[::1]" or "[::1]:8080". A bare
        // `rfind(':')` split is wrong here -- an IPv6 address itself contains colons, so
        // splitting on the last colon in "[::1]" (no port) grabs the second colon of the
        // address instead of a port separator, corrupting both host and port.
        size_t closeBracket = authority.find(']');
        if (closeBracket == std::string::npos)
            throw System::UriFormatException("HttpClient: unterminated IPv6 literal in URL: " + url);
        result.host = authority.substr(1, closeBracket - 1);
        if (closeBracket + 1 < authority.size() && authority[closeBracket + 1] == ':') {
            portText = authority.substr(closeBracket + 2);
            hasPort  = true;
        }
    } else {
        size_t colonPos = authority.rfind(':');
        if (colonPos != std::string::npos) {
            result.host = authority.substr(0, colonPos);
            portText    = authority.substr(colonPos + 1);
            hasPort     = true;
        } else {
            result.host = authority;
        }
    }

    if (hasPort) {
        // A TCP port is a 16-bit unsigned number: 0..65535, at most five digits, no sign and
        // no tail. Before this ticket `std::stoi` accepted every one of `80abc`, `-1`, ` 80`,
        // `+80` and `99999` (review plan §4.2). The empty port text of `http://host:` was
        // already rejected -- by `std::stoi` throwing -- and still is, by the same rule that
        // rejects the rest.
        long long port = 0;
        if (!tryParseWholeUnsignedField(portText, 5, 65535, port))
            throw System::UriFormatException("HttpClient: invalid port in URL: " + url);
        result.port = static_cast<SharpRuntime::intcs>(port);
    } else {
        result.port = 80;
    }

    if (result.host.empty())
        throw System::UriFormatException("HttpClient: empty host in URL: " + url);

    result.host = toLowerAscii(std::move(result.host));

    return result;
}

// A status line that can't be parsed (no space to find the code, or a non-numeric code) must
// not silently report success -- this previously defaulted the status code to 200 (OK)
// whenever the line had no space at all, making a garbled/empty response from the server look
// exactly like a successful one to calling code.
HttpClient::ParsedStatusLine HttpClient::parseStatusLine(const std::string& statusLine) {
    ParsedStatusLine result;

    // Ticket #2063 (SR-AUD-316's reason half, cause NH-B). recvLine() strips the terminating
    // CRLF, so a CR, LF or NUL still present inside the line is a malformed status line the
    // server sent -- and the reason phrase is handed straight to
    // HttpResponseMessage::setReasonPhraseProperty, a public door that now rejects exactly
    // those characters. Rejecting here, with THIS module's response-error type, is what keeps
    // a malformed *response* surfacing as HttpRequestException rather than as the
    // FormatException the public setter raises for a direct caller's bad argument.
    // The line is not echoed into the message: it is remote-controlled and messages get
    // logged, so echoing it would recreate the injection in the log.
    if (detail::ContainsProtocolControlCharacter(statusLine))
        throw HttpRequestException(
            "HttpClient: the response status line contains a carriage return, a line feed or a "
            "NUL character.");

    size_t sp1 = statusLine.find(' ');
    if (sp1 == std::string::npos)
        throw HttpRequestException("HttpClient: malformed status line: '" + statusLine + "'");

    // Ticket #2064 (SR-AUD-312, cause NH-A). The version token was never examined AT ALL --
    // the review's premise correction 6.2. Measured before this ticket, `GARBAGE 200 OK`
    // yielded 200, so a response that is not HTTP at all was reported as a successful HTTP
    // response. There was nothing to make stricter here; there was a check to ADD.
    //
    // RFC 9112 §2.3: HTTP-version = "HTTP" "/" DIGIT "." DIGIT -- exactly one digit each, so
    // `HTTP/11` and `HTTP/1.10` are malformed. `HTTP/9.9` satisfies the grammar and is
    // ACCEPTED: this port speaks 1.1, but a version it does not know is the server's
    // behaviour to report, not a parse error, and no repository-contained evidence says .NET
    // rejects it. Recorded as this port's choice and pinned, per plan §15.
    const std::string versionToken = statusLine.substr(0, sp1);
    const bool versionWellFormed =
        versionToken.size() == 8
        && versionToken.compare(0, 5, "HTTP/") == 0
        && versionToken[5] >= '0' && versionToken[5] <= '9'
        && versionToken[6] == '.'
        && versionToken[7] >= '0' && versionToken[7] <= '9';
    if (!versionWellFormed)
        throw HttpRequestException(
            "HttpClient: malformed HTTP version in status line: '" + statusLine + "'");

    size_t sp2 = statusLine.find(' ', sp1 + 1);
    std::string codeStr = (sp2 != std::string::npos)
        ? statusLine.substr(sp1 + 1, sp2 - sp1 - 1)
        : statusLine.substr(sp1 + 1);

    // RFC 9112 §4: status-code = 3DIGIT. `std::stoi` accepted a prefix, a sign and any
    // width, so `200trailer` was 200, `2` was 2, `-5` was -5 and `99999` was 99999 -- and the
    // caller casts the result into System::Net::HttpStatusCode, a PUBLIC enum, so `-5`
    // produced an enum value no enumerator names. Exactly three ASCII digits, nothing else.
    //
    // `099` IS accepted, as the status code 99: it is exactly three digits, which is all the
    // grammar asks for. Whether .NET agrees is NOT known here (/rv/tmp/runtime/ absent), so
    // this is recorded as this port's choice and PINNED either way rather than left open
    // (plan §11's requirement for this row, §15's deferred-evidence rule).
    long long code = 0;
    if (!tryParseWholeUnsignedField(codeStr, 3, 999, code) || codeStr.size() != 3)
        throw HttpRequestException("HttpClient: malformed status line: '" + statusLine + "'");
    result.statusCode = static_cast<SharpRuntime::intcs>(code);

    if (sp2 != std::string::npos) result.reason = statusLine.substr(sp2 + 1);
    return result;
}

// ---------------------------------------------------------------------------
// HttpClient public methods
// ---------------------------------------------------------------------------

// The actual socket-level send/receive implementation lives in HttpClientHandler
// (System::Net::Http::HttpClientHandler, see HttpClientHandler.cpp) so that HttpClient can be
// pointed at a custom HttpMessageHandler/DelegatingHandler chain instead (ticket 1496).

HttpClient::HttpClient() : handler_(std::make_shared<HttpClientHandler>()),
                           asyncOps_(std::make_shared<AsyncOperations>()) {}

HttpClient::HttpClient(std::shared_ptr<HttpMessageHandler> handler)
    : handler_(std::move(handler)), asyncOps_(std::make_shared<AsyncOperations>()) {
    System::ArgumentNullException::ThrowIfNull(handler_.get(), "handler");
}

HttpClient::~HttpClient() { waitForAsyncOperations(); }   // #2066: see the header

std::shared_ptr<HttpResponseMessage> HttpClient::Send(
    std::shared_ptr<HttpRequestMessage> request)
{
    // Verified against HttpClient.cs's CheckRequestBeforeSend: every Send/SendAsync overload
    // validates the request is non-null before touching it. Previously this dereferenced
    // request immediately with no check -- a null-pointer dereference (UB/crash) instead of
    // a catchable exception.
    System::ArgumentNullException::ThrowIfNull(request.get(), "request");

    std::string url = request->getRequestUriProperty();
    if (url.empty()) url = baseAddress_;
    else if (url.find("://") == std::string::npos && !baseAddress_.empty())
        url = baseAddress_ + url;
    request->setRequestUriProperty(url);

    // DefaultRequestHeaders are merged onto the request here (client-level defaults never
    // override a header the caller already set on this specific request) before handing off
    // to the handler chain -- matching real .NET's HttpClient applying DefaultRequestHeaders
    // ahead of invoking the handler pipeline.
    for (const auto& [k, v] : defaultHeaders_) {
        if (request->getHeadersProperty().find(k) == request->getHeadersProperty().end())
            request->setHeader(k, v);
    }

    return handler_->Send(std::move(request));
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

// ---------------------------------------------------------------------------------------------
// #2066 / SR-AUD-310 (CCF-019) — the liveness boundary for the five *Async members.
//
// Each builds a TaskT from a lambda capturing raw `this`, and TaskT dispatches immediately, so a
// task outliving its client read a freed defaultHeaders_ and a freed handler_. .NET needs no
// boundary because the GC keeps the client alive for as long as the captured delegate can reach
// it; C++ has no such mechanism, so the destructor waits -- the same shape #2134 gave Socket and
// #2347 gave FileSystemWatcher.
//
// Note what is NOT here, deliberately: Socket needed a stop flag and a polling accept because
// shutdown() cannot unblock accept(). A request has no equivalent -- the socket it uses is
// created inside the handler and is not reachable from this object -- so the bound is the
// request's own connect/read timeout and there is nothing to interrupt.
struct HttpClient::AsyncOperations {
    std::mutex              mutex;
    std::condition_variable idle;
    int                     inFlight = 0;
};

// Constructed INSIDE each async body rather than captured by it: std::async keeps the callable
// alive until the last future to its shared state is destroyed, so a guard captured by the
// lambda is released when the CALLER drops the TaskT, not when the work finishes -- a boundary
// built on that waits for the caller and deadlocks. #2134 hit this and it is written down here
// so the next member added to this file does not hit it again.
struct HttpClient::AsyncOperationScope {
    std::shared_ptr<AsyncOperations> ops;
    ~AsyncOperationScope() {
        if (!ops) return;
        {
            std::lock_guard<std::mutex> lock(ops->mutex);
            --ops->inFlight;
        }
        ops->idle.notify_all();
    }
};

std::shared_ptr<HttpClient::AsyncOperations> HttpClient::beginAsyncOperation() {
    auto ops = asyncOps_;
    {
        std::lock_guard<std::mutex> lock(ops->mutex);
        ++ops->inFlight;
    }
    return ops;
}

void HttpClient::waitForAsyncOperations() noexcept {
    if (!asyncOps_) return;
    std::unique_lock<std::mutex> lock(asyncOps_->mutex);
    asyncOps_->idle.wait(lock, [this] { return asyncOps_->inFlight == 0; });
}

System::Threading::Tasks::TaskT<std::shared_ptr<HttpResponseMessage>>
HttpClient::SendAsync(std::shared_ptr<HttpRequestMessage> request) {
    auto ops = beginAsyncOperation();
    return System::Threading::Tasks::TaskT<std::shared_ptr<HttpResponseMessage>>([this, request, ops]() {
        AsyncOperationScope release{ops};
        return Send(request);
    });
}

System::Threading::Tasks::TaskT<std::shared_ptr<HttpResponseMessage>>
HttpClient::GetAsync(const std::string& url) {
    auto ops = beginAsyncOperation();
    return System::Threading::Tasks::TaskT<std::shared_ptr<HttpResponseMessage>>([this, url, ops]() {
        AsyncOperationScope release{ops};
        return Get(url);
    });
}

System::Threading::Tasks::TaskT<std::shared_ptr<HttpResponseMessage>>
HttpClient::PostAsync(const std::string& url, std::shared_ptr<HttpContent> content) {
    auto ops = beginAsyncOperation();
    return System::Threading::Tasks::TaskT<std::shared_ptr<HttpResponseMessage>>([this, url, content, ops]() {
        AsyncOperationScope release{ops};
        return Post(url, content);
    });
}

System::Threading::Tasks::TaskT<std::string>
HttpClient::GetStringAsync(const std::string& url) {
    auto ops = beginAsyncOperation();
    return System::Threading::Tasks::TaskT<std::string>([this, url, ops]() {
        AsyncOperationScope release{ops};
        return GetString(url);
    });
}

System::Threading::Tasks::TaskT<std::vector<SharpRuntime::bytecs>>
HttpClient::GetByteArrayAsync(const std::string& url) {
    auto ops = beginAsyncOperation();
    return System::Threading::Tasks::TaskT<std::vector<SharpRuntime::bytecs>>([this, url, ops]() {
        AsyncOperationScope release{ops};
        return GetByteArray(url);
    });
}

// Ticket #2063 (SR-AUD-313, cause NH-B). A default header is merged onto every request this
// client sends, so it reaches the wire through exactly the same concatenation as a per-request
// header and needs the same rejection. Doing it only on HttpRequestMessage::setHeader would
// have left the client-wide door open.
void HttpClient::setDefaultHeader(const std::string& name, const std::string& value) {
    detail::ThrowIfControlCharacter(name, "header name");
    detail::ThrowIfControlCharacter(value, "header value");
    defaultHeaders_[name] = value;
}

std::string HttpClient::getDefaultHeader(const std::string& name) const {
    auto it = defaultHeaders_.find(name);
    return it != defaultHeaders_.end() ? it->second : "";
}

} // namespace System::Net::Http
