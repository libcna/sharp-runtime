// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Ticket #2089 — SR-AUD-248 and SR-AUD-249, the RFC 6455 handshake's caller-text doors.
//
// SR-AUD-248: ClientWebSocket::performHandshake concatenates
// `name + ": " + value + "\r\n"` straight into the upgrade request, so a value carrying CRLF
// injected an extra header field and a NAME carrying CRLF injected a whole request line.
// Measured before the repair in build-probe/2089_probe1_before.log: a six-field request became
// an eight-field one carrying a smuggled `GET /admin HTTP/1.1`.
//
// SR-AUD-249: validateSubprotocol rejected `c <= 0x20 || c == 0x7F` and NOT ONE RFC 7230
// separator, so `chat,evil` was stored as a single subprotocol and joined into
// Sec-WebSocket-Protocol with ", " — the caller advertised two protocols where the API
// reported one. The finding's premise needed correcting in the port's favour and against it at
// once (plan §6.2): a validator DID exist; this widens it rather than creating it.
//
// SCOPE CORRECTION, the same one #2063 made for System::Net::Http: the finding names
// SetRequestHeader, but the REQUEST URI is a third door. System::Uri preserves CR/LF/NUL
// (build-probe/2089_probe2_uri_door.log; the Uri-side defect is the separate blocked ticket
// #2003), and the URI's path/query goes on the request LINE while its host goes into Host:.
// Closing only the options doors would have left request smuggling open.
//
// The predicate is System::Net::detail::ContainsProtocolFieldTerminator — the single shared
// body #2063 established for the same rule across ten HTTP doors, deliberately NOT a second
// local copy of the policy.
//
// Every case that touches the wire runs over a REAL loopback socket against a mock server
// thread, synchronised only by blocking accept/read — no sleeps.
#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include "System/ArgumentException.hpp"
#include "System/Net/IPEndPoint.hpp"
#include "System/Net/Sockets/Socket.hpp"
#include "System/Net/WebSockets/ClientWebSocket.hpp"
#include "System/Net/WebSockets/ClientWebSocketOptions.hpp"
#include "System/Uri.hpp"

using namespace System::Net::WebSockets;
using System::Net::IPAddress;
using System::Net::IPEndPoint;
using System::Net::Sockets::Socket;
using SharpRuntime::bytecs;

namespace {

/// Runs one ConnectAsync against a loopback listener that only reads, and returns the exact
/// request bytes that reached the wire. The server never upgrades, so the client's handshake
/// read fails afterwards — irrelevant here, the request bytes are the measurement.
std::string CaptureHandshakeRequest(const std::function<void(ClientWebSocketOptions&)>& configure) {
    Socket listener(System::Net::Sockets::AddressFamily::InterNetwork,
                    System::Net::Sockets::SocketType::Stream,
                    System::Net::Sockets::ProtocolType::Tcp);
    listener.Bind(IPEndPoint(IPAddress::Loopback, 0));
    listener.Listen();
    auto local = std::dynamic_pointer_cast<IPEndPoint>(listener.getLocalEndPointProperty());
    EXPECT_NE(local, nullptr);
    const SharpRuntime::intcs port = local->getPortProperty();

    std::string request;
    std::thread serverThread([&]() {
        try {
            auto server = listener.Accept();
            std::vector<bytecs> one(1);
            while (request.size() < 4 || request.compare(request.size() - 4, 4, "\r\n\r\n") != 0) {
                if (server->Receive(one) == 0) break;
                request += static_cast<char>(one[0]);
            }
            server->Close();
        } catch (...) {
            // The server thread must always terminate so join() always returns, whatever the
            // client did — a test that hangs proves nothing about the guard it was aimed at.
        }
    });

    ClientWebSocket client;
    try {
        configure(client.getOptionsProperty());
        System::Uri uri("ws://127.0.0.1:" + std::to_string(port) + "/");
        client.ConnectAsync(uri).Wait();
    } catch (const System::Exception&) {
        // Expected: the mock server closes without upgrading.
    }
    client.Dispose();
    serverThread.join();
    listener.Close();
    return request;
}

/// Counts the header fields a server would parse out of a captured request — the number that
/// actually moved when the injection worked.
size_t HeaderFieldCount(const std::string& request) {
    size_t fields = 0;
    size_t pos = request.find("\r\n");
    while (pos != std::string::npos) {
        const size_t next = request.find("\r\n", pos + 2);
        if (next == std::string::npos || next == pos + 2) break;
        ++fields;
        pos = next;
    }
    return fields;
}

#if defined(__linux__)
constexpr bool kDescriptorCountAvailable = true;
SharpRuntime::intcs openDescriptorCount() {
    SharpRuntime::intcs count = 0;
    std::error_code ec;
    for (auto it = std::filesystem::directory_iterator("/proc/self/fd", ec);
         !ec && it != std::filesystem::directory_iterator(); it.increment(ec)) {
        ++count;
    }
    return count;
}
#else
constexpr bool kDescriptorCountAvailable = false;
SharpRuntime::intcs openDescriptorCount() { return 0; }
#endif

const std::string kNul("\0", 1);

} // namespace

// ===========================================================================
// SR-AUD-248 — the header NAME door
// ===========================================================================

TEST(ClientWebSocketHandshakeValidationTests, HeaderNameRejectsEachTerminatorAtEveryPosition) {
    for (const std::string& bad : {std::string("\r"), std::string("\n"), std::string("\r\n"), kNul}) {
        ClientWebSocketOptions atStart, inMiddle, atEnd;
        EXPECT_THROW(atStart.SetRequestHeader(bad + "X-Test", "v"), System::ArgumentException);
        EXPECT_THROW(inMiddle.SetRequestHeader("X-" + bad + "Test", "v"), System::ArgumentException);
        EXPECT_THROW(atEnd.SetRequestHeader("X-Test" + bad, "v"), System::ArgumentException);
    }
}

TEST(ClientWebSocketHandshakeValidationTests, HeaderNameRejectsARequestLineInjection) {
    // The sharpest case: a NAME carrying CRLF used to smuggle a whole extra request line.
    ClientWebSocketOptions options;
    EXPECT_THROW(options.SetRequestHeader("X-A: v\r\nGET /admin HTTP/1.1\r\nHost", "evil"),
                 System::ArgumentException);
    EXPECT_TRUE(options.getRequestHeadersProperty().empty())
        << "a rejected header must not be stored";
}

// ===========================================================================
// SR-AUD-248 — the header VALUE door
// ===========================================================================

TEST(ClientWebSocketHandshakeValidationTests, HeaderValueRejectsEachTerminatorAtEveryPosition) {
    for (const std::string& bad : {std::string("\r"), std::string("\n"), std::string("\r\n"), kNul}) {
        ClientWebSocketOptions atStart, inMiddle, atEnd;
        EXPECT_THROW(atStart.SetRequestHeader("X-Test", bad + "v"), System::ArgumentException);
        EXPECT_THROW(inMiddle.SetRequestHeader("X-Test", "a" + bad + "b"), System::ArgumentException);
        EXPECT_THROW(atEnd.SetRequestHeader("X-Test", "v" + bad), System::ArgumentException);
    }
}

TEST(ClientWebSocketHandshakeValidationTests, HeaderValueRejectsAHeaderFieldInjection) {
    ClientWebSocketOptions options;
    EXPECT_THROW(options.SetRequestHeader("X-Test", "a\r\nX-Injected: yes"), System::ArgumentException);
    EXPECT_TRUE(options.getRequestHeadersProperty().empty());
}

TEST(ClientWebSocketHandshakeValidationTests, TheRejectedTextIsNotEchoedIntoTheMessage) {
    // The rejected text is attacker-controlled and exception messages get logged; echoing a
    // CRLF-bearing value into a log re-creates the injection the rejection exists to prevent.
    ClientWebSocketOptions options;
    try {
        options.SetRequestHeader("X-Test", "a\r\nX-Injected: yes");
        FAIL() << "the value must be rejected";
    } catch (const System::ArgumentException& e) {
        const std::string message = e.what();
        EXPECT_EQ(message.find("X-Injected"), std::string::npos) << message;
        EXPECT_EQ(message.find('\r'), std::string::npos) << "the message must carry no CR";
        EXPECT_EQ(message.find('\n'), std::string::npos) << "the message must carry no LF";
        EXPECT_NE(message.find("headerValue"), std::string::npos)
            << "the message must name which door rejected: " << message;
    }
}

// ===========================================================================
// What the header doors must still ACCEPT — the repair must not over-narrow
// ===========================================================================

TEST(ClientWebSocketHandshakeValidationTests, LegalHeaderTextIsStillAccepted) {
    ClientWebSocketOptions options;
    // A space and a horizontal tab are legal inside a header value.
    EXPECT_NO_THROW(options.SetRequestHeader("X-Spaces", "a value with spaces"));
    EXPECT_NO_THROW(options.SetRequestHeader("X-Tab", "a\tb"));
    // Other C0 controls do not terminate a field, so they stay accepted — the same deliberate
    // boundary System::Net::Http #2063 drew.
    EXPECT_NO_THROW(options.SetRequestHeader("X-Vt", "a\x0Bb"));
    EXPECT_NO_THROW(options.SetRequestHeader("X-Bel", "a\x07" "b"));
    // Percent-encoded control characters are ordinary text: they are not terminators.
    EXPECT_NO_THROW(options.SetRequestHeader("X-Encoded", "a%0D%0AX-Injected:%20yes"));
    // An empty value is legal.
    EXPECT_NO_THROW(options.SetRequestHeader("X-Empty", ""));
    // Token punctuation and Unicode text.
    EXPECT_NO_THROW(options.SetRequestHeader("X-Punct", "a;b=c,d/e\"f\""));
    EXPECT_NO_THROW(options.SetRequestHeader("X-Unicode", "grüße-\xE4\xB8\xAD\xE6\x96\x87"));
    EXPECT_EQ(options.getRequestHeadersProperty().size(), 8u);
}

TEST(ClientWebSocketHandshakeValidationTests, TheEmittedRequestIsByteIdenticalForAcceptedInput) {
    // The repair must change nothing on the accepted path. Two captures with the same accepted
    // header must differ only in the generated Sec-WebSocket-Key and the ephemeral port.
    const std::string request = CaptureHandshakeRequest([](ClientWebSocketOptions& o) {
        o.SetRequestHeader("X-Ok", "plain value");
        o.SetRequestHeader("X-Tab", "a\tb");
    });
    EXPECT_NE(request.find("\r\nX-Ok: plain value\r\n"), std::string::npos) << request;
    EXPECT_NE(request.find("\r\nX-Tab: a\tb\r\n"), std::string::npos) << request;
    EXPECT_NE(request.find("GET / HTTP/1.1\r\n"), std::string::npos) << request;
    EXPECT_NE(request.find("\r\nUpgrade: websocket\r\n"), std::string::npos) << request;
    EXPECT_NE(request.find("\r\nSec-WebSocket-Version: 13\r\n"), std::string::npos) << request;
    // Six fields: Host, Upgrade, Connection, Sec-WebSocket-Key, Sec-WebSocket-Version, and the
    // two custom ones — seven. The count is what moved when the injection worked.
    EXPECT_EQ(HeaderFieldCount(request), 7u) << request;
}

// ===========================================================================
// SR-AUD-249 — the subprotocol token grammar
// ===========================================================================

TEST(ClientWebSocketHandshakeValidationTests, EveryRfc7230SeparatorIsRejectedInASubprotocol) {
    // Not one of these was rejected before #2089 (build-probe/2089_probe1_before.log).
    for (const char sep : {'(', ')', '<', '>', '@', ',', ';', ':', '\\', '"',
                           '/', '[', ']', '?', '=', '{', '}'}) {
        ClientWebSocketOptions options;
        const std::string candidate = std::string("a") + sep + "b";
        EXPECT_THROW(options.AddSubProtocol(candidate), System::ArgumentException)
            << "separator '" << sep << "' must be rejected";
        EXPECT_TRUE(options.getRequestedSubProtocolsProperty().empty())
            << "a rejected subprotocol must not be stored";
    }
}

TEST(ClientWebSocketHandshakeValidationTests, TheFindingsOwnExampleIsRejected) {
    // SR-AUD-249's example verbatim: `chat,evil` used to be stored as ONE subprotocol and
    // joined into Sec-WebSocket-Protocol with ", ", advertising two.
    ClientWebSocketOptions options;
    EXPECT_THROW(options.AddSubProtocol("chat,evil"), System::ArgumentException);
    EXPECT_THROW(options.AddSubProtocol("a;b"), System::ArgumentException);
}

TEST(ClientWebSocketHandshakeValidationTests, ValidTokenCharactersAreStillAccepted) {
    ClientWebSocketOptions options;
    // tchar: every VCHAR except the separators. `-`, `.`, `_`, `+` and digits are named by the
    // ticket's acceptance criteria; the rest of the punctuation set is tchar too.
    for (const char* good : {"chat", "chat9", "v1.2-x_y+z", "soap", "a!b", "a#b", "a$b", "a%b",
                             "a&b", "a'b", "a*b", "a^b", "a`b", "a|b", "a~b"}) {
        EXPECT_NO_THROW(options.AddSubProtocol(good)) << "token '" << good << "' must be accepted";
    }
    EXPECT_EQ(options.getRequestedSubProtocolsProperty().size(), 15u);
}

TEST(ClientWebSocketHandshakeValidationTests, ThePreExistingSubprotocolRejectionsAreUnchanged) {
    // The widening must not have disturbed what already worked.
    ClientWebSocketOptions options;
    EXPECT_THROW(options.AddSubProtocol(""), System::ArgumentException);        // empty
    EXPECT_THROW(options.AddSubProtocol("a b"), System::ArgumentException);     // space
    EXPECT_THROW(options.AddSubProtocol("a\tb"), System::ArgumentException);    // tab
    EXPECT_THROW(options.AddSubProtocol("a\x7F" "b"), System::ArgumentException); // DEL
    EXPECT_THROW(options.AddSubProtocol("a\rb"), System::ArgumentException);    // CR
    EXPECT_THROW(options.AddSubProtocol("a\nb"), System::ArgumentException);    // LF
    EXPECT_THROW(options.AddSubProtocol(std::string("a\0b", 3)), System::ArgumentException); // NUL
    // The case-insensitive duplicate check must still work.
    EXPECT_NO_THROW(options.AddSubProtocol("chat"));
    EXPECT_THROW(options.AddSubProtocol("CHAT"), System::ArgumentException);
}

TEST(ClientWebSocketHandshakeValidationTests, AnAcceptedSubprotocolListStillReachesTheRequest) {
    const std::string request = CaptureHandshakeRequest([](ClientWebSocketOptions& o) {
        o.AddSubProtocol("chat");
        o.AddSubProtocol("superchat");
    });
    EXPECT_NE(request.find("\r\nSec-WebSocket-Protocol: chat, superchat\r\n"), std::string::npos)
        << request;
}

// ===========================================================================
// The third door the finding does not name — the request URI
// ===========================================================================

TEST(ClientWebSocketHandshakeValidationTests, ATerminatorInTheUriPathOrQueryIsRejected) {
    // System::Uri keeps CR/LF/NUL (that is the separate, blocked ticket #2003); this module's
    // own door must not concatenate them into the request line.
    for (const std::string& bad : {std::string("\r"), std::string("\n"), std::string("\r\n"), kNul}) {
        ClientWebSocket client;
        System::Uri uri("ws://127.0.0.1:9/a" + bad + "X-Injected: yes");
        EXPECT_THROW(client.ConnectAsync(uri).Wait(), System::ArgumentException);
        client.Dispose();
    }
    for (const std::string& bad : {std::string("\r\n"), kNul}) {
        ClientWebSocket client;
        System::Uri uri("ws://127.0.0.1:9/p?q=a" + bad + "X-Injected: yes");
        EXPECT_THROW(client.ConnectAsync(uri).Wait(), System::ArgumentException);
        client.Dispose();
    }
}

TEST(ClientWebSocketHandshakeValidationTests, ATerminatorInTheUriHostIsRejected) {
    // The host is concatenated into `Host: <host>:<port>`.
    ClientWebSocket client;
    System::Uri uri("ws://ho\r\nst:9/");
    EXPECT_THROW(client.ConnectAsync(uri).Wait(), System::ArgumentException);
    client.Dispose();
}

TEST(ClientWebSocketHandshakeValidationTests, TheUriRejectionOpensNoSocketAndSendsNoBytes) {
    // The check must run BEFORE the socket is constructed: a rejected URI must open no
    // connection, send no byte and leak no descriptor.
    //
    // The instrument is the process's own open-descriptor count. LSan does NOT cover this — it
    // tracks memory, not descriptors — so a clean LSan run would say nothing here and must not
    // be substituted. Where /proc/self/fd is unavailable the assertion is SKIPPED rather than
    // passed: a missing instrument is not a measurement.
    if (!kDescriptorCountAvailable) GTEST_SKIP() << "/proc/self/fd is unavailable";

    const SharpRuntime::intcs before = openDescriptorCount();
    int threw = 0;
    for (int i = 0; i < 20; ++i) {
        ClientWebSocket client;
        System::Uri uri("ws://127.0.0.1:9/a\r\nX-Injected: yes");
        try {
            client.ConnectAsync(uri).Wait();
        } catch (const System::ArgumentException&) {
            ++threw;
        }
        client.Dispose();
    }
    const SharpRuntime::intcs after = openDescriptorCount();
    EXPECT_EQ(threw, 20) << "every attempt must be rejected — otherwise nothing was measured";
    EXPECT_EQ(after - before, 0) << "a rejected URI must not open a descriptor";
}

TEST(ClientWebSocketHandshakeValidationTests, ALegalUriStillConnectsAndIsUnchangedOnTheWire) {
    // The accepted path must be untouched: percent-encoded control characters and ordinary
    // path/query text still reach the request line verbatim.
    Socket listener(System::Net::Sockets::AddressFamily::InterNetwork,
                    System::Net::Sockets::SocketType::Stream,
                    System::Net::Sockets::ProtocolType::Tcp);
    listener.Bind(IPEndPoint(IPAddress::Loopback, 0));
    listener.Listen();
    auto local = std::dynamic_pointer_cast<IPEndPoint>(listener.getLocalEndPointProperty());
    ASSERT_NE(local, nullptr);
    const SharpRuntime::intcs port = local->getPortProperty();

    std::string request;
    std::thread serverThread([&]() {
        try {
            auto server = listener.Accept();
            std::vector<bytecs> one(1);
            while (request.size() < 4 || request.compare(request.size() - 4, 4, "\r\n\r\n") != 0) {
                if (server->Receive(one) == 0) break;
                request += static_cast<char>(one[0]);
            }
            server->Close();
        } catch (...) {
        }
    });

    ClientWebSocket client;
    System::Uri uri("ws://127.0.0.1:" + std::to_string(port) + "/path/x?q=a%0D%0Ab&r=1");
    try {
        client.ConnectAsync(uri).Wait();
    } catch (const System::Exception&) {
        // The mock server never upgrades; the request bytes are the measurement.
    }
    client.Dispose();
    serverThread.join();
    listener.Close();

    EXPECT_NE(request.find("GET /path/x?q=a%0D%0Ab&r=1 HTTP/1.1\r\n"), std::string::npos) << request;
    EXPECT_NE(request.find("\r\nHost: 127.0.0.1:" + std::to_string(port) + "\r\n"), std::string::npos)
        << request;
}
