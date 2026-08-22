// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Net/WebSockets/ClientWebSocket.hpp"
#include "System/ObjectDisposedException.hpp"
#include <condition_variable>
#include <memory>
#include <mutex>
#include <array>
#include <cstring>
#include <map>
#include "System/ArgumentException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Convert.hpp"
#include "System/Guid.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/Net/IPEndPoint.hpp"
#include "System/Net/detail/ProtocolFieldValidation.hpp"
#include "System/Net/WebSockets/WebSocketException.hpp"
#include "System/PlatformNotSupportedException.hpp"
#include "System/Security/Cryptography/RandomNumberGenerator.hpp"
#include "System/Threading/Tasks/TaskCanceledException.hpp"
#include <chrono>
#include <condition_variable>
#include <thread>

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

    // BOTH OF THESE USED `std::random_device`, AND THAT IS A DEFECT RATHER THAN A STYLE POINT
    // (#2401). The standard explicitly permits a deterministic `std::random_device`, and THIS
    // REPOSITORY HAS ALREADY MEASURED ONE: `Random.cpp:69-70` records "on a platform whose
    // random_device is deterministic (MinGW-w64's historically was)", and MinGW-w64 is a
    // supported compile target of this project. On such a platform every connection would send
    // the SAME Sec-WebSocket-Key and every frame's masking key would be predictable.
    //
    // RFC 6455 does not leave that to taste. Section 5.3: "The masking key needs to be
    // unpredictable; thus, the masking key MUST be derived from a strong source of entropy, and
    // the masking key for a given frame MUST NOT make it simple for a server/proxy to predict the
    // masking key for a subsequent frame." Masking exists to stop cache-poisoning of
    // intermediaries (RFC 6455 section 10.3), so a predictable key defeats the one attack it was
    // introduced for.
    //
    // .NET USES A CSPRNG FOR BOTH, BY TWO DIFFERENT ROUTES, and each is transcribed as it stands
    // rather than harmonised into one.

    /// The Sec-WebSocket-Key nonce. `WebSocketHandle.Managed.cs:490-494` --
    /// `Guid.NewGuid().TryWriteBytes(bytes)`, base64-encoded. Since #2228 this port's
    /// `Guid::NewGuid()` draws from the platform CSPRNG, so this route costs no component edge:
    /// `Core.Base` is already a public dependency. A v4 GUID fixes 6 of its 128 bits (version and
    /// variant), so the nonce carries 122 bits of entropy rather than 128 -- which is .NET's own
    /// arithmetic here, not a shortcut taken by this port.
    std::array<bytecs, 16> randomBytes16() { return System::Guid::NewGuid().ToByteArray(); }

    /// The per-frame masking key. `ManagedWebSocket.cs:762-763` -- `WriteRandomMask` is
    /// `RandomNumberGenerator.Fill(buffer.AsSpan(offset, MaskLength))`. That is what the
    /// `Security.Cryptography.Random` PRIVATE dependency buys; calling `getentropy()` directly from
    /// here instead would be a third copy of the platform entropy call, which is the duplication
    /// #2354 spent a ticket removing.
    uint32_t randomMaskingKey() {
        std::vector<bytecs> key(4);
        System::Security::Cryptography::RandomNumberGenerator::Fill(key);
        return static_cast<uint32_t>(key[0]) | (static_cast<uint32_t>(key[1]) << 8) |
               (static_cast<uint32_t>(key[2]) << 16) | (static_cast<uint32_t>(key[3]) << 24);
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
    // System::Uri preserves CR, LF and NUL in path/query under its resolved #2003
    // no-percent-encoding contract (measured in build-probe/2089_probe2_uri_door.log), so
    // ws://host/a\r\nX-Injected:+yes used to put "GET /a" on the request line and
    // "X-Injected: yes HTTP/1.1" into a header field -- request smuggling, not one extra field.
    // #2359 subsequently made Uri reject those characters in a host; retaining the host check
    // here keeps this protocol boundary self-contained. This is the same scope correction #2063
    // made for System::Net::Http, where the URI was a door the review paraphrase had dropped.
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

    // #2096: built into a local, published under stateMutex_, then used through the local. A
    // concurrent Dispose() can take the member away at any point after the publish; the local
    // keeps this handshake's socket alive until the handshake returns.
    auto socket = std::make_shared<System::Net::Sockets::Socket>(
        System::Net::Sockets::AddressFamily::InterNetwork, System::Net::Sockets::SocketType::Stream,
        System::Net::Sockets::ProtocolType::Tcp);
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        socket_ = socket;
    }
    socket->Connect(uri.getHostProperty(), port);

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
    socket->Send(requestBytes);

    // Read the HTTP response headers, one byte at a time, until "\r\n\r\n".
    std::string response;
    std::vector<bytecs> one(1);
    while (response.size() < 4 || response.compare(response.size() - 4, 4, "\r\n\r\n") != 0) {
        intcs n = socket->Receive(one);
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
    // Rejecting rather than ignoring was initially recorded as this port's conservative choice.
    // The reference is now available and confirms it: WebSocketHandle.Managed rejects a returned
    // protocol unless it matches one of the requested values with OrdinalIgnoreCase.
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
        std::lock_guard<std::mutex> lock(stateMutex_);
        subProtocol_ = protoIt->second;
    }

    storeState(WebSocketState::Open);
    startKeepAlive();   // #2094: only once the connection is actually usable
}

// ---------------------------------------------------------------------------------------------
// #2096 — the state and socket accessors every other member goes through.
// ---------------------------------------------------------------------------------------------

WebSocketState ClientWebSocket::loadState() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return state_;
}

void ClientWebSocket::storeState(WebSocketState next) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    state_ = next;
}

// #2357: .NET's OUTER gate, `ClientWebSocket.ConnectedWebSocket` (`ClientWebSocket.cs:163-177`).
//
// THE TICKET'S FRAMING WAS TOO SIMPLE, AND THE REFERENCE CORRECTS IT. It reported that every door
// here raises WebSocketException(InvalidState) where .NET "never" does. .NET actually has TWO
// layers, and only the outer one avoids WebSocketException:
//
//   * OUTER -- `ConnectedWebSocket`: ObjectDisposedException if the instance is disposed,
//     InvalidOperationException("The WebSocket is not connected.") if it was never connected or
//     is still connecting. This layer did not exist here at all.
//   * INNER -- `WebSocketStateHelper.ThrowIfInvalidState` (`WebSocketStateHelper.cs:21-41`), run
//     per operation by ManagedWebSocket: WebSocketException(InvalidState) when the CURRENT state
//     forbids the operation. That is exactly what this port's WebSocket::ThrowOnInvalidState
//     already did, and it is KEPT. Rewriting it would have replaced a correct exception with a
//     wrong one.
//
// NO NEW DATA MEMBER IS NEEDED, so sizeof(ClientWebSocket) is unchanged and this is not an SA-3
// change. .NET's InternalState maps exactly onto state this class already holds, because
// `Abort()` calls `Dispose()` (`ClientWebSocket.cs:179-193`) so Aborted IS Disposed there, and
// because `socket_` is assigned only on a successful connect and cleared only by Dispose():
//
//     Created    !connectStarted_
//     Connecting  connectStarted_ && !socket_ && state_ == Connecting
//     Disposed    connectStarted_ && !socket_ && state_ != Connecting
//     Connected   socket_ != nullptr        (CloseAsync does NOT clear it, matching .NET, where
//                                            a closed socket is still InternalState.Connected and
//                                            the INNER layer reports the state)
void ClientWebSocket::throwIfNotConnected() const {
    bool disposed = false;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        if (socket_) return;                       // Connected -- the inner check decides
        disposed = connectStarted_ && state_ != WebSocketState::Connecting;
    }
    if (disposed) {
        throw System::ObjectDisposedException("System.Net.WebSockets.ClientWebSocket");
    }
    throw System::InvalidOperationException("The WebSocket is not connected.");
}

std::shared_ptr<System::Net::Sockets::Socket> ClientWebSocket::socketForIo() const {
    std::shared_ptr<System::Net::Sockets::Socket> socket;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        socket = socket_;
    }
    if (!socket) {
        // #2357: this is reached when Dispose()/Abort() took the socket away underneath an
        // operation that had already passed the outer gate -- so it is the DISPOSED case, and
        // throwIfNotConnected() names it as such. #2096 deliberately raised
        // WebSocketException(InvalidState) here to match the non-racy path; now that the non-racy
        // path raises ObjectDisposedException, matching it means raising that. The two sides of
        // the race still agree, which is what #2096 required.
        throwIfNotConnected();
        throw System::ObjectDisposedException("System.Net.WebSockets.ClientWebSocket");
    }
    return socket;
}

std::optional<WebSocketCloseStatus> ClientWebSocket::getCloseStatusProperty() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return closeStatus_;
}

std::optional<std::string> ClientWebSocket::getCloseStatusDescriptionProperty() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return closeStatusDescription_;
}

std::optional<std::string> ClientWebSocket::getSubProtocolProperty() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return subProtocol_;
}

WebSocketState ClientWebSocket::getStateProperty() const { return loadState(); }

// ---------------------------------------------------------------------------------------------
// #2093 — cancellation. See CancellationScope's doc-comment for why this is nine lines rather
// than the transport-level redesign the ticket expected.
// ---------------------------------------------------------------------------------------------

ClientWebSocket::CancellationScope::CancellationScope(ClientWebSocket* owner,
                                                      System::Threading::CancellationToken token)
    : token_(token) {
    // A token already cancelled means the operation never starts. .NET reaches the same place by
    // awaiting its receive/send mutex with the token, which throws before any I/O.
    token_.ThrowIfCancellationRequested();
    registration_ = token_.Register([owner] { owner->Abort(); });
}

ClientWebSocket::CancellationScope::~CancellationScope() { registration_.Dispose(); }

bool ClientWebSocket::CancellationScope::cancelled() const {
    return token_.getIsCancellationRequestedProperty();
}

// ---------------------------------------------------------------------------------------------
// #2094 — the keep-alive heartbeat, transcribed from ManagedWebSocket.KeepAlive.cs.
//
// .NET has TWO strategies and picks between them by whether KeepAliveTimeout is positive
// (ManagedWebSocket.cs:169-198):
//
//   * interval <= 0                  -> no heartbeat at all;
//   * interval > 0, timeout <= 0     -> UNSOLICITED PONG: send an empty Pong every interval and
//                                       expect nothing back. This is the DEFAULT, because .NET's
//                                       own default timeout is Timeout.InfiniteTimeSpan
//                                       (WebSocketDefaults.cs:15-17) and this port already
//                                       matched both defaults;
//   * interval > 0, timeout > 0      -> PING/PONG: send a Ping carrying an 8-byte big-endian
//                                       counter and require a matching Pong within `timeout`,
//                                       else fault the connection.
//
// The heartbeat ticks at max(min(delay, timeout) / 4, 1) ms in Ping/Pong mode and at `interval`
// in unsolicited mode -- KeepAlivePingState.HeartBeatIntervalMs, ManagedWebSocket.KeepAlive.cs:135.
//
// ONE LIMITATION, STATED RATHER THAN DISCOVERED LATER. A Pong is only observed while a
// ReceiveAsync is running, because this port has no independent receive pump -- and neither does
// .NET's ManagedWebSocket, which also processes pongs inside ReceiveAsyncPrivate. So a caller who
// enables Ping/Pong and then never receives will time out. That is why the DEFAULT is unsolicited
// Pong, where nothing is ever expected back and the strategy cannot fault.
struct ClientWebSocket::KeepAlive {
    std::mutex              mutex;
    std::condition_variable wake;
    bool                    stop = false;

    long long delayMs   = 0;   // KeepAliveInterval
    long long timeoutMs = 0;   // KeepAliveTimeout; <= 0 means the unsolicited-Pong strategy
    long long tickMs    = 0;   // HeartBeatIntervalMs

    // Ping/Pong bookkeeping. Guarded by `mutex`.
    bool      pingSent            = false;
    long long pingPayload         = 0;
    long long pingTimeoutTick     = 0;
    long long nextPingRequestTick = 0;
    bool      faulted             = false;

    std::thread thread;

    [[nodiscard]] bool usesPingPong() const { return timeoutMs > 0; }

    static long long nowMs() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
            .count();
    }

    /// .NET's OnDataReceived: ANY received frame pushes the next ping out.
    void onDataReceived() {
        std::lock_guard<std::mutex> lock(mutex);
        nextPingRequestTick = nowMs() + delayMs;
    }

    /// .NET's OnPongResponseReceived: only a Pong whose payload matches the outstanding Ping
    /// clears it. An unsolicited Pong from the peer is ignored rather than treated as an answer.
    void onPongReceived(const std::vector<SharpRuntime::bytecs>& payload) {
        std::lock_guard<std::mutex> lock(mutex);
        if (!pingSent || payload.size() != 8) return;
        long long value = 0;
        for (std::size_t i = 0; i < 8; ++i) {
            value = (value << 8) | static_cast<unsigned char>(payload[i]);
        }
        if (value != pingPayload) return;
        pingSent = false;
        pingTimeoutTick = 0;
    }
};

void ClientWebSocket::CancellationScope::rethrowAsCancelled() const {
    // The body failed and the token has fired, so the abort is the cause and the caller asked
    // for it. Reporting the WebSocketException the abort produced would be technically true and
    // useless: a caller cancelling its own operation wants OperationCanceledException, which is
    // what every .NET awaiter of a cancelled WebSocket call observes.
    throw System::Threading::Tasks::TaskCanceledException(
        "The WebSocket operation was canceled.");
}

void ClientWebSocket::startKeepAlive() {
    const auto interval = options_.getKeepAliveIntervalProperty();
    const auto timeout  = options_.getKeepAliveTimeoutProperty();

    // Timeout::InfiniteTimeSpan and zero both mean "no heartbeat" for the interval, and
    // "no deadline" for the timeout. Matching ManagedWebSocket.cs:169-173's `> TimeSpan.Zero`.
    const long long intervalMs =
        interval.getTicksProperty() == System::Threading::Timeout::InfiniteTimeSpan
            ? 0
            : static_cast<long long>(interval.getTotalMillisecondsProperty());
    const long long timeoutMs =
        timeout.getTicksProperty() == System::Threading::Timeout::InfiniteTimeSpan
            ? 0
            : static_cast<long long>(timeout.getTotalMillisecondsProperty());
    if (intervalMs <= 0) return;

    auto state       = std::make_shared<KeepAlive>();
    state->delayMs   = intervalMs;
    state->timeoutMs = timeoutMs;
    // HeartBeatIntervalMs, ManagedWebSocket.KeepAlive.cs:135. In unsolicited-Pong mode there is
    // nothing to poll for, so the tick IS the interval.
    state->tickMs = timeoutMs > 0 ? std::max((std::min)(intervalMs, timeoutMs) / 4, 1LL) : intervalMs;
    state->nextPingRequestTick = KeepAlive::nowMs() + intervalMs;

    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        keepAlive_ = state;
    }
    state->thread = std::thread([this, state] {
        std::unique_lock<std::mutex> lock(state->mutex);
        while (!state->stop) {
            state->wake.wait_for(lock, std::chrono::milliseconds(state->tickMs),
                                 [&state] { return state->stop; });
            if (state->stop) break;
            lock.unlock();
            keepAliveHeartBeat(state);
            lock.lock();
        }
    });
}

void ClientWebSocket::stopKeepAlive() noexcept {
    std::shared_ptr<KeepAlive> state;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        state = keepAlive_;
    }
    if (!state) return;
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        state->stop = true;
    }
    state->wake.notify_all();
    // The heartbeat may be inside a blocking Send rather than waiting on the condition variable,
    // so setting the flag is not enough to reach the join below. Shutting the socket down is what
    // makes a blocked send return -- the same reason waitForAsyncOperations() does it, and the
    // reason #2358 had to make that return an ERROR rather than a SIGPIPE first. Only the
    // destructor calls this, so the socket is being retired anyway.
    std::shared_ptr<System::Net::Sockets::Socket> socket;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        socket = socket_;
    }
    if (socket) {
        try {
            socket->Shutdown(System::Net::Sockets::SocketShutdown::Both);
        } catch (...) {
        }
    }
    // Never called from the heartbeat thread itself -- only the destructor calls this, and the
    // destructor cannot run on a thread this object is still joining. A self-join is the defect
    // #2347 removed from FileSystemWatcher and it is deliberately not reintroduced here: the
    // heartbeat's own fault path calls Abort(), which sets the flag but does NOT join.
    if (state->thread.joinable()) state->thread.join();
}

void ClientWebSocket::keepAliveHeartBeat(const std::shared_ptr<KeepAlive>& state) {
    // One decision, taken under the lock; every action happens after it is released, because
    // sendFrame() blocks and Abort() takes stateMutex_.
    enum class Action { Nothing, UnsolicitedPong, SendPing, Fault };
    Action    action      = Action::Nothing;
    long long pingPayload = 0;
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        if (state->faulted) return;

        if (!state->usesPingPong()) {
            action = Action::UnsolicitedPong;
        } else if (state->pingSent) {
            if (KeepAlive::nowMs() > state->pingTimeoutTick) {
                // KeepAlivePingHeartBeat's timeout branch (ManagedWebSocket.KeepAlive.cs:71-85):
                // .NET records the exception and aborts. So does this, and the recorded reason is
                // what throwIfKeepAliveFaulted surfaces, so a caller learns WHY the socket died
                // rather than only that it did.
                state->faulted = true;
                action         = Action::Fault;
            }
        } else if (KeepAlive::nowMs() > state->nextPingRequestTick) {
            // OnNextPingRequestCore (:188-195).
            state->pingSent        = true;
            state->pingTimeoutTick = KeepAlive::nowMs() + state->timeoutMs;
            ++state->pingPayload;
            action      = Action::SendPing;
            pingPayload = state->pingPayload;
        }
    }

    if (action == Action::Fault) {
        // Abort() sets the stop flag through Dispose() but never joins, so this thread is not
        // self-joining -- the defect #2347 removed from FileSystemWatcher.
        Abort();
        return;
    }
    if (action == Action::Nothing) return;

    try {
        if (action == Action::SendPing) {
            std::vector<bytecs> payload(8);
            for (int i = 0; i < 8; ++i) {
                payload[static_cast<std::size_t>(i)] = static_cast<bytecs>(
                    (static_cast<unsigned long long>(pingPayload) >> ((7 - i) * 8)) & 0xFF);
            }
            sendFrame(0x9, payload.data(), payload.size(), true);
        } else {
            sendFrame(0xA, nullptr, 0, true);
        }
    } catch (...) {
        // .NET's TrySendKeepAliveFrameAsync deliberately swallows: "we can't send any frames, but
        // no need to throw as we are not observing errors anyway" (ManagedWebSocket.KeepAlive.cs
        // :38-46). A socket that has gone away is the ordinary case here, not an error worth
        // reporting from a background thread nobody is awaiting.
    }
}

std::shared_ptr<ClientWebSocket::KeepAlive> ClientWebSocket::keepAliveState() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return keepAlive_;
}

void ClientWebSocket::throwIfKeepAliveFaulted() const {
    auto state = keepAliveState();
    if (!state) return;
    std::lock_guard<std::mutex> lock(state->mutex);
    if (state->faulted) {
        throw WebSocketException(WebSocketError::Faulted,
                                  "The WebSocket keep-alive ping timed out.");
    }
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
    std::shared_ptr<AsyncOperations> ops;
    {
        // #2096: the lazy initialisation was itself a race -- two threads starting their first
        // *Async member at the same time could each construct an AsyncOperations, and the loser's
        // in-flight count would then be invisible to the destructor boundary.
        std::lock_guard<std::mutex> lock(stateMutex_);
        if (!asyncOps_) asyncOps_ = std::make_shared<AsyncOperations>();
        ops = asyncOps_;
    }
    {
        std::lock_guard<std::mutex> lock(ops->mutex);
        ++ops->inFlight;
    }
    return ops;
}

void ClientWebSocket::waitForAsyncOperations() noexcept {
    std::shared_ptr<AsyncOperations> ops;
    std::shared_ptr<System::Net::Sockets::Socket> socket;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        ops = asyncOps_;
        socket = socket_;
    }
    if (!ops) return;
    {
        std::unique_lock<std::mutex> lock(ops->mutex);
        if (ops->inFlight == 0) return;
    }
    // A pending ReceiveAsync is blocked in recv() waiting for a frame the peer may never send,
    // so the boundary has to be able to CROSS -- otherwise it turns a use-after-free into a
    // hang, which is not a repair. shutdown() unblocks recv() reliably (this is the case where
    // it does; #2134 records the one where it does not, a listening socket's accept()).
    // Deliberately not Close(): the descriptor stays this object's until Dispose() retires it,
    // so a worker returning from recv() is not left holding a number the process has reused.
    if (socket) {
        try {
            socket->Shutdown(System::Net::Sockets::SocketShutdown::Both);
        } catch (...) {
            // Already closed or never connected: there is nothing to wake, and the wait below
            // is then the whole boundary.
        }
    }
    // #2096: `ops`, not `asyncOps_` -- the member may not be read here without stateMutex_, and
    // taking it while waiting would hold a lock across an unbounded wait.
    std::unique_lock<std::mutex> lock(ops->mutex);
    ops->idle.wait(lock, [&ops] { return ops->inFlight == 0; });
}

ClientWebSocket::~ClientWebSocket() {
    // #2094: the heartbeat first. It is the only thread that outlives an operation, and joining
    // it here -- never from itself -- is what keeps `this` alive for its whole run.
    stopKeepAlive();
    waitForAsyncOperations();
    Dispose();
}

System::Threading::Tasks::Task
ClientWebSocket::ConnectAsync(const System::Uri& uri, System::Threading::CancellationToken cancellationToken) {
    if (connectStarted_) {
        throw System::InvalidOperationException("ConnectAsync may only be called once.");
    }
    connectStarted_ = true;
    storeState(WebSocketState::Connecting);
    options_.setToReadOnly();

    // #2096 also closes a gap #2088 left: only SendAsync and ReceiveAsync joined the liveness
    // boundary. ConnectAsync, CloseAsync and CloseOutputAsync capture raw `this` exactly the same
    // way, so all five now do.
    auto ops = beginAsyncOperation();
    return System::Threading::Tasks::Task([this, uri, ops, cancellationToken]() {
        AsyncOperationScope release{ops};
        // #2093: constructed BEFORE the handshake, so a token already cancelled here throws
        // without opening a socket -- which is the case the ticket names, where a pre-cancelled
        // `wss` connect used to fault with PlatformNotSupportedException.
        CancellationScope cancellation{this, cancellationToken};
        try {
            performHandshake(uri);
        } catch (...) {
            Dispose();
            if (cancellation.cancelled()) cancellation.rethrowAsCancelled();
            throw;
        }
    });
}

void ClientWebSocket::sendFrame(bytecs opcode, const bytecs* data, size_t len, bool fin) {
    // #2096: taken BEFORE sendMutex_ and held for the whole call, so a concurrent Dispose() can
    // neither delete the socket under `Send` nor leave this thread dereferencing null. Taking it
    // first also keeps the lock order (stateMutex_ then sendMutex_) the same everywhere.
    auto socket = socketForIo();
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

    socket->Send(frame);
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
// Every rule here is an RFC 6455 protocol fact and agrees with the now-available .NET reference.
// The exact exception identity -- WebSocketException with the closest WebSocketError -- remains
// this port's documented mapping.
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
    /// `System::Net::Security::SslApplicationProtocol::isValidUtf8` -- rather than invented;
    /// that implementation was already derived from .NET's ExceptionFallback decoder. It is
    /// copied rather than shared because that one is a *private member*
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
    ///   permanent and readily available public specification"), so they are ACCEPTED. Current
    ///   .NET's ManagedWebSocket accepts the same three values explicitly.
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
    // #2096: one strong reference for the whole frame. `readExact(*socket_, …)` dereferenced the
    // member three times per frame, and a Dispose() between any two of them was a null
    // dereference on a thread the caller could not see.
    auto socket = socketForIo();
    std::vector<bytecs> header;
    readExact(*socket, header, 2);

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
        readExact(*socket, ext, 2);
        len = (static_cast<uint64_t>(ext[0]) << 8) | ext[1];
    } else if (len == 127) {
        std::vector<bytecs> ext;
        readExact(*socket, ext, 8);
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
    readExact(*socket, result.payload, static_cast<size_t>(len));
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
                            bool endOfMessage, System::Threading::CancellationToken cancellationToken) {
    validateWebSocketBuffer(buffer, offset, count);
    // #2088's second half, which the finding recorded as wider than its headline: this used to
    // capture `&buffer`, so the CALLER's vector was a second borrowed object the task could
    // outlive. The destructor boundary below cannot help there -- the buffer is not this object.
    // Send takes it by const reference and only reads it, so copying the bytes it will actually
    // send is a complete fix at the cost of one copy.
    std::vector<bytecs> payload(buffer.begin() + offset, buffer.begin() + offset + count);
    auto ops = beginAsyncOperation();
    return System::Threading::Tasks::Task([this, payload = std::move(payload), count, messageType,
                                            endOfMessage, ops, cancellationToken]() {
        AsyncOperationScope release{ops};
        CancellationScope cancellation{this, cancellationToken};
        try {
            throwIfNotConnected();   // #2357: the OUTER gate, before the per-operation state check
            WebSocket::ThrowOnInvalidState(loadState(), {WebSocketState::Open, WebSocketState::CloseReceived});
            bytecs opcode;
            if (sendContinuation_) {
                opcode = 0x0;
            } else {
                opcode = messageType == WebSocketMessageType::Text ? 0x1 : 0x2;
            }
            sendFrame(opcode, payload.data(), static_cast<size_t>(count), endOfMessage);
            sendContinuation_ = !endOfMessage;
        } catch (...) {
            if (cancellation.cancelled()) cancellation.rethrowAsCancelled();
            throw;
        }
    });
}

System::Threading::Tasks::TaskT<WebSocketReceiveResult>
ClientWebSocket::ReceiveAsync(std::vector<bytecs>& buffer, intcs offset, intcs count,
                               System::Threading::CancellationToken cancellationToken) {
    validateWebSocketBuffer(buffer, offset, count);
    // Receive keeps the reference, and that is not an oversight: `buffer` is the OUT-parameter
    // this operation writes its result into, so a caller that destroys it before awaiting has
    // discarded the result it asked for. The destructor boundary covers `this`; the buffer's
    // lifetime is the caller's, exactly as it is for the synchronous overload.
    auto ops = beginAsyncOperation();
    return System::Threading::Tasks::TaskT<WebSocketReceiveResult>(
        [this, &buffer, offset, count, ops, cancellationToken]() -> WebSocketReceiveResult {
        AsyncOperationScope release{ops};
        CancellationScope cancellation{this, cancellationToken};
        try {
        throwIfNotConnected();   // #2357: the OUTER gate, before the per-operation state check
        WebSocket::ThrowOnInvalidState(loadState(), {WebSocketState::Open, WebSocketState::CloseSent});

        if (recvLeftoverPos_ < recvLeftover_.size()) {
            size_t remaining = recvLeftover_.size() - recvLeftoverPos_;
            size_t n = std::min(static_cast<size_t>(count), remaining);
            // Even a zero-byte memcpy requires valid pointer arguments. An empty caller buffer
            // legitimately has data() == nullptr, so leave both pointers untouched when n is 0.
            if (n != 0) {
                std::memcpy(buffer.data() + offset, recvLeftover_.data() + recvLeftoverPos_, n);
            }
            recvLeftoverPos_ += n;
            bool drained = recvLeftoverPos_ == recvLeftover_.size();
            if (drained) {
                recvLeftover_.clear();
                recvLeftoverPos_ = 0;
            }
            // #2095: `endOfMessage` is the FRAME's FIN, not the buffer's exhaustion. This used to
            // report true whenever the leftover ran out, so the tail of a non-final frame claimed
            // the message had ended -- .NET returns header.EndOfMessage (ManagedWebSocket.cs:995).
            return WebSocketReceiveResult(static_cast<intcs>(n), recvLeftoverType_,
                                           drained && recvLeftoverFinal_);
        }

        while (true) {
            RawFrame frame = readFrame();
            // #2094 / OnDataReceived (ManagedWebSocket.KeepAlive.cs:154-160): ANY received frame
            // pushes the next ping out, so a busy connection is never pinged.
            if (auto ka = keepAliveState()) ka->onDataReceived();
            switch (frame.opcode) {
                case 0x9: { // Ping
                    sendFrame(0xA, frame.payload.data(), frame.payload.size(), true);
                    continue;
                }
                case 0xA: // Pong
                    // #2094: a Pong whose payload matches the outstanding Ping clears it. An
                    // unsolicited Pong from the peer is ignored rather than treated as an answer,
                    // matching OnPongResponseReceived (ManagedWebSocket.KeepAlive.cs:162-185).
                    if (auto ka = keepAliveState()) ka->onPongReceived(frame.payload);
                    continue;
                case 0x8: { // Close
                    std::optional<WebSocketCloseStatus> receivedStatus;
                    std::optional<std::string> receivedDescription;
                    // Ticket #2091. Throws before closeStatus_/state_ are touched, so a
                    // malformed close frame cannot leave the socket reporting a clean close.
                    parseClosePayload(frame.payload, receivedStatus, receivedDescription);
                    {
                        // #2096: one critical section, so a reader never sees the close status
                        // published against the old state or the new state against the old status.
                        std::lock_guard<std::mutex> lock(stateMutex_);
                        closeStatus_ = receivedStatus;
                        closeStatusDescription_ = receivedDescription;
                        state_ = state_ == WebSocketState::CloseSent ? WebSocketState::Closed
                                                                     : WebSocketState::CloseReceived;
                    }
                    return WebSocketReceiveResult(0, WebSocketMessageType::Close, true, receivedStatus, receivedDescription);
                }
                default: { // Continuation(0x0)/Text(0x1)/Binary(0x2)
                    // #2095 / RFC 6455 §5.4, and .NET does check it (ManagedWebSocket.cs:1382-1411).
                    // The review deferred this because "rejecting is defensible and so is
                    // tolerating, and .NET's exact choice is not verifiable with /rv absent". It
                    // is verifiable now, and .NET REJECTS both orderings -- with these messages.
                    if (frame.opcode == 0x0) {
                        if (lastReceivedFrameWasFinal_) {
                            throw WebSocketException(
                                WebSocketError::Faulted,
                                "The WebSocket received a continuation frame from a previous final message.");
                        }
                    } else if (!lastReceivedFrameWasFinal_) {
                        throw WebSocketException(
                            WebSocketError::Faulted,
                            "The WebSocket expected a continuation frame after having received a "
                            "previous non-final frame.");
                    }

                    WebSocketMessageType type =
                        frame.opcode == 0x0 ? fragmentType_ : (frame.opcode == 0x1 ? WebSocketMessageType::Text : WebSocketMessageType::Binary);
                    if (frame.opcode != 0x0) fragmentType_ = type;
                    // Only DATA frames update it, matching .NET: control frames never reach
                    // `_lastReceiveHeader = header`.
                    lastReceivedFrameWasFinal_ = frame.fin;

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
                        if (!frame.payload.empty()) {
                            std::memcpy(buffer.data() + offset, frame.payload.data(), frame.payload.size());
                        }
                        return WebSocketReceiveResult(static_cast<intcs>(frame.payload.size()), type, frame.fin);
                    }

                    if (count != 0) {
                        std::memcpy(buffer.data() + offset, frame.payload.data(), static_cast<size_t>(count));
                    }
                    recvLeftover_.assign(frame.payload.begin() + count, frame.payload.end());
                    recvLeftoverPos_ = 0;
                    recvLeftoverType_ = type;
                    recvLeftoverFinal_ = frame.fin;   // #2095: carried so the tail reports the FRAME's FIN
                    return WebSocketReceiveResult(count, type, false);
                }
            }
        }
        } catch (...) {
            if (cancellation.cancelled()) cancellation.rethrowAsCancelled();
            // #2094: if the keep-alive aborted the socket, say so. Otherwise the caller sees only
            // the generic invalid-state failure the abort produced and cannot tell a dead peer
            // from a local Dispose(). .NET surfaces the same cause (ManagedWebSocket.cs:1016-1021).
            throwIfKeepAliveFaulted();
            throw;
        }
    });
}

System::Threading::Tasks::Task
ClientWebSocket::CloseOutputAsync(WebSocketCloseStatus closeStatus, const std::optional<std::string>& statusDescription,
                                    System::Threading::CancellationToken cancellationToken) {
    auto ops = beginAsyncOperation();  // #2096: was outside the #2088 boundary
    return System::Threading::Tasks::Task([this, closeStatus, statusDescription, ops, cancellationToken]() {
        AsyncOperationScope release{ops};
        CancellationScope cancellation{this, cancellationToken};
        try {
            throwIfNotConnected();   // #2357: the OUTER gate, before the per-operation state check
            WebSocket::ThrowOnInvalidState(loadState(), {WebSocketState::Open, WebSocketState::CloseReceived});
            sendCloseFrame(closeStatus, statusDescription);
            std::lock_guard<std::mutex> lock(stateMutex_);
            state_ = state_ == WebSocketState::CloseReceived ? WebSocketState::Closed : WebSocketState::CloseSent;
        } catch (...) {
            if (cancellation.cancelled()) cancellation.rethrowAsCancelled();
            throw;
        }
    });
}

System::Threading::Tasks::Task
ClientWebSocket::CloseAsync(WebSocketCloseStatus closeStatus, const std::optional<std::string>& statusDescription,
                              System::Threading::CancellationToken cancellationToken) {
    // The old comment here was right about the problem and wrong about the cost. It said real
    // cancellation "would need to interrupt the loop's blocking socket read mid-flight, which
    // only makes sense done consistently across all of this file's async methods at once". The
    // first half is true; the second turned out to be nine lines, because .NET's answer is to
    // ABORT the whole WebSocket on cancellation rather than to interrupt one read
    // (ManagedWebSocket.cs:608,789). #2093 does it consistently across all five members, which
    // is what that comment asked for.
    auto ops = beginAsyncOperation();  // #2096: was outside the #2088 boundary
    return System::Threading::Tasks::Task([this, closeStatus, statusDescription, ops, cancellationToken]() {
        AsyncOperationScope release{ops};
        CancellationScope cancellation{this, cancellationToken};
        try {
        // #2357: CloseAsync goes through .NET's ConnectedWebSocket gate too, so a DISPOSED
        // instance faults here rather than treating the call as a no-op. It has no
        // per-operation ThrowOnInvalidState of its own -- a close on an already-closed but
        // still-live socket really is a no-op, and stays one.
        throwIfNotConnected();
        if (loadState() == WebSocketState::Open) {
            sendCloseFrame(closeStatus, statusDescription);
            storeState(WebSocketState::CloseSent);
        }
        if (loadState() == WebSocketState::CloseSent) {
            // #2096: the loop condition is re-read under the lock on every turn, and readFrame()
            // now throws rather than dereferencing null if Dispose() takes the socket away -- so
            // an aborted close handshake ends with a WebSocketException instead of a crash or an
            // endless wait for a state another thread will never publish.
            while (loadState() != WebSocketState::Closed) {
                RawFrame frame = readFrame();
                if (frame.opcode == 0x8) {
                    // Ticket #2091: the same parser as ReceiveAsync's, not a second copy of it.
                    // This site had an identical unvalidated `>= 2` block, so a malformed close
                    // frame arriving during the close handshake was accepted here even after the
                    // receive path had been repaired.
                    std::optional<WebSocketCloseStatus> receivedStatus;
                    std::optional<std::string> receivedDescription;
                    parseClosePayload(frame.payload, receivedStatus, receivedDescription);
                    std::lock_guard<std::mutex> lock(stateMutex_);
                    closeStatus_ = receivedStatus;
                    closeStatusDescription_ = receivedDescription;
                    state_ = WebSocketState::Closed;
                }
                // Any other frame received while waiting for the close handshake is discarded.
            }
        }
        } catch (...) {
            if (cancellation.cancelled()) cancellation.rethrowAsCancelled();
            throw;
        }
    });
}

void ClientWebSocket::Abort() {
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        if (state_ != WebSocketState::Aborted && state_ != WebSocketState::Closed) {
            state_ = WebSocketState::Aborted;
        }
    }
    Dispose();
}

void ClientWebSocket::Dispose() {
    // #2096. This used to call socket_->Close() and reset the unique_ptr while a task thread
    // could be inside Send() or recv() on that very descriptor: first a null dereference for
    // whichever call came next, and -- worse -- a closed file descriptor number the process is
    // free to hand to something else while a worker is still syscalling on it.
    //
    // Shared ownership makes the order safe. The member is taken away under the lock so no new
    // operation can start on it, then the socket is SHUT DOWN (which unblocks a worker parked in
    // recv), and only then is this function's own reference dropped. If a worker still holds one,
    // the descriptor stays open until that worker returns -- microseconds, and never reused
    // underneath it. Deliberately not Close(): closing a descriptor another thread is blocked on
    // is the hazard this repair exists to remove, not a way to implement it.
    std::shared_ptr<System::Net::Sockets::Socket> socket;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        socket.swap(socket_);
        if (state_ != WebSocketState::Aborted) {
            state_ = WebSocketState::Closed;
        }
    }
    if (socket) {
        try {
            socket->Shutdown(System::Net::Sockets::SocketShutdown::Both);
        } catch (...) {
            // Never connected, or already shut down by waitForAsyncOperations().
        }
    }
}

} // namespace System::Net::WebSockets
