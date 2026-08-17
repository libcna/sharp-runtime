// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Net/WebSockets/ClientWebSocket.hpp"
#include <condition_variable>
#include <memory>
#include <mutex>
#include <array>
#include <cstring>
#include <map>
#include <random>
#include "System/ArgumentException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Convert.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/Net/IPEndPoint.hpp"
#include "System/Net/detail/ProtocolFieldValidation.hpp"
#include "System/Net/WebSockets/WebSocketException.hpp"
#include "System/PlatformNotSupportedException.hpp"

namespace System::Net::WebSockets {

using SharpRuntime::bytecs;
using SharpRuntime::intcs;

namespace {

    // --- Minimal, self-contained SHA-1 (RFC 3174) -------------------------------------------
    // Not System::Security::Cryptography::SHA1 (that namespace is not yet ported/decided) —
    // this is a private implementation detail needed only for the Sec-WebSocket-Accept digest
    // in the RFC 6455 handshake, kept file-local rather than exposed as a public crypto API.
    std::array<uint8_t, 20> sha1(const std::string& input) {
        uint32_t h0 = 0x67452301, h1 = 0xEFCDAB89, h2 = 0x98BADCFE, h3 = 0x10325476, h4 = 0xC3D2E1F0;

        std::vector<uint8_t> msg(input.begin(), input.end());
        uint64_t bitLen = static_cast<uint64_t>(msg.size()) * 8;
        msg.push_back(0x80);
        while (msg.size() % 64 != 56) msg.push_back(0);
        for (int i = 7; i >= 0; --i) msg.push_back(static_cast<uint8_t>((bitLen >> (i * 8)) & 0xFF));

        for (size_t chunk = 0; chunk < msg.size(); chunk += 64) {
            std::array<uint32_t, 80> w{};
            for (int i = 0; i < 16; ++i) {
                w[i] = (static_cast<uint32_t>(msg[chunk + i * 4]) << 24) |
                       (static_cast<uint32_t>(msg[chunk + i * 4 + 1]) << 16) |
                       (static_cast<uint32_t>(msg[chunk + i * 4 + 2]) << 8) |
                       static_cast<uint32_t>(msg[chunk + i * 4 + 3]);
            }
            for (int i = 16; i < 80; ++i) {
                uint32_t v = w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16];
                w[i] = (v << 1) | (v >> 31);
            }

            uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;
            for (int i = 0; i < 80; ++i) {
                uint32_t f, k;
                if (i < 20) { f = (b & c) | ((~b) & d); k = 0x5A827999; }
                else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1; }
                else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
                else { f = b ^ c ^ d; k = 0xCA62C1D6; }

                uint32_t temp = ((a << 5) | (a >> 27)) + f + e + k + w[i];
                e = d;
                d = c;
                c = (b << 30) | (b >> 2);
                b = a;
                a = temp;
            }
            h0 += a; h1 += b; h2 += c; h3 += d; h4 += e;
        }

        std::array<uint8_t, 20> digest{};
        uint32_t hs[5] = {h0, h1, h2, h3, h4};
        for (int i = 0; i < 5; ++i) {
            digest[i * 4] = static_cast<uint8_t>((hs[i] >> 24) & 0xFF);
            digest[i * 4 + 1] = static_cast<uint8_t>((hs[i] >> 16) & 0xFF);
            digest[i * 4 + 2] = static_cast<uint8_t>((hs[i] >> 8) & 0xFF);
            digest[i * 4 + 3] = static_cast<uint8_t>(hs[i] & 0xFF);
        }
        return digest;
    }

    std::string toLower(std::string s) {
        for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    }

    std::string trim(const std::string& s) {
        size_t start = s.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        size_t end = s.find_last_not_of(" \t\r\n");
        return s.substr(start, end - start + 1);
    }

    std::vector<bytecs> toBytes(const std::string& s) { return std::vector<bytecs>(s.begin(), s.end()); }

    constexpr const char* kWebSocketGuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

    std::array<bytecs, 16> randomBytes16() {
        std::random_device rd;
        std::array<bytecs, 16> bytes{};
        for (auto& b : bytes) b = static_cast<bytecs>(rd() & 0xFF);
        return bytes;
    }

    uint32_t randomMaskingKey() {
        std::random_device rd;
        return rd();
    }

} // namespace

void ClientWebSocket::performHandshake(const System::Uri& uri) {
    const std::string& scheme = uri.getSchemeProperty();
    if (scheme == "wss") {
        throw System::PlatformNotSupportedException(
            "wss:// requires TLS, which this runtime's HttpClient/WebSocket implementations do not support.");
    }
    if (scheme != "ws") {
        throw System::ArgumentException("The URI scheme must be 'ws' or 'wss'.", "uri");
    }

    // Ticket #2089, the door SR-AUD-248 does not name. The finding names SetRequestHeader, and
    // ClientWebSocketOptions now rejects CR/LF/NUL there -- but the request line and the Host
    // field are built from the URI, not from the options bag:
    //
    //     "GET " + uri.getPathAndQueryProperty() + " HTTP/1.1\r\n"
    //     "Host: " + uri.getHostProperty() + ":" + port + "\r\n"
    //
    // System::Uri preserves CR, LF and NUL in both components (measured in
    // build-probe/2089_probe2_uri_door.log; the Uri-side finding is the separate, blocked
    // ticket #2003), so ws://host/a\r\nX-Injected:+yes used to put "GET /a" on the request
    // line and "X-Injected: yes HTTP/1.1" into a header field -- request smuggling, not one
    // extra field. This is the same scope correction #2063 made for System::Net::Http, where
    // the request URI turned out to be a door the review's paraphrase had dropped.
    //
    // The check runs BEFORE the socket is constructed, so a rejected URI opens no connection,
    // sends no bytes and leaks no descriptor. System::Uri is not modified: this is this
    // module's own door, validated with the shared predicate rather than a second local rule.
    if (System::Net::detail::ContainsProtocolFieldTerminator(uri.getHostProperty()) ||
        System::Net::detail::ContainsProtocolFieldTerminator(uri.getPathAndQueryProperty())) {
        // The offending text is deliberately not echoed -- it is attacker-controlled and
        // exception messages get logged.
        throw System::ArgumentException(
            "The WebSocket URI must not contain a carriage return, a line feed or a NUL "
            "character in its host, path or query.",
            "uri");
    }

    intcs port = uri.getPortProperty();
    if (port <= 0) port = 80;

    socket_ = std::make_unique<System::Net::Sockets::Socket>(
        System::Net::Sockets::AddressFamily::InterNetwork, System::Net::Sockets::SocketType::Stream,
        System::Net::Sockets::ProtocolType::Tcp);
    socket_->Connect(uri.getHostProperty(), port);

    auto keyBytes = randomBytes16();
    std::string secWebSocketKey = System::Convert::ToBase64String(std::vector<bytecs>(keyBytes.begin(), keyBytes.end()));

    std::string request = "GET " + uri.getPathAndQueryProperty() + " HTTP/1.1\r\n";
    request += "Host: " + uri.getHostProperty() + ":" + std::to_string(port) + "\r\n";
    request += "Upgrade: websocket\r\n";
    request += "Connection: Upgrade\r\n";
    request += "Sec-WebSocket-Key: " + secWebSocketKey + "\r\n";
    request += "Sec-WebSocket-Version: 13\r\n";
    for (const auto& [name, value] : options_.getRequestHeadersProperty()) {
        request += name + ": " + value + "\r\n";
    }
    const auto& subProtocols = options_.getRequestedSubProtocolsProperty();
    if (!subProtocols.empty()) {
        std::string joined;
        for (size_t i = 0; i < subProtocols.size(); ++i) {
            if (i > 0) joined += ", ";
            joined += subProtocols[i];
        }
        request += "Sec-WebSocket-Protocol: " + joined + "\r\n";
    }
    request += "\r\n";

    auto requestBytes = toBytes(request);
    socket_->Send(requestBytes);

    // Read the HTTP response headers, one byte at a time, until "\r\n\r\n".
    std::string response;
    std::vector<bytecs> one(1);
    while (response.size() < 4 || response.compare(response.size() - 4, 4, "\r\n\r\n") != 0) {
        intcs n = socket_->Receive(one);
        if (n == 0) {
            throw WebSocketException(WebSocketError::ConnectionClosedPrematurely,
                                      "The connection was closed before the WebSocket handshake completed.");
        }
        response += static_cast<char>(one[0]);
        if (response.size() > 16384) {
            throw WebSocketException(WebSocketError::HeaderError, "The WebSocket handshake response is too large.");
        }
    }

    size_t lineEnd = response.find("\r\n");
    std::string statusLine = response.substr(0, lineEnd);
    if (statusLine.find(" 101 ") == std::string::npos) {
        throw WebSocketException(WebSocketError::NotAWebSocket,
                                  "The server did not respond with a valid WebSocket handshake: " + statusLine);
    }

    std::map<std::string, std::string> headers;
    size_t pos = lineEnd + 2;
    while (pos < response.size()) {
        size_t next = response.find("\r\n", pos);
        if (next == std::string::npos || next == pos) break;
        std::string line = response.substr(pos, next - pos);
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            headers[toLower(trim(line.substr(0, colon)))] = trim(line.substr(colon + 1));
        }
        pos = next + 2;
    }

    auto upgradeIt = headers.find("upgrade");
    auto acceptIt = headers.find("sec-websocket-accept");
    if (upgradeIt == headers.end() || toLower(upgradeIt->second) != "websocket" || acceptIt == headers.end()) {
        throw WebSocketException(WebSocketError::NotAWebSocket, "The WebSocket handshake response is missing required headers.");
    }

    auto expectedDigest = sha1(secWebSocketKey + kWebSocketGuid);
    std::string expectedAccept =
        System::Convert::ToBase64String(std::vector<bytecs>(expectedDigest.begin(), expectedDigest.end()));
    if (acceptIt->second != expectedAccept) {
        throw WebSocketException(WebSocketError::HeaderError, "Sec-WebSocket-Accept did not match the expected value.");
    }

    // Ticket #2091 / plan §7.10. This used to store whatever the server named, so the server
    // chose the client's SubProtocol freely -- even when the client had requested none at all.
    // RFC 6455 §4.1 forbids that: the server "MUST NOT" return a subprotocol the client did not
    // request, and returning one when none was requested is an invalid response.
    //
    // Rejecting rather than ignoring is THIS PORT'S CHOICE (plan §16): the reference tree is
    // absent, so whether .NET rejects or silently ignores cannot be verified here. Rejecting is
    // chosen because the alternative leaves getSubProtocolProperty() reporting a protocol the
    // application never agreed to speak.
    //
    // The comparison is OrdinalIgnoreCase, matching the duplicate check AddSubProtocol already
    // performs on the same values, so the two ends of the same list agree about identity.
    auto protoIt = headers.find("sec-websocket-protocol");
    if (protoIt != headers.end()) {
        const auto& requested = options_.getRequestedSubProtocolsProperty();
        bool wasRequested = false;
        for (const auto& candidate : requested) {
            if (System::String::Equals(candidate, protoIt->second,
                                       System::StringComparison::OrdinalIgnoreCase)) {
                wasRequested = true;
                break;
            }
        }
        if (!wasRequested) {
            // The server-supplied value is deliberately not echoed into the message: it is
            // remote text and exception messages get logged.
            throw WebSocketException(
                WebSocketError::UnsupportedProtocol,
                requested.empty()
                    ? "The server returned a WebSocket subprotocol although none was requested."
                    : "The server returned a WebSocket subprotocol that was not requested.");
        }
        subProtocol_ = protoIt->second;
    }

    state_ = WebSocketState::Open;
}

// ---------------------------------------------------------------------------------------------
// #2088 / SR-AUD-247 (CCF-019) — the liveness boundary for the five *Async members.
//
// Every one of them returns a task whose body captures raw `this` and runs on a std::async
// thread, with nothing keeping this object alive until it ran. .NET needs no boundary because
// the GC does that job; C++ has no such mechanism, so the destructor waits -- the same shape
// #2134 gave Socket and #2347 gave FileSystemWatcher.
//
// The decrement is constructed INSIDE each body rather than captured by it, because std::async
// keeps the callable alive until the last future to its shared state is destroyed: a guard
// captured by the lambda is released when the CALLER drops the task, so a boundary built on it
// waits for the caller and deadlocks. #2134 hit exactly that.
struct ClientWebSocket::AsyncOperations {
    std::mutex              mutex;
    std::condition_variable idle;
    int                     inFlight = 0;
};

struct ClientWebSocket::AsyncOperationScope {
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

std::shared_ptr<ClientWebSocket::AsyncOperations> ClientWebSocket::beginAsyncOperation() {
    if (!asyncOps_) asyncOps_ = std::make_shared<AsyncOperations>();
    auto ops = asyncOps_;
    {
        std::lock_guard<std::mutex> lock(ops->mutex);
        ++ops->inFlight;
    }
    return ops;
}

void ClientWebSocket::waitForAsyncOperations() noexcept {
    if (!asyncOps_) return;
    {
        std::unique_lock<std::mutex> lock(asyncOps_->mutex);
        if (asyncOps_->inFlight == 0) return;
    }
    // A pending ReceiveAsync is blocked in recv() waiting for a frame the peer may never send,
    // so the boundary has to be able to CROSS -- otherwise it turns a use-after-free into a
    // hang, which is not a repair. shutdown() unblocks recv() reliably (this is the case where
    // it does; #2134 records the one where it does not, a listening socket's accept()).
    // Deliberately not Close(): the descriptor stays this object's until Dispose() retires it,
    // so a worker returning from recv() is not left holding a number the process has reused.
    if (socket_) {
        try {
            socket_->Shutdown(System::Net::Sockets::SocketShutdown::Both);
        } catch (...) {
            // Already closed or never connected: there is nothing to wake, and the wait below
            // is then the whole boundary.
        }
    }
    std::unique_lock<std::mutex> lock(asyncOps_->mutex);
    asyncOps_->idle.wait(lock, [this] { return asyncOps_->inFlight == 0; });
}

ClientWebSocket::~ClientWebSocket() {
    waitForAsyncOperations();
    Dispose();
}

System::Threading::Tasks::Task
ClientWebSocket::ConnectAsync(const System::Uri& uri, System::Threading::CancellationToken /*cancellationToken*/) {
    if (connectStarted_) {
        throw System::InvalidOperationException("ConnectAsync may only be called once.");
    }
    connectStarted_ = true;
    state_ = WebSocketState::Connecting;
    options_.setToReadOnly();

    return System::Threading::Tasks::Task([this, uri]() {
        try {
            performHandshake(uri);
        } catch (...) {
            Dispose();
            throw;
        }
    });
}

void ClientWebSocket::sendFrame(bytecs opcode, const bytecs* data, size_t len, bool fin) {
    std::lock_guard<std::mutex> lock(sendMutex_);

    std::vector<bytecs> frame;
    frame.push_back(static_cast<bytecs>((fin ? 0x80 : 0x00) | (opcode & 0x0F)));

    uint32_t maskingKey = randomMaskingKey();
    bytecs maskBytes[4] = {
        static_cast<bytecs>((maskingKey >> 24) & 0xFF),
        static_cast<bytecs>((maskingKey >> 16) & 0xFF),
        static_cast<bytecs>((maskingKey >> 8) & 0xFF),
        static_cast<bytecs>(maskingKey & 0xFF),
    };

    if (len <= 125) {
        frame.push_back(static_cast<bytecs>(0x80 | len));
    } else if (len <= 0xFFFF) {
        frame.push_back(static_cast<bytecs>(0x80 | 126));
        frame.push_back(static_cast<bytecs>((len >> 8) & 0xFF));
        frame.push_back(static_cast<bytecs>(len & 0xFF));
    } else {
        frame.push_back(static_cast<bytecs>(0x80 | 127));
        for (int i = 7; i >= 0; --i) frame.push_back(static_cast<bytecs>((static_cast<uint64_t>(len) >> (i * 8)) & 0xFF));
    }

    frame.insert(frame.end(), maskBytes, maskBytes + 4);

    size_t frameHeaderLen = frame.size();
    frame.resize(frameHeaderLen + len);
    for (size_t i = 0; i < len; ++i) {
        frame[frameHeaderLen + i] = static_cast<bytecs>(data[i] ^ maskBytes[i % 4]);
    }

    socket_->Send(frame);
}

namespace {

    void readExact(System::Net::Sockets::Socket& socket, std::vector<bytecs>& buffer, size_t n) {
        buffer.resize(n);
        size_t total = 0;
        while (total < n) {
            std::vector<bytecs> chunk(n - total);
            intcs received = socket.Receive(chunk, 0, static_cast<intcs>(n - total), System::Net::Sockets::SocketFlags::None);
            if (received == 0) {
                throw WebSocketException(WebSocketError::ConnectionClosedPrematurely,
                                          "The remote endpoint closed the connection.");
            }
            std::memcpy(buffer.data() + total, chunk.data(), static_cast<size_t>(received));
            total += static_cast<size_t>(received);
        }
    }

    // Verified against WebSocketValidate.cs's ValidateBuffer: negative offset/count and an
    // offset/count exceeding the buffer are all rejected. Previously SendAsync/ReceiveAsync
    // did buffer.data() + offset / std::memcpy(..., count) with no bounds check at all -- an
    // out-of-bounds read (Send) or write (Receive) whenever offset+count exceeded the buffer.
    template<typename Buf>
    void validateWebSocketBuffer(const Buf& buffer, intcs offset, intcs count) {
        if (offset < 0 || static_cast<size_t>(offset) > buffer.size())
            throw System::ArgumentOutOfRangeException("offset");
        if (count < 0 || static_cast<size_t>(count) > buffer.size() - static_cast<size_t>(offset))
            throw System::ArgumentOutOfRangeException("count");
    }

} // namespace

// Ticket #2090 -- frame-header validation. Until this landed, readFrame() decoded the header
// and validated NOTHING in it, so a server the client had connected to could drive five
// distinct protocol violations straight through the parser (docs/SystemNetWebSocketsNamespace
// ReviewPlan.md §7.1-§7.5). Each check below rejects before any dependent bytes are read, so a
// malformed frame never causes an allocation or a read sized by its own bad header.
//
// Every rule here is RFC 6455, cited as a PROTOCOL fact rather than as .NET behaviour: the
// reference tree is absent, and these five are decidable from the wire format alone. The
// exception identity -- WebSocketException with the closest WebSocketError -- is THIS PORT'S
// CHOICE, recorded as such.
namespace {

    constexpr bool isControlOpcode(bytecs opcode) { return (opcode & 0x08) != 0; }

    /// RFC 6455 §5.2: 0x0 continuation, 0x1 text, 0x2 binary, 0x8 close, 0x9 ping, 0xA pong.
    /// Everything else (0x3-0x7, 0xB-0xF) is reserved and MUST fail the connection. Before
    /// this, ReceiveAsync's switch routed every reserved opcode to `default:` and delivered it
    /// to the caller as ordinary message data.
    constexpr bool isDefinedOpcode(bytecs opcode) {
        return opcode == 0x0 || opcode == 0x1 || opcode == 0x2 ||
               opcode == 0x8 || opcode == 0x9 || opcode == 0xA;
    }

    // --- Ticket #2091 ------------------------------------------------------------------------
    // Where #2090 validated the frame HEADER, these validate the frame's PAYLOAD. Both are
    // driven entirely by the server the client connected to.

    /// Strict UTF-8 validity, rejecting overlong encodings, surrogate encodings and scalars
    /// above U+10FFFF, as RFC 6455 §8.1 requires for a Text payload and a close reason.
    ///
    /// Transcribed from this repository's own already-correct implementation --
    /// `System::Net::Security::SslApplicationProtocol::isValidUtf8` -- rather than invented,
    /// because that one was written against .NET's ExceptionFallback decoder and the reference
    /// tree is absent. It is copied rather than shared because that one is a *private member*
    /// of an unrelated class in a different (INTERFACE) component; extracting it would be a
    /// refactor of a third module, and this repository already holds several independent UTF-8
    /// decoders (`UTF8Encoding`, `Utf8JsonWriter`, `BinaryReader`, `IdnMapping`). Consolidating
    /// them is a real but separate concern, recorded in the plan rather than done here.
    [[nodiscard]] bool isValidUtf8(const std::vector<bytecs>& bytes, size_t from) {
        size_t i = from, n = bytes.size();
        while (i < n) {
            unsigned c0 = static_cast<unsigned char>(bytes[i]);
            size_t extra;
            unsigned minCp;
            if (c0 < 0x80) { ++i; continue; }
            else if ((c0 & 0xE0) == 0xC0) { extra = 1; minCp = 0x80; }
            else if ((c0 & 0xF0) == 0xE0) { extra = 2; minCp = 0x800; }
            else if ((c0 & 0xF8) == 0xF0) { extra = 3; minCp = 0x10000; }
            else return false;
            if (i + extra >= n) return false;
            unsigned cp = c0 & (0xFFu >> (extra + 2));
            for (size_t k = 1; k <= extra; ++k) {
                unsigned cb = static_cast<unsigned char>(bytes[i + k]);
                if ((cb & 0xC0) != 0x80) return false;
                cp = (cp << 6) | (cb & 0x3F);
            }
            if (cp < minCp || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return false;
            i += extra + 1;
        }
        return true;
    }

    /// RFC 6455 §7.4: which close codes may legitimately appear in a Close frame on the wire.
    ///
    /// * 1000-1003 and 1007-1011 are defined by §7.4.1 and may be sent.
    /// * 1004 is reserved, and 1005 / 1006 are defined for *local* reporting only -- §7.4.1
    ///   says each of the three MUST NOT be set in a Close frame. 1005 is this port's
    ///   `WebSocketCloseStatus::Empty`, which is exactly why it must be rejected on the wire:
    ///   an enumerator existing locally does not make the value legal in a frame.
    /// * 1012-1014 are registered with IANA under the procedure §7.4.2 delegates ("reserved for
    ///   definition by this protocol, its future revisions, and extensions specified in a
    ///   permanent and readily available public specification"), so they are ACCEPTED. This is
    ///   THIS PORT'S CHOICE, taken deliberately: rejecting them would reject a conforming
    ///   server, and the reference tree is absent so .NET's own set cannot be consulted.
    /// * 1015 is registered but §7.4.1 likewise says it MUST NOT appear in a frame.
    /// * 1016-2999 are reserved and undefined; 3000-4999 are delegated by §7.4.2 to IANA
    ///   registration and private use respectively, and are ACCEPTED.
    ///
    /// Codes in 3000-4999 (and 1012-1014) are legal but have no enumerator in
    /// `WebSocketCloseStatus` -- in this port or in .NET, whose enum names the same subset. That
    /// is inherent to the enum's design and is NOT what this ticket repairs; what it repairs is
    /// codes that can never be valid reaching the public getter.
    [[nodiscard]] constexpr bool isValidReceivedCloseCode(uint16_t code) {
        return (code >= 1000 && code <= 1003)
            || (code >= 1007 && code <= 1014)
            || (code >= 3000 && code <= 4999);
    }

} // namespace

// Ticket #2091. Before this, the Close payload was decoded IN TWO PLACES -- ReceiveAsync's
// `case 0x8` and CloseAsync's close-handshake loop -- by two copies of the same
// `if (payload.size() >= 2)` block, and neither validated anything. Routing both through one
// function is half the repair: two structurally identical parsers of remote input are exactly
// how one of them ends up fixed and the other does not.
namespace {

    void parseClosePayload(const std::vector<bytecs>& payload,
                           std::optional<WebSocketCloseStatus>& status,
                           std::optional<std::string>& description) {
        status.reset();
        description.reset();

        // RFC 6455 §5.5.1: a Close frame carries either no payload, or a 2-byte status code
        // optionally followed by a reason. A 1-byte payload is a protocol error. It used to be
        // silently ignored by the `>= 2` guard, which reported a CLEAN close with no status.
        if (payload.empty()) {
            return;
        }
        if (payload.size() == 1) {
            throw WebSocketException(WebSocketError::Faulted,
                                      "A WebSocket close payload must be empty or at least two bytes.");
        }

        const uint16_t code = static_cast<uint16_t>((static_cast<uint16_t>(payload[0]) << 8) | payload[1]);
        if (!isValidReceivedCloseCode(code)) {
            throw WebSocketException(WebSocketError::Faulted,
                                      "The WebSocket close frame carries a status code that RFC 6455 "
                                      "does not permit on the wire.");
        }
        status = static_cast<WebSocketCloseStatus>(code);

        if (payload.size() > 2) {
            // RFC 6455 §5.5.1: the reason is UTF-8 text. The whole reason is validated before any
            // of it is published, so a caller never observes a partially decoded reason.
            if (!isValidUtf8(payload, 2)) {
                throw WebSocketException(WebSocketError::Faulted,
                                          "The WebSocket close reason is not valid UTF-8.");
            }
            description = std::string(payload.begin() + 2, payload.end());
        }
    }

} // namespace

ClientWebSocket::RawFrame ClientWebSocket::readFrame() {
    std::vector<bytecs> header;
    readExact(*socket_, header, 2);

    bool fin = (header[0] & 0x80) != 0;
    bytecs opcode = header[0] & 0x0F;
    bool masked = (header[1] & 0x80) != 0;
    uint64_t len = header[1] & 0x7F;

    // RFC 6455 §5.2: RSV1/RSV2/RSV3 must be zero unless an extension that defines them was
    // negotiated. This client negotiates none (permessage-deflate is out of scope), so any set
    // reserved bit is a protocol error rather than something to ignore.
    if ((header[0] & 0x70) != 0) {
        throw WebSocketException(WebSocketError::HeaderError,
                                  "The WebSocket frame sets a reserved bit, but no extension was negotiated.");
    }

    if (!isDefinedOpcode(opcode)) {
        throw WebSocketException(WebSocketError::HeaderError,
                                  "The WebSocket frame uses a reserved opcode.");
    }

    // RFC 6455 §5.1: a server MUST NOT mask the frames it sends, and a client that receives a
    // masked frame MUST fail the connection. This port previously honoured the mask bit and
    // unmasked the payload, accepting exactly what it is required to reject.
    if (masked) {
        throw WebSocketException(WebSocketError::HeaderError,
                                  "The server sent a masked frame, which RFC 6455 forbids.");
    }

    // RFC 6455 §5.5: control frames MUST NOT be fragmented and MUST carry at most 125 bytes.
    // The length check runs on the 7-bit field, BEFORE the extended-length bytes are read, so a
    // 256 MiB "Ping" is rejected without ever being allocated -- it used to be read into memory
    // and then echoed straight back to the sender as a Pong.
    if (isControlOpcode(opcode)) {
        if (!fin) {
            throw WebSocketException(WebSocketError::HeaderError,
                                      "A WebSocket control frame must not be fragmented.");
        }
        if (len > 125) {
            throw WebSocketException(WebSocketError::HeaderError,
                                      "A WebSocket control frame payload must not exceed 125 bytes.");
        }
    }

    if (len == 126) {
        std::vector<bytecs> ext;
        readExact(*socket_, ext, 2);
        len = (static_cast<uint64_t>(ext[0]) << 8) | ext[1];
    } else if (len == 127) {
        std::vector<bytecs> ext;
        readExact(*socket_, ext, 8);
        len = 0;
        for (int i = 0; i < 8; ++i) len = (len << 8) | ext[static_cast<size_t>(i)];
    }

    // The 64-bit extended length comes directly off the wire with no upper bound otherwise --
    // a malicious or misbehaving server could send an arbitrarily large value (e.g. close to
    // UINT64_MAX) and readExact()'s buffer.resize(n) would attempt a correspondingly huge
    // allocation, throwing a raw std::length_error/std::bad_alloc (invisible to code catching
    // System::Exception&) instead of a clean, catchable WebSocketException. 256 MiB is a
    // generous cap for a single WebSocket frame -- comfortably above any realistic legitimate
    // message -- chosen defensively, matching the existing 16384-byte cap already applied to
    // the handshake response a few lines up in this same file.
    constexpr uint64_t kMaxFramePayloadBytes = 256ull * 1024 * 1024;
    if (len > kMaxFramePayloadBytes) {
        throw WebSocketException(WebSocketError::HeaderError,
                                  "The WebSocket frame payload length exceeds the maximum allowed size.");
    }

    // No masking key is read and no unmasking is performed: a masked server frame was rejected
    // above, so by construction every frame reaching this point is unmasked.
    RawFrame result;
    result.fin = fin;
    result.opcode = opcode;
    readExact(*socket_, result.payload, static_cast<size_t>(len));
    return result;
}

void ClientWebSocket::sendCloseFrame(WebSocketCloseStatus closeStatus, const std::optional<std::string>& statusDescription) {
    std::vector<bytecs> payload;
    auto code = static_cast<uint16_t>(closeStatus);
    payload.push_back(static_cast<bytecs>((code >> 8) & 0xFF));
    payload.push_back(static_cast<bytecs>(code & 0xFF));
    if (statusDescription.has_value()) {
        auto bytes = toBytes(*statusDescription);
        payload.insert(payload.end(), bytes.begin(), bytes.end());
    }
    sendFrame(0x8, payload.data(), payload.size(), true);
}

System::Threading::Tasks::Task
ClientWebSocket::SendAsync(const std::vector<bytecs>& buffer, intcs offset, intcs count, WebSocketMessageType messageType,
                            bool endOfMessage, System::Threading::CancellationToken /*cancellationToken*/) {
    validateWebSocketBuffer(buffer, offset, count);
    // #2088's second half, which the finding recorded as wider than its headline: this used to
    // capture `&buffer`, so the CALLER's vector was a second borrowed object the task could
    // outlive. The destructor boundary below cannot help there -- the buffer is not this object.
    // Send takes it by const reference and only reads it, so copying the bytes it will actually
    // send is a complete fix at the cost of one copy.
    std::vector<bytecs> payload(buffer.begin() + offset, buffer.begin() + offset + count);
    auto ops = beginAsyncOperation();
    return System::Threading::Tasks::Task([this, payload = std::move(payload), count, messageType,
                                            endOfMessage, ops]() {
        AsyncOperationScope release{ops};
        WebSocket::ThrowOnInvalidState(state_, {WebSocketState::Open, WebSocketState::CloseReceived});
        bytecs opcode;
        if (sendContinuation_) {
            opcode = 0x0;
        } else {
            opcode = messageType == WebSocketMessageType::Text ? 0x1 : 0x2;
        }
        sendFrame(opcode, payload.data(), static_cast<size_t>(count), endOfMessage);
        sendContinuation_ = !endOfMessage;
    });
}

System::Threading::Tasks::TaskT<WebSocketReceiveResult>
ClientWebSocket::ReceiveAsync(std::vector<bytecs>& buffer, intcs offset, intcs count,
                               System::Threading::CancellationToken /*cancellationToken*/) {
    validateWebSocketBuffer(buffer, offset, count);
    // Receive keeps the reference, and that is not an oversight: `buffer` is the OUT-parameter
    // this operation writes its result into, so a caller that destroys it before awaiting has
    // discarded the result it asked for. The destructor boundary covers `this`; the buffer's
    // lifetime is the caller's, exactly as it is for the synchronous overload.
    auto ops = beginAsyncOperation();
    return System::Threading::Tasks::TaskT<WebSocketReceiveResult>([this, &buffer, offset, count, ops]() {
        AsyncOperationScope release{ops};
        WebSocket::ThrowOnInvalidState(state_, {WebSocketState::Open, WebSocketState::CloseSent});

        if (recvLeftoverPos_ < recvLeftover_.size()) {
            size_t remaining = recvLeftover_.size() - recvLeftoverPos_;
            size_t n = std::min(static_cast<size_t>(count), remaining);
            std::memcpy(buffer.data() + offset, recvLeftover_.data() + recvLeftoverPos_, n);
            recvLeftoverPos_ += n;
            bool isLast = recvLeftoverPos_ == recvLeftover_.size();
            if (isLast) {
                recvLeftover_.clear();
                recvLeftoverPos_ = 0;
            }
            return WebSocketReceiveResult(static_cast<intcs>(n), recvLeftoverType_, isLast);
        }

        while (true) {
            RawFrame frame = readFrame();
            switch (frame.opcode) {
                case 0x9: { // Ping
                    sendFrame(0xA, frame.payload.data(), frame.payload.size(), true);
                    continue;
                }
                case 0xA: // Pong
                    continue;
                case 0x8: { // Close
                    std::optional<WebSocketCloseStatus> receivedStatus;
                    std::optional<std::string> receivedDescription;
                    // Ticket #2091. Throws before closeStatus_/state_ are touched, so a
                    // malformed close frame cannot leave the socket reporting a clean close.
                    parseClosePayload(frame.payload, receivedStatus, receivedDescription);
                    closeStatus_ = receivedStatus;
                    closeStatusDescription_ = receivedDescription;
                    state_ = state_ == WebSocketState::CloseSent ? WebSocketState::Closed : WebSocketState::CloseReceived;
                    return WebSocketReceiveResult(0, WebSocketMessageType::Close, true, receivedStatus, receivedDescription);
                }
                default: { // Continuation(0x0)/Text(0x1)/Binary(0x2)
                    WebSocketMessageType type =
                        frame.opcode == 0x0 ? fragmentType_ : (frame.opcode == 0x1 ? WebSocketMessageType::Text : WebSocketMessageType::Binary);
                    if (frame.opcode != 0x0) fragmentType_ = type;

                    // Ticket #2091 / RFC 6455 §8.1: a Text message must be valid UTF-8.
                    //
                    // SCOPE, stated precisely because the limit is real: this validates a Text
                    // message that arrives as ONE COMPLETE FRAME (fin && opcode 0x1). A
                    // FRAGMENTED Text message is NOT validated, because a scalar may legally
                    // straddle a fragment boundary, so per-frame validation would reject
                    // conforming input. Validating it correctly needs either buffering the whole
                    // message or carrying incremental decoder state on the object -- an
                    // OBJECT-LAYOUT CHANGE, which this ticket is compatible precisely because it
                    // does not make (plan §10/§11, pinned by the layout static_assert). The gap
                    // is recorded in the plan rather than closed by a check that would be wrong.
                    //
                    // The whole frame is validated BEFORE any byte is copied to the caller, so a
                    // rejected message is never partially published.
                    if (frame.fin && frame.opcode == 0x1 && !isValidUtf8(frame.payload, 0)) {
                        throw WebSocketException(WebSocketError::Faulted,
                                                  "The WebSocket text message is not valid UTF-8.");
                    }

                    if (frame.payload.size() <= static_cast<size_t>(count)) {
                        std::memcpy(buffer.data() + offset, frame.payload.data(), frame.payload.size());
                        return WebSocketReceiveResult(static_cast<intcs>(frame.payload.size()), type, frame.fin);
                    }

                    std::memcpy(buffer.data() + offset, frame.payload.data(), static_cast<size_t>(count));
                    recvLeftover_.assign(frame.payload.begin() + count, frame.payload.end());
                    recvLeftoverPos_ = 0;
                    recvLeftoverType_ = type;
                    return WebSocketReceiveResult(count, type, false);
                }
            }
        }
    });
}

System::Threading::Tasks::Task
ClientWebSocket::CloseOutputAsync(WebSocketCloseStatus closeStatus, const std::optional<std::string>& statusDescription,
                                    System::Threading::CancellationToken /*cancellationToken*/) {
    return System::Threading::Tasks::Task([this, closeStatus, statusDescription]() {
        WebSocket::ThrowOnInvalidState(state_, {WebSocketState::Open, WebSocketState::CloseReceived});
        sendCloseFrame(closeStatus, statusDescription);
        state_ = state_ == WebSocketState::CloseReceived ? WebSocketState::Closed : WebSocketState::CloseSent;
    });
}

System::Threading::Tasks::Task
ClientWebSocket::CloseAsync(WebSocketCloseStatus closeStatus, const std::optional<std::string>& statusDescription,
                              System::Threading::CancellationToken /*cancellationToken*/) {
    // Unlike ConnectAsync/SendAsync/ReceiveAsync (which all consistently comment out this same
    // parameter), this one used to be captured by name and threaded into the lambda without
    // ever actually being checked anywhere in the close-handshake wait loop below -- silently
    // misleading, since the parameter's presence implies cancellation works here specifically.
    // Wiring up real cancellation would need to interrupt the loop's blocking socket read
    // mid-flight, which only makes sense done consistently across all of this file's async
    // methods at once, not as a one-off partial fix for just this one; left for a follow-up
    // ticket, matching the honest "not implemented" stance the other three already take.
    return System::Threading::Tasks::Task([this, closeStatus, statusDescription]() {
        if (state_ == WebSocketState::Open) {
            sendCloseFrame(closeStatus, statusDescription);
            state_ = WebSocketState::CloseSent;
        }
        if (state_ == WebSocketState::CloseSent) {
            while (state_ != WebSocketState::Closed) {
                RawFrame frame = readFrame();
                if (frame.opcode == 0x8) {
                    // Ticket #2091: the same parser as ReceiveAsync's, not a second copy of it.
                    // This site had an identical unvalidated `>= 2` block, so a malformed close
                    // frame arriving during the close handshake was accepted here even after the
                    // receive path had been repaired.
                    std::optional<WebSocketCloseStatus> receivedStatus;
                    std::optional<std::string> receivedDescription;
                    parseClosePayload(frame.payload, receivedStatus, receivedDescription);
                    closeStatus_ = receivedStatus;
                    closeStatusDescription_ = receivedDescription;
                    state_ = WebSocketState::Closed;
                }
                // Any other frame received while waiting for the close handshake is discarded.
            }
        }
    });
}

void ClientWebSocket::Abort() {
    if (state_ != WebSocketState::Aborted && state_ != WebSocketState::Closed) {
        state_ = WebSocketState::Aborted;
    }
    Dispose();
}

void ClientWebSocket::Dispose() {
    if (socket_) {
        socket_->Close();
        socket_.reset();
    }
    if (state_ != WebSocketState::Aborted) {
        state_ = WebSocketState::Closed;
    }
}

} // namespace System::Net::WebSockets
