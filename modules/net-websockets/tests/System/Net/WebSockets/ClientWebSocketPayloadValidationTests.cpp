// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Ticket #2091 — the WebSocket frame PAYLOAD, where #2090 validated the frame HEADER.
// docs/SystemNetWebSocketsNamespaceReviewPlan.md §7.6, §7.7, §7.8, §7.10. Every defect below is
// driven by the server the client connected to, and every one was measured open in
// build-probe/2091_probe1_before.log:
//
//   §7.6  a 1-byte close payload was silently ignored (the guard was `payload.size() >= 2`),
//         leaving closeStatus_ unset and reporting a CLEAN close;
//   §7.7  any 16-bit value was cast straight to WebSocketCloseStatus, so 0, 999, 1004, 1005,
//         1006, 1015 and 5000 all reached the public getCloseStatusProperty();
//   §7.8  Text payloads and close reasons were never UTF-8 validated;
//   §7.10 the response's Sec-WebSocket-Protocol was stored without checking it against the
//         requested list, or that any was requested at all — the server chose the client's
//         SubProtocol freely.
//
// The close payload was decoded in TWO places — ReceiveAsync's `case 0x8` and CloseAsync's
// close-handshake loop — by two copies of the same unvalidated block. Both now go through one
// parser, and both are tested here: two structurally identical parsers of remote input are
// exactly how one gets fixed and the other does not.
//
// Rules are RFC 6455 cited as PROTOCOL facts, not as .NET behaviour (the reference tree is
// absent). The exception identity — WebSocketException with WebSocketError::Faulted for a
// payload violation, distinct from #2090's HeaderError for a header violation — and the
// decision to REJECT rather than ignore an unrequested subprotocol are this port's recorded
// choices.
//
// Every case runs over a REAL loopback socket against a mock server thread, synchronised only
// by blocking accept/read — no sleeps.
#include <gtest/gtest.h>

#include <array>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "System/Convert.hpp"
#include "System/Net/IPEndPoint.hpp"
#include "System/Net/Sockets/Socket.hpp"
#include "System/Net/WebSockets/ClientWebSocket.hpp"
#include "System/Net/WebSockets/WebSocketException.hpp"
#include "System/Uri.hpp"

using namespace System::Net::WebSockets;
using System::Net::IPAddress;
using System::Net::IPEndPoint;
using System::Net::Sockets::Socket;
using SharpRuntime::bytecs;

namespace {

std::array<uint8_t, 20> payloadTestSha1(const std::string& input) {
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
            if (i < 20)      { f = (b & c) | ((~b) & d);       k = 0x5A827999; }
            else if (i < 40) { f = b ^ c ^ d;                   k = 0x6ED9EBA1; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
            else             { f = b ^ c ^ d;                   k = 0xCA62C1D6; }
            uint32_t temp = ((a << 5) | (a >> 27)) + f + e + k + w[i];
            e = d; d = c; c = (b << 30) | (b >> 2); b = a; a = temp;
        }
        h0 += a; h1 += b; h2 += c; h3 += d; h4 += e;
    }
    std::array<uint8_t, 20> digest{};
    uint32_t hs[5] = {h0, h1, h2, h3, h4};
    for (int i = 0; i < 5; ++i) {
        digest[i * 4]     = static_cast<uint8_t>((hs[i] >> 24) & 0xFF);
        digest[i * 4 + 1] = static_cast<uint8_t>((hs[i] >> 16) & 0xFF);
        digest[i * 4 + 2] = static_cast<uint8_t>((hs[i] >> 8) & 0xFF);
        digest[i * 4 + 3] = static_cast<uint8_t>(hs[i] & 0xFF);
    }
    return digest;
}

std::vector<bytecs> toBytes(const std::string& s) { return std::vector<bytecs>(s.begin(), s.end()); }

std::string readHeaders(Socket& socket) {
    std::string response;
    std::vector<bytecs> one(1);
    while (response.size() < 4 || response.compare(response.size() - 4, 4, "\r\n\r\n") != 0) {
        if (socket.Receive(one) == 0) break;
        response += static_cast<char>(one[0]);
    }
    return response;
}

/// Completes an RFC 6455 upgrade — optionally echoing `respondProtocol` in the response — then
/// hands the connected socket to `afterHandshake` and runs `clientBody`.
void RunAgainstServer(const std::string& respondProtocol,
                      const std::function<void(ClientWebSocketOptions&)>& configure,
                      const std::function<void(Socket&)>& afterHandshake,
                      const std::function<void(ClientWebSocket&)>& clientBody) {
    Socket listener(System::Net::Sockets::AddressFamily::InterNetwork,
                    System::Net::Sockets::SocketType::Stream,
                    System::Net::Sockets::ProtocolType::Tcp);
    listener.Bind(IPEndPoint(IPAddress::Loopback, 0));
    listener.Listen();
    auto local = std::dynamic_pointer_cast<IPEndPoint>(listener.getLocalEndPointProperty());
    ASSERT_NE(local, nullptr);
    const SharpRuntime::intcs port = local->getPortProperty();

    std::thread serverThread([&]() {
        try {
            auto server = listener.Accept();
            const std::string request = readHeaders(*server);
            const size_t keyPos = request.find("Sec-WebSocket-Key: ");
            ASSERT_NE(keyPos, std::string::npos);
            const size_t keyStart = keyPos + std::string("Sec-WebSocket-Key: ").size();
            const std::string key = request.substr(keyStart, request.find("\r\n", keyStart) - keyStart);
            const auto digest = payloadTestSha1(key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
            const std::string accept =
                System::Convert::ToBase64String(std::vector<bytecs>(digest.begin(), digest.end()));
            std::string response = "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\n"
                                   "Connection: Upgrade\r\nSec-WebSocket-Accept: " + accept + "\r\n";
            if (!respondProtocol.empty()) response += "Sec-WebSocket-Protocol: " + respondProtocol + "\r\n";
            response += "\r\n";
            server->Send(toBytes(response));
            afterHandshake(*server);
            server->Close();
        } catch (...) {
            // The server thread must always terminate so join() always returns.
        }
    });

    ClientWebSocket client;
    configure(client.getOptionsProperty());
    System::Uri uri("ws://127.0.0.1:" + std::to_string(port) + "/");
    client.ConnectAsync(uri).Wait();
    ASSERT_EQ(client.getStateProperty(), WebSocketState::Open);
    clientBody(client);
    client.Dispose();
    serverThread.join();
    listener.Close();
}

void noConfig(ClientWebSocketOptions&) {}

std::vector<bytecs> CloseFrame(const std::vector<bytecs>& payload) {
    std::vector<bytecs> f{0x88, static_cast<bytecs>(payload.size())};
    f.insert(f.end(), payload.begin(), payload.end());
    return f;
}

/// The server sends one close frame; the client's ReceiveAsync must throw.
void ExpectCloseRejected(const std::vector<bytecs>& payload, const char* what) {
    RunAgainstServer(
        "", noConfig,
        [&](Socket& server) { server.Send(CloseFrame(payload)); },
        [&](ClientWebSocket& client) {
            std::vector<bytecs> buffer(1024);
            EXPECT_THROW(client.ReceiveAsync(buffer, 0, 1024).Wait(), WebSocketException) << what;
        });
}

/// The server sends one close frame; the client's ReceiveAsync must accept it and report
/// `expected` from getCloseStatusProperty().
void ExpectCloseAccepted(const std::vector<bytecs>& payload, int expectedCode, const char* what) {
    RunAgainstServer(
        "", noConfig,
        [&](Socket& server) { server.Send(CloseFrame(payload)); },
        [&](ClientWebSocket& client) {
            std::vector<bytecs> buffer(1024);
            auto result = client.ReceiveAsync(buffer, 0, 1024).getResultProperty();
            EXPECT_EQ(result.getMessageTypeProperty(), WebSocketMessageType::Close) << what;
            const auto status = client.getCloseStatusProperty();
            if (expectedCode < 0) {
                EXPECT_FALSE(status.has_value()) << what;
            } else {
                ASSERT_TRUE(status.has_value()) << what;
                EXPECT_EQ(static_cast<int>(*status), expectedCode) << what;
            }
        });
}

std::vector<bytecs> CodePayload(uint16_t code) {
    return {static_cast<bytecs>((code >> 8) & 0xFF), static_cast<bytecs>(code & 0xFF)};
}

} // namespace

// ===========================================================================
// §7.6 — the close payload length
// ===========================================================================

TEST(ClientWebSocketPayloadValidationTests, AZeroBytePayloadIsStillAcceptedWithNoStatus) {
    ExpectCloseAccepted({}, -1, "an empty close payload is legal and carries no status");
}

TEST(ClientWebSocketPayloadValidationTests, AOneBytePayloadIsRejected) {
    // RFC 6455 §5.5.1. This used to be SILENTLY IGNORED and reported as a clean close.
    ExpectCloseRejected({0x03}, "a 1-byte close payload is a protocol error");
}

TEST(ClientWebSocketPayloadValidationTests, ATwoBytePayloadIsAccepted) {
    ExpectCloseAccepted(CodePayload(1000), 1000, "1000 NormalClosure");
}

// ===========================================================================
// §7.7 — the close code domain
// ===========================================================================

TEST(ClientWebSocketPayloadValidationTests, TheRfcDefinedCloseCodesAreAccepted) {
    for (uint16_t code : {1000, 1001, 1002, 1003, 1007, 1008, 1009, 1010, 1011}) {
        ExpectCloseAccepted(CodePayload(code), code, "an RFC 6455 §7.4.1 code");
    }
}

TEST(ClientWebSocketPayloadValidationTests, TheDelegatedRangesAreAccepted) {
    // RFC 6455 §7.4.2 delegates 3000-3999 to IANA registration and 4000-4999 to private use.
    // These are legal but have NO enumerator in WebSocketCloseStatus — in this port or in .NET,
    // whose enum names the same subset. That is inherent to the enum's design and is not what
    // this ticket repairs.
    for (uint16_t code : {3000, 3999, 4000, 4999}) {
        ExpectCloseAccepted(CodePayload(code), code, "a delegated-range code");
    }
    // 1012-1014 are registered under the procedure §7.4.2 delegates. Accepting them is THIS
    // PORT'S CHOICE: rejecting them would reject a conforming server, and the reference tree is
    // absent so .NET's own set cannot be consulted.
    for (uint16_t code : {1012, 1013, 1014}) {
        ExpectCloseAccepted(CodePayload(code), code, "an IANA-registered code");
    }
}

TEST(ClientWebSocketPayloadValidationTests, CodesThatCanNeverAppearOnTheWireAreRejected) {
    // Every one of these reached the public getCloseStatusProperty() before #2091.
    ExpectCloseRejected(CodePayload(0),    "0 is not a close code at all");
    ExpectCloseRejected(CodePayload(999),  "below the assigned range");
    ExpectCloseRejected(CodePayload(1004), "1004 is reserved");
    ExpectCloseRejected(CodePayload(1005), "1005 is local-only — and is this port's Empty enumerator");
    ExpectCloseRejected(CodePayload(1006), "1006 is local-only");
    ExpectCloseRejected(CodePayload(1015), "1015 is registered but must not be sent");
    ExpectCloseRejected(CodePayload(1016), "1016-2999 are reserved and undefined");
    ExpectCloseRejected(CodePayload(2999), "1016-2999 are reserved and undefined");
    ExpectCloseRejected(CodePayload(5000), "above the private-use range");
    ExpectCloseRejected(CodePayload(65535), "the largest 16-bit value");
}

TEST(ClientWebSocketPayloadValidationTests, ARejectedCloseCodeLeavesNoCloseStatusBehind) {
    // The parse throws before closeStatus_ or state_ is touched, so a malformed close frame
    // cannot leave the socket reporting a clean close with a bogus status.
    RunAgainstServer(
        "", noConfig,
        [&](Socket& server) { server.Send(CloseFrame(CodePayload(1006))); },
        [&](ClientWebSocket& client) {
            std::vector<bytecs> buffer(1024);
            EXPECT_THROW(client.ReceiveAsync(buffer, 0, 1024).Wait(), WebSocketException);
            EXPECT_FALSE(client.getCloseStatusProperty().has_value())
                << "a rejected close frame must not publish a status";
            EXPECT_NE(client.getStateProperty(), WebSocketState::Closed)
                << "a rejected close frame must not be reported as a clean close";
        });
}

// ===========================================================================
// §7.8 — UTF-8 in close reasons and Text messages
// ===========================================================================

TEST(ClientWebSocketPayloadValidationTests, AnInvalidUtf8CloseReasonIsRejected) {
    ExpectCloseRejected({0x03, 0xE8, 0xFF}, "0xFF is not a legal UTF-8 byte anywhere");
    ExpectCloseRejected({0x03, 0xE8, 0xED, 0xA0, 0x80}, "a lone surrogate encoding");
    ExpectCloseRejected({0x03, 0xE8, 0xC0, 0xAF}, "an overlong two-byte encoding of '/'");
    ExpectCloseRejected({0x03, 0xE8, 0xC3}, "a truncated two-byte sequence");
    ExpectCloseRejected({0x03, 0xE8, 0xF5, 0x80, 0x80, 0x80}, "a scalar above U+10FFFF");
}

TEST(ClientWebSocketPayloadValidationTests, AValidUtf8CloseReasonIsAccepted) {
    RunAgainstServer(
        "", noConfig,
        [&](Socket& server) {
            // 1000 + "ü" (C3 BC) + "中" (E4 B8 AD) + U+1F600 (F0 9F 98 80), a supplementary scalar.
            server.Send(CloseFrame({0x03, 0xE8, 0xC3, 0xBC, 0xE4, 0xB8, 0xAD, 0xF0, 0x9F, 0x98, 0x80}));
        },
        [&](ClientWebSocket& client) {
            std::vector<bytecs> buffer(1024);
            auto result = client.ReceiveAsync(buffer, 0, 1024).getResultProperty();
            EXPECT_EQ(result.getMessageTypeProperty(), WebSocketMessageType::Close);
            const auto description = client.getCloseStatusDescriptionProperty();
            ASSERT_TRUE(description.has_value());
            EXPECT_EQ(description->size(), 9u) << "three scalars, 2 + 3 + 4 bytes";
        });
}

TEST(ClientWebSocketPayloadValidationTests, AnInvalidUtf8TextMessageIsRejected) {
    RunAgainstServer(
        "", noConfig,
        [&](Socket& server) {
            std::vector<bytecs> f{0x81, 0x03, 0x61, 0xFF, 0x62}; // FIN|text, len 3, "a\xFFb"
            server.Send(f);
        },
        [&](ClientWebSocket& client) {
            std::vector<bytecs> buffer(1024);
            EXPECT_THROW(client.ReceiveAsync(buffer, 0, 1024).Wait(), WebSocketException);
        });
}

TEST(ClientWebSocketPayloadValidationTests, AnInvalidUtf8TextMessageIsNeverPartiallyPublished) {
    // The whole frame is validated before any byte is copied into the caller's buffer.
    RunAgainstServer(
        "", noConfig,
        [&](Socket& server) {
            std::vector<bytecs> f{0x81, 0x04, 0x61, 0x62, 0x63, 0xFF}; // "abc\xFF"
            server.Send(f);
        },
        [&](ClientWebSocket& client) {
            std::vector<bytecs> buffer(1024, static_cast<bytecs>(0xAA));
            EXPECT_THROW(client.ReceiveAsync(buffer, 0, 1024).Wait(), WebSocketException);
            EXPECT_EQ(buffer[0], static_cast<bytecs>(0xAA))
                << "no byte of a rejected message may reach the caller's buffer";
        });
}

TEST(ClientWebSocketPayloadValidationTests, AValidUtf8TextMessageIsStillAccepted) {
    RunAgainstServer(
        "", noConfig,
        [&](Socket& server) {
            std::vector<bytecs> f{0x81, 0x09, 0xC3, 0xBC, 0xE4, 0xB8, 0xAD, 0xF0, 0x9F, 0x98, 0x80};
            server.Send(f);
        },
        [&](ClientWebSocket& client) {
            std::vector<bytecs> buffer(1024);
            auto result = client.ReceiveAsync(buffer, 0, 1024).getResultProperty();
            EXPECT_EQ(result.getCountProperty(), 9);
            EXPECT_EQ(result.getMessageTypeProperty(), WebSocketMessageType::Text);
        });
}

TEST(ClientWebSocketPayloadValidationTests, ABinaryMessageIsNotUtf8Validated) {
    // RFC 6455 §8.1 constrains Text, not Binary. Arbitrary bytes must still round-trip.
    RunAgainstServer(
        "", noConfig,
        [&](Socket& server) {
            std::vector<bytecs> f{0x82, 0x03, 0xFF, 0xFE, 0xFD}; // FIN|binary
            server.Send(f);
        },
        [&](ClientWebSocket& client) {
            std::vector<bytecs> buffer(1024);
            auto result = client.ReceiveAsync(buffer, 0, 1024).getResultProperty();
            EXPECT_EQ(result.getCountProperty(), 3);
            EXPECT_EQ(result.getMessageTypeProperty(), WebSocketMessageType::Binary);
        });
}

TEST(ClientWebSocketPayloadValidationTests, AFragmentedTextMessageIsDeliberatelyNotValidated) {
    // PIN of a stated limit, not an assertion that this is correct. A scalar may legally
    // straddle a fragment boundary, so per-frame validation would reject conforming input;
    // validating properly needs incremental decoder state on the object, which is an
    // OBJECT-LAYOUT CHANGE this compatible ticket does not make (plan §10/§11, and the layout
    // static_assert in ClientWebSocketFrameValidationTests pins it). The two halves here are
    // each invalid alone but form one valid "ü" when joined.
    RunAgainstServer(
        "", noConfig,
        [&](Socket& server) {
            server.Send(std::vector<bytecs>{0x01, 0x01, 0xC3}); // text, FIN=0, first byte of "ü"
            server.Send(std::vector<bytecs>{0x80, 0x01, 0xBC}); // continuation, FIN=1
        },
        [&](ClientWebSocket& client) {
            std::vector<bytecs> buffer(1024);
            auto first = client.ReceiveAsync(buffer, 0, 1024).getResultProperty();
            EXPECT_EQ(first.getCountProperty(), 1);
            EXPECT_FALSE(first.getEndOfMessageProperty());
            auto second = client.ReceiveAsync(buffer, 0, 1024).getResultProperty();
            EXPECT_EQ(second.getCountProperty(), 1);
            EXPECT_TRUE(second.getEndOfMessageProperty());
        });
}

// ===========================================================================
// §7.10 — the negotiated subprotocol response.
//
// These cannot go through RunAgainstServer: it asserts a COMPLETED connect, and
// for two of the three cases the rejection IS the connect failing. They drive
// ConnectAsync directly and assert on the message.
// ===========================================================================

namespace {

/// Runs a full ConnectAsync against a server that answers with `respondProtocol`, and returns
/// the exception message (empty when the connect succeeded).
std::string ConnectWithProtocolResponse(const std::string& respondProtocol,
                                        const std::vector<std::string>& requested,
                                        std::optional<std::string>* outSubProtocol) {
    Socket listener(System::Net::Sockets::AddressFamily::InterNetwork,
                    System::Net::Sockets::SocketType::Stream,
                    System::Net::Sockets::ProtocolType::Tcp);
    listener.Bind(IPEndPoint(IPAddress::Loopback, 0));
    listener.Listen();
    auto local = std::dynamic_pointer_cast<IPEndPoint>(listener.getLocalEndPointProperty());
    EXPECT_NE(local, nullptr);
    const SharpRuntime::intcs port = local->getPortProperty();

    std::thread serverThread([&]() {
        try {
            auto server = listener.Accept();
            const std::string request = readHeaders(*server);
            const size_t keyPos = request.find("Sec-WebSocket-Key: ");
            const size_t keyStart = keyPos + std::string("Sec-WebSocket-Key: ").size();
            const std::string key = request.substr(keyStart, request.find("\r\n", keyStart) - keyStart);
            const auto digest = payloadTestSha1(key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
            const std::string accept =
                System::Convert::ToBase64String(std::vector<bytecs>(digest.begin(), digest.end()));
            std::string response = "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\n"
                                   "Connection: Upgrade\r\nSec-WebSocket-Accept: " + accept + "\r\n";
            if (!respondProtocol.empty()) response += "Sec-WebSocket-Protocol: " + respondProtocol + "\r\n";
            response += "\r\n";
            server->Send(toBytes(response));
            server->Close();
        } catch (...) {
        }
    });

    std::string message;
    ClientWebSocket client;
    for (const auto& p : requested) client.getOptionsProperty().AddSubProtocol(p);
    try {
        System::Uri uri("ws://127.0.0.1:" + std::to_string(port) + "/");
        client.ConnectAsync(uri).Wait();
        if (outSubProtocol) *outSubProtocol = client.getSubProtocolProperty();
    } catch (const System::Exception& e) {
        message = e.what();
    }
    client.Dispose();
    serverThread.join();
    listener.Close();
    return message;
}

} // namespace

TEST(ClientWebSocketPayloadValidationTests, ConnectRejectsASubprotocolThatWasNeverRequested) {
    std::optional<std::string> sub;
    const std::string message = ConnectWithProtocolResponse("evil", {}, &sub);
    EXPECT_NE(message.find("none was requested"), std::string::npos) << message;
    EXPECT_EQ(message.find("evil"), std::string::npos)
        << "the server-supplied value must not be echoed: " << message;
}

TEST(ClientWebSocketPayloadValidationTests, ConnectRejectsASubprotocolOtherThanTheRequestedOne) {
    std::optional<std::string> sub;
    const std::string message = ConnectWithProtocolResponse("evil", {"chat"}, &sub);
    EXPECT_NE(message.find("was not requested"), std::string::npos) << message;
    EXPECT_EQ(message.find("evil"), std::string::npos) << message;
}

TEST(ClientWebSocketPayloadValidationTests, ConnectAcceptsTheRequestedSubprotocol) {
    std::optional<std::string> sub;
    const std::string message = ConnectWithProtocolResponse("superchat", {"chat", "superchat"}, &sub);
    EXPECT_TRUE(message.empty()) << message;
    ASSERT_TRUE(sub.has_value());
    EXPECT_EQ(*sub, "superchat");
}

TEST(ClientWebSocketPayloadValidationTests, TheSubprotocolComparisonIsCaseInsensitive) {
    // AddSubProtocol's duplicate check is already OrdinalIgnoreCase, so the two ends of the same
    // list must agree about identity.
    std::optional<std::string> sub;
    const std::string message = ConnectWithProtocolResponse("CHAT", {"chat"}, &sub);
    EXPECT_TRUE(message.empty()) << message;
    ASSERT_TRUE(sub.has_value());
    EXPECT_EQ(*sub, "CHAT") << "the server's spelling is preserved";
}

TEST(ClientWebSocketPayloadValidationTests, NoSubprotocolResponseIsStillFineWhenSomeWereRequested) {
    // RFC 6455 §4.1: a server that supports none of the requested protocols simply omits the
    // header. That must remain a successful connect with no SubProtocol.
    std::optional<std::string> sub;
    const std::string message = ConnectWithProtocolResponse("", {"chat"}, &sub);
    EXPECT_TRUE(message.empty()) << message;
    EXPECT_FALSE(sub.has_value());
}

// ===========================================================================
// The SECOND close-parse site — CloseAsync's close-handshake loop, which had an
// identical unvalidated block
// ===========================================================================

TEST(ClientWebSocketPayloadValidationTests, TheCloseHandshakeLoopValidatesTheSamePayload) {
    // CloseAsync sends a close frame and then reads until it sees the peer's. That read used its
    // OWN copy of the unvalidated `>= 2` block, so before #2091 a malformed close frame was
    // accepted here even with the receive path repaired.
    for (const std::vector<bytecs>& bad : {std::vector<bytecs>{0x03},                 // 1 byte
                                           std::vector<bytecs>{0x03, 0xEE},           // code 1006
                                           std::vector<bytecs>{0x03, 0xE8, 0xFF}}) {  // bad UTF-8
        RunAgainstServer(
            "", noConfig,
            [&](Socket& server) {
                std::vector<bytecs> clientClose(64);
                (void)server.Receive(clientClose); // the client's own close frame
                server.Send(CloseFrame(bad));
            },
            [&](ClientWebSocket& client) {
                EXPECT_THROW(client.CloseAsync(WebSocketCloseStatus::NormalClosure, std::nullopt).Wait(),
                             WebSocketException);
            });
    }
}

TEST(ClientWebSocketPayloadValidationTests, TheCloseHandshakeLoopStillAcceptsAValidPayload) {
    RunAgainstServer(
        "", noConfig,
        [&](Socket& server) {
            std::vector<bytecs> clientClose(64);
            (void)server.Receive(clientClose);
            server.Send(CloseFrame({0x03, 0xE8, 0xC3, 0xBC})); // 1000 + "ü"
        },
        [&](ClientWebSocket& client) {
            EXPECT_NO_THROW(client.CloseAsync(WebSocketCloseStatus::NormalClosure, std::nullopt).Wait());
            EXPECT_EQ(client.getStateProperty(), WebSocketState::Closed);
            const auto status = client.getCloseStatusProperty();
            ASSERT_TRUE(status.has_value());
            EXPECT_EQ(*status, WebSocketCloseStatus::NormalClosure);
        });
}

// ===========================================================================
// Interaction with #2089 and #2090 — the invariants they established must hold
// ===========================================================================

TEST(ClientWebSocketPayloadValidationTests, TheHandshakeDoorsFrom2089StillReject) {
    // #2091 edited performHandshake; #2089's URI door must still be closed, and must still
    // reject before a socket exists.
    ClientWebSocket client;
    System::Uri uri("ws://127.0.0.1:9/a\r\nX-Injected: yes");
    EXPECT_THROW(client.ConnectAsync(uri).Wait(), System::ArgumentException);
    client.Dispose();
}

TEST(ClientWebSocketPayloadValidationTests, TheFrameHeaderChecksFrom2090StillReject) {
    // #2091 edited readFrame's caller, not readFrame; an oversized control frame must still be
    // rejected on the 7-bit length field, before the close payload parser is ever reached.
    const std::string payload(200, 'p');
    RunAgainstServer(
        "", noConfig,
        [&](Socket& server) {
            std::vector<bytecs> f{0x88, 0x7E, 0x00, 0xC8}; // close, 16-bit length 200
            f.insert(f.end(), payload.begin(), payload.end());
            server.Send(f);
        },
        [&](ClientWebSocket& client) {
            std::vector<bytecs> buffer(1024);
            EXPECT_THROW(client.ReceiveAsync(buffer, 0, 1024).Wait(), WebSocketException);
        });
}
