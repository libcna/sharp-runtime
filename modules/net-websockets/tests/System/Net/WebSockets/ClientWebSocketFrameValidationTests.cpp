// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Ticket #2090 — WebSocket frame-header validation.
//
// docs/SystemNetWebSocketsNamespaceReviewPlan.md corrected premise 6.5: THE FRAME PARSER IS
// ENTIRELY UNAUDITED. None of SR-AUD-247…252 names readFrame(), the code that touches remote
// bytes. Reading it produced five header-level defects (§7.1–§7.5), every one reachable by the
// server the client connected to:
//
//   * reserved bits RSV1/2/3 were never examined;
//   * the opcode was never validated, so reserved opcodes 0x3–0x7 and 0xB–0xF fell through
//     ReceiveAsync's switch into `default:` and were delivered as application data;
//   * a MASKED server frame was accepted and unmasked, although RFC 6455 §5.1 says a client
//     MUST fail the connection on one;
//   * a fragmented control frame (FIN=0 on 0x8/0x9/0xA) was accepted;
//   * a control payload far above the 125-byte limit was accepted — a 256 MiB "Ping" was read
//     into memory AND ECHOED BACK to the sender as a Pong.
//
// Every rule asserted here is RFC 6455 cited as a PROTOCOL fact, not as .NET behaviour: the
// reference tree is absent and these five are decidable from the wire format alone. The
// exception identity (WebSocketException) is this port's recorded choice.
//
// Every case runs over a REAL loopback socket against a mock server thread, and every
// synchronisation point is a blocking accept/read — there are no sleeps.
#include <gtest/gtest.h>

#include <array>
#include <functional>
#include <mutex>
#include <optional>
#include <type_traits>
#include <memory>
#include <cstring>
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

std::array<uint8_t, 20> frameTestSha1(const std::string& input) {
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
            if (i < 20)      { f = (b & c) | ((~b) & d);            k = 0x5A827999; }
            else if (i < 40) { f = b ^ c ^ d;                        k = 0x6ED9EBA1; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d);      k = 0x8F1BBCDC; }
            else             { f = b ^ c ^ d;                        k = 0xCA62C1D6; }
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

/// Completes a correct RFC 6455 upgrade, then hands the connected socket to `afterHandshake`,
/// which writes whatever raw bytes the case needs. Returns the exception (if any) that the
/// client's first ReceiveAsync produced.
///
/// Synchronisation is entirely by blocking accept/read on both sides — no sleeps.
template <typename ServerBody>
void RunAgainstServer(ServerBody&& afterHandshake,
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
        auto server = listener.Accept();
        const std::string request = readHeaders(*server);
        const size_t keyPos = request.find("Sec-WebSocket-Key: ");
        ASSERT_NE(keyPos, std::string::npos);
        const size_t keyStart = keyPos + std::string("Sec-WebSocket-Key: ").size();
        const std::string key = request.substr(keyStart, request.find("\r\n", keyStart) - keyStart);
        const auto digest = frameTestSha1(key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
        const std::string accept =
            System::Convert::ToBase64String(std::vector<bytecs>(digest.begin(), digest.end()));
        server->Send(toBytes("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\n"
                             "Connection: Upgrade\r\nSec-WebSocket-Accept: " + accept + "\r\n\r\n"));
        afterHandshake(*server);
        server->Close();
    });

    ClientWebSocket client;
    System::Uri uri("ws://127.0.0.1:" + std::to_string(port) + "/");
    client.ConnectAsync(uri).Wait();
    ASSERT_EQ(client.getStateProperty(), WebSocketState::Open);
    clientBody(client);
    client.Dispose();
    serverThread.join();
    listener.Close();
}

/// Sends `raw` from the server and asserts the client's ReceiveAsync throws WebSocketException.
void ExpectFrameRejected(const std::vector<bytecs>& raw, const char* what) {
    RunAgainstServer(
        [&](Socket& server) { server.Send(raw); },
        [&](ClientWebSocket& client) {
            std::vector<bytecs> buffer(1024);
            EXPECT_THROW(client.ReceiveAsync(buffer, 0, 1024).Wait(), WebSocketException) << what;
        });
}

std::vector<bytecs> Frame(bytecs firstByte, bytecs secondByte, const std::string& payload = "") {
    std::vector<bytecs> f{firstByte, secondByte};
    f.insert(f.end(), payload.begin(), payload.end());
    return f;
}

} // namespace

// ===========================================================================
// §7.1 — reserved bits
// ===========================================================================

TEST(ClientWebSocketFrameValidationTests, EachReservedBitAloneIsRejected) {
    ExpectFrameRejected(Frame(0x80 | 0x40 | 0x1, 0x02, "hi"), "RSV1");
    ExpectFrameRejected(Frame(0x80 | 0x20 | 0x1, 0x02, "hi"), "RSV2");
    ExpectFrameRejected(Frame(0x80 | 0x10 | 0x1, 0x02, "hi"), "RSV3");
}

TEST(ClientWebSocketFrameValidationTests, AllThreeReservedBitsTogetherAreRejected) {
    ExpectFrameRejected(Frame(0x80 | 0x70 | 0x1, 0x02, "hi"), "RSV1|RSV2|RSV3");
}

// ===========================================================================
// §7.2 — the opcode domain
// ===========================================================================

TEST(ClientWebSocketFrameValidationTests, EveryReservedOpcodeIsRejected) {
    // Before #2090 these fell through ReceiveAsync's `default:` and were handed to the caller
    // as ordinary message data.
    for (bytecs opcode : {bytecs{0x3}, bytecs{0x4}, bytecs{0x5}, bytecs{0x6}, bytecs{0x7},
                          bytecs{0xB}, bytecs{0xC}, bytecs{0xD}, bytecs{0xE}, bytecs{0xF}}) {
        ExpectFrameRejected(Frame(static_cast<bytecs>(0x80 | opcode), 0x02, "hi"),
                            "reserved opcode");
    }
}

// ===========================================================================
// §7.3 — a masked server frame
// ===========================================================================

TEST(ClientWebSocketFrameValidationTests, AMaskedServerFrameIsRejected) {
    // RFC 6455 §5.1. The port used to honour the mask bit and unmask the payload.
    std::vector<bytecs> f{0x81, 0x82, 0x01, 0x02, 0x03, 0x04}; // FIN|text, MASK|len 2, key
    f.push_back(static_cast<bytecs>('h' ^ 0x01));
    f.push_back(static_cast<bytecs>('i' ^ 0x02));
    ExpectFrameRejected(f, "masked server frame");
}

// ===========================================================================
// §7.4 — fragmented control frames
// ===========================================================================

TEST(ClientWebSocketFrameValidationTests, AFragmentedControlFrameIsRejected) {
    ExpectFrameRejected(Frame(0x08, 0x00), "fragmented close");           // FIN=0, close
    ExpectFrameRejected(Frame(0x09, 0x02, "hi"), "fragmented ping");      // FIN=0, ping
    ExpectFrameRejected(Frame(0x0A, 0x02, "hi"), "fragmented pong");      // FIN=0, pong
}

// ===========================================================================
// §7.5 — oversized control payloads
// ===========================================================================

TEST(ClientWebSocketFrameValidationTests, AControlPayloadAbove125BytesIsRejected) {
    // The 7-bit length field is checked BEFORE the extended-length bytes are read, so the
    // rejection costs no allocation. len==126 signals a 16-bit extended length, which for a
    // control frame is by definition over the limit; len==127 signals 64-bit, likewise.
    //
    // These frames are deliberately COMPLETE -- full extended length and full payload. An
    // earlier version of this test sent only the two header bytes, and it passed with the
    // check REMOVED: the parser simply read past the end, hit EOF, and threw
    // ConnectionClosedPrematurely, which is also a WebSocketException. A truncated frame
    // cannot distinguish "rejected for being an oversized control frame" from "rejected for
    // ending early", so it proves nothing about this guard.
    const std::string payload(200, 'p');
    {
        std::vector<bytecs> f{0x89, 0x7E, 0x00, 0xC8}; // ping, 16-bit length = 200
        f.insert(f.end(), payload.begin(), payload.end());
        ExpectFrameRejected(f, "ping with a complete 200-byte payload");
    }
    {
        std::vector<bytecs> f{0x88, 0x7E, 0x00, 0xC8}; // close, 16-bit length = 200
        f.insert(f.end(), payload.begin(), payload.end());
        ExpectFrameRejected(f, "close with a complete 200-byte payload");
    }
    {
        std::vector<bytecs> f{0x8A, 0x7F, 0, 0, 0, 0, 0, 0, 0x00, 0xC8}; // pong, 64-bit = 200
        f.insert(f.end(), payload.begin(), payload.end());
        ExpectFrameRejected(f, "pong with a complete 64-bit-length 200-byte payload");
    }
}

TEST(ClientWebSocketFrameValidationTests, AControlPayloadOfExactly125BytesIsAccepted) {
    // The boundary must stay open: 125 is legal and a Ping must still be answered with a Pong.
    const std::string payload(125, 'p');
    RunAgainstServer(
        [&](Socket& server) {
            server.Send(Frame(0x89, 0x7D, payload)); // ping, 125 bytes
            // The client answers with a masked Pong; read it so the exchange is deterministic.
            std::vector<bytecs> header(2);
            server.Receive(header);
            EXPECT_EQ(header[0] & 0x0F, 0x0A) << "the ping must be answered with a pong";
            std::vector<bytecs> rest(4 + 125);
            size_t total = 0;
            while (total < rest.size()) {
                std::vector<bytecs> chunk(rest.size() - total);
                const auto n = server.Receive(chunk);
                if (n == 0) break;
                total += static_cast<size_t>(n);
            }
            server.Send(Frame(0x81, 0x02, "ok")); // then a real message
        },
        [&](ClientWebSocket& client) {
            std::vector<bytecs> buffer(1024);
            auto result = client.ReceiveAsync(buffer, 0, 1024).getResultProperty();
            EXPECT_EQ(result.getCountProperty(), 2);
            EXPECT_EQ(std::string(buffer.begin(), buffer.begin() + 2), "ok");
        });
}

// ===========================================================================
// What must NOT be rejected
// ===========================================================================

TEST(ClientWebSocketFrameValidationTests, OrdinaryDataFramesStillRoundTrip) {
    for (bytecs opcode : {bytecs{0x1}, bytecs{0x2}}) { // text, binary
        RunAgainstServer(
            [&](Socket& server) { server.Send(Frame(static_cast<bytecs>(0x80 | opcode), 0x05, "hello")); },
            [&](ClientWebSocket& client) {
                std::vector<bytecs> buffer(1024);
                auto result = client.ReceiveAsync(buffer, 0, 1024).getResultProperty();
                EXPECT_EQ(result.getCountProperty(), 5);
                EXPECT_TRUE(result.getEndOfMessageProperty());
                EXPECT_EQ(std::string(buffer.begin(), buffer.begin() + 5), "hello");
            });
    }
}

TEST(ClientWebSocketFrameValidationTests, AFragmentedDataMessageIsStillAccepted) {
    // FIN=0 is legal on a DATA frame — only control frames may not be fragmented.
    RunAgainstServer(
        [&](Socket& server) {
            server.Send(Frame(0x01, 0x03, "abc")); // text, FIN=0
            server.Send(Frame(0x80, 0x03, "def")); // continuation, FIN=1
        },
        [&](ClientWebSocket& client) {
            std::vector<bytecs> buffer(1024);
            auto first = client.ReceiveAsync(buffer, 0, 1024).getResultProperty();
            EXPECT_EQ(first.getCountProperty(), 3);
            EXPECT_FALSE(first.getEndOfMessageProperty());
            auto second = client.ReceiveAsync(buffer, 0, 1024).getResultProperty();
            EXPECT_EQ(second.getCountProperty(), 3);
            EXPECT_TRUE(second.getEndOfMessageProperty());
        });
}

TEST(ClientWebSocketFrameValidationTests, ExtendedPayloadLengthsStillWork) {
    // 126 selects the 16-bit length field. 300 bytes exercises it on a DATA frame, where it is
    // legal — the control-frame check must not have leaked onto data frames.
    const std::string payload(300, 'x');
    RunAgainstServer(
        [&](Socket& server) {
            std::vector<bytecs> f{0x81, 0x7E, static_cast<bytecs>((300 >> 8) & 0xFF),
                                  static_cast<bytecs>(300 & 0xFF)};
            f.insert(f.end(), payload.begin(), payload.end());
            server.Send(f);
        },
        [&](ClientWebSocket& client) {
            std::vector<bytecs> buffer(1024);
            auto result = client.ReceiveAsync(buffer, 0, 1024).getResultProperty();
            EXPECT_EQ(result.getCountProperty(), 300);
        });
}

TEST(ClientWebSocketFrameValidationTests, AZeroLengthDataFrameIsStillAccepted) {
    RunAgainstServer(
        [&](Socket& server) { server.Send(Frame(0x81, 0x00)); },
        [&](ClientWebSocket& client) {
            std::vector<bytecs> buffer(1024);
            auto result = client.ReceiveAsync(buffer, 0, 1024).getResultProperty();
            EXPECT_EQ(result.getCountProperty(), 0);
            EXPECT_TRUE(result.getEndOfMessageProperty());
        });
}

// ===========================================================================
// Truncation and EOF — the frame header must never be read past what arrived
// ===========================================================================

TEST(ClientWebSocketFrameValidationTests, EofAtEveryHeaderByteOffsetThrows) {
    // A server that closes mid-header must produce a clean WebSocketException, never a hang or
    // a read of uninitialised bytes. Offset 0 is "no bytes at all"; 1 is a half-read 2-byte
    // header; 2 and 3 truncate a 16-bit extended length; 4..9 truncate a 64-bit one.
    const std::vector<bytecs> full16{0x81, 0x7E, 0x01, 0x2C};                        // len 300
    const std::vector<bytecs> full64{0x81, 0x7F, 0, 0, 0, 0, 0, 0, 0x01, 0x2C};      // len 300
    for (size_t prefix = 0; prefix < full16.size(); ++prefix) {
        ExpectFrameRejected(std::vector<bytecs>(full16.begin(), full16.begin() + prefix),
                            "truncated 16-bit length header");
    }
    for (size_t prefix = 2; prefix < full64.size(); ++prefix) {
        ExpectFrameRejected(std::vector<bytecs>(full64.begin(), full64.begin() + prefix),
                            "truncated 64-bit length header");
    }
}

TEST(ClientWebSocketFrameValidationTests, ATruncatedPayloadThrows) {
    ExpectFrameRejected(Frame(0x81, 0x05, "abc"), "declared 5 bytes, sent 3");
}

// ===========================================================================
// PINS — plan §11 (layout, by the first implementation ticket) and #2088's
// ownership-model pin, which is a compile-time assertion because a
// use-after-free is not a behaviour you can pin behaviourally.
// ===========================================================================

namespace {

/// ClientWebSocket's declared members, in declaration order, behind the same polymorphic base.
/// A probe struct rather than a literal byte count: these members include std::string,
/// std::vector, std::optional and std::mutex, whose sizes differ between libstdc++ and libc++,
/// and the platform policy requires the MinGW/Emscripten/Apple-Clang builds to keep compiling.
struct ClientWebSocketLayoutProbe {
    virtual ~ClientWebSocketLayoutProbe() = default; // ClientWebSocket : WebSocket (polymorphic)
    ClientWebSocketOptions                  options;
    std::unique_ptr<int>                    socket;
    WebSocketState                          state;
    bool                                    connectStarted;
    std::optional<std::string>              subProtocol;
    std::optional<WebSocketCloseStatus>     closeStatus;
    std::optional<std::string>              closeStatusDescription;
    bool                                    sendContinuation;
    WebSocketMessageType                    fragmentType;
    std::vector<bytecs>                     recvLeftover;
    size_t                                  recvLeftoverPos;
    WebSocketMessageType                    recvLeftoverType;
    std::mutex                              sendMutex;
    // #2088 added this one, and only this one: the liveness boundary's shared state. Declared
    // last, matching the class, so the probe stays a field-for-field shadow rather than a
    // number to re-guess.
    std::shared_ptr<void>                   asyncOps;
};

} // namespace

static_assert(sizeof(ClientWebSocket) == sizeof(ClientWebSocketLayoutProbe),
              "ClientWebSocket's object layout moved. The probe above is a field-for-field "
              "shadow; #2088 added exactly one member to it (the liveness boundary's shared "
              "state) under docs/StandingApprovals.md SA-3. Any further data member here is a "
              "new object-layout change and needs its own approval. See "
              "docs/SystemNetWebSocketsNamespaceReviewPlan.md section 11.");
static_assert(alignof(ClientWebSocket) == alignof(ClientWebSocketLayoutProbe),
              "ClientWebSocket alignment moved");

// #2088 / SR-AUD-247 / CCF-019. The finding is that every async member captures a raw `this`
// with no owner liveness, and that SendAsync/ReceiveAsync additionally capture the caller's
// buffer by reference. CCF-019's family design is UNSELECTED (#2066 records two competing
// options and no decision across six sites), so this review deliberately did not pick one.
// What it CAN do is pin the ownership model, exactly as #2066's pin does: the day someone
// changes ClientWebSocket to be shared-ownership-aware, this stops compiling and the change is
// visible instead of silent.
static_assert(!std::is_base_of_v<std::enable_shared_from_this<ClientWebSocket>, ClientWebSocket>,
              "ClientWebSocket's ownership model changed. That is the CCF-019 repair shape and "
              "it is BLOCKED pending #2066's family design selection -- see #2088.");

TEST(ClientWebSocketFrameValidationTests, LayoutAndOwnershipModelArePinned) {
    // The static_asserts above are the real guard; this makes the pin visible in the suite.
    EXPECT_EQ(sizeof(ClientWebSocket), sizeof(ClientWebSocketLayoutProbe));
    EXPECT_FALSE((std::is_base_of_v<std::enable_shared_from_this<ClientWebSocket>, ClientWebSocket>));
}

TEST(ClientWebSocketFrameValidationTests, TheExistingFrameAndHandshakeCapsArePinned) {
    // Two positives the review recorded so they are not lost (plan §6.6): a 256 MiB frame cap
    // and a 16 KiB handshake-response cap. The frame cap is exercised by
    // ClientWebSocketTests.ReceiveAsync_HugeFrameLength_ThrowsWebSocketException; this pins the
    // handshake cap, which nothing covered.
    Socket listener(System::Net::Sockets::AddressFamily::InterNetwork,
                    System::Net::Sockets::SocketType::Stream,
                    System::Net::Sockets::ProtocolType::Tcp);
    listener.Bind(IPEndPoint(IPAddress::Loopback, 0));
    listener.Listen();
    auto local = std::dynamic_pointer_cast<IPEndPoint>(listener.getLocalEndPointProperty());
    ASSERT_NE(local, nullptr);
    const SharpRuntime::intcs port = local->getPortProperty();

    std::thread serverThread([&]() {
        auto server = listener.Accept();
        (void)readHeaders(*server);
        // A response that never terminates its headers, well past the 16 KiB cap.
        server->Send(toBytes("HTTP/1.1 101 Switching Protocols\r\n"));
        for (int i = 0; i < 400 && server->getConnectedProperty(); ++i) {
            try {
                server->Send(toBytes("X-Filler: " + std::string(60, 'f') + "\r\n"));
            } catch (...) {
                break; // the client hung up once the cap tripped -- that is the pass condition
            }
        }
        server->Close();
    });

    ClientWebSocket client;
    System::Uri uri("ws://127.0.0.1:" + std::to_string(port) + "/");
    EXPECT_THROW(client.ConnectAsync(uri).Wait(), WebSocketException);
    client.Dispose();
    serverThread.join();
    listener.Close();
}
