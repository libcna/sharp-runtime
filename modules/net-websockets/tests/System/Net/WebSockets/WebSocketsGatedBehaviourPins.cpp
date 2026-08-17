// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// `System::Net::WebSockets` gated-behaviour pins.
//
// docs/SystemNetWebSocketsNamespaceReviewPlan.md §19 requires that SR-AUD-247, SR-AUD-250,
// SR-AUD-251 and SR-AUD-252 each carry a blocked ticket **and a pin**, and §13 requires #2095's
// measured fragmentation behaviour to be pinned. #2090 landed the first three pins (the layout
// probe, #2088's CCF-019 ownership `static_assert`, and the 16 KiB handshake cap). This file
// completes the set.
//
// **These tests assert what the code does TODAY, not what it should do.** Every one of them
// documents a defect whose repair is BLOCKED. They exist so that an approved repair cannot land
// silently: when one of these tickets is unblocked and implemented, the corresponding pin
// FAILS, and that failure is the signal to update this file in the same change.
//
// Nothing here is an endorsement, and none of these findings is remediated by this file
// existing.
#include <gtest/gtest.h>
#include <optional>

#include <algorithm>
#include <array>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "System/Convert.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/OperationCanceledException.hpp"
#include "System/Net/IPEndPoint.hpp"
#include "System/Net/Sockets/Socket.hpp"
#include "System/Net/WebSockets/ClientWebSocket.hpp"
#include "System/Net/WebSockets/WebSocketException.hpp"
#include "System/Threading/CancellationTokenSource.hpp"
#include "System/TimeSpan.hpp"
#include "System/Uri.hpp"

using namespace System::Net::WebSockets;
using System::Net::IPAddress;
using System::Net::IPEndPoint;
using System::Net::Sockets::Socket;
using SharpRuntime::bytecs;

namespace {

std::array<uint8_t, 20> pinSha1(const std::string& input) {
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

/// #2093 exposed a defect in this harness: if `clientBody` throws -- which it now legitimately
/// does, because a cancelled operation faults -- `serverThread.join()` was skipped and
/// ~std::thread on a joinable thread called std::terminate. The whole suite died with
/// "terminate called without an active exception" and no failing test name. The join is RAII
/// now, so a throwing body reports itself instead of taking the process down.
struct ServerJoiner {
    std::thread thread;
    Socket*     listener;
    ~ServerJoiner() {
        try {
            listener->Close();
        } catch (...) {
        }
        if (thread.joinable()) thread.join();
    }
};

/// Reads one whole client frame, unmasking its payload. A client always masks (RFC 6455 §5.1),
/// so a server that wants to see WHICH control frames arrive has to undo it. Returns false at
/// end of stream.
bool readClientFrame(Socket& socket, SharpRuntime::bytecs& opcode, std::vector<bytecs>& payload) {
    auto readExactly = [&socket](std::vector<bytecs>& into, size_t n) {
        into.assign(n, 0);
        size_t total = 0;
        while (total < n) {
            std::vector<bytecs> chunk(n - total);
            const auto got = socket.Receive(chunk);
            if (got <= 0) return false;
            std::copy(chunk.begin(), chunk.begin() + got, into.begin() + static_cast<long>(total));
            total += static_cast<size_t>(got);
        }
        return true;
    };

    std::vector<bytecs> header;
    if (!readExactly(header, 2)) return false;
    opcode = static_cast<SharpRuntime::bytecs>(header[0] & 0x0F);
    const bool masked = (header[1] & 0x80) != 0;
    size_t len = static_cast<size_t>(header[1] & 0x7F);
    if (len == 126) {
        std::vector<bytecs> ext;
        if (!readExactly(ext, 2)) return false;
        len = (static_cast<size_t>(ext[0]) << 8) | static_cast<size_t>(ext[1]);
    }
    std::vector<bytecs> mask;
    if (masked && !readExactly(mask, 4)) return false;
    if (!readExactly(payload, len)) return false;
    if (masked) {
        for (size_t i = 0; i < payload.size(); ++i) {
            payload[i] = static_cast<bytecs>(payload[i] ^ mask[i % 4]);
        }
    }
    return true;
}

/// Sends an unmasked server frame (a server must NOT mask -- RFC 6455 §5.1).
void sendServerFrame(Socket& socket, SharpRuntime::bytecs opcode, const std::vector<bytecs>& payload) {
    std::vector<bytecs> frame;
    frame.push_back(static_cast<bytecs>(0x80 | (opcode & 0x0F)));
    frame.push_back(static_cast<bytecs>(payload.size()));
    frame.insert(frame.end(), payload.begin(), payload.end());
    socket.Send(frame);
}

void RunAgainstServer(const std::function<void(ClientWebSocketOptions&)>& configure,
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
            const auto digest = pinSha1(key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
            const std::string accept =
                System::Convert::ToBase64String(std::vector<bytecs>(digest.begin(), digest.end()));
            server->Send(toBytes("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\n"
                                 "Connection: Upgrade\r\nSec-WebSocket-Accept: " + accept + "\r\n\r\n"));
            afterHandshake(*server);
            server->Close();
        } catch (...) {
        }
    });

    ServerJoiner joiner{std::move(serverThread), &listener};

    ClientWebSocket client;
    configure(client.getOptionsProperty());
    System::Uri uri("ws://127.0.0.1:" + std::to_string(port) + "/");
    client.ConnectAsync(uri).Wait();
    ASSERT_EQ(client.getStateProperty(), WebSocketState::Open);
    clientBody(client);
    client.Dispose();
}

void noConfig(ClientWebSocketOptions&) {}

} // namespace

// ===========================================================================
// SR-AUD-250 → #2092, BLOCKED: the inner exception is discarded
// ===========================================================================

TEST(WebSocketsGatedBehaviourPins, Pin2092_WebSocketExceptionDiscardsItsInnerException) {
    // The three-argument constructor contains a literal `(void)innerException;`. The comment
    // explaining it is ACCURATE, which is exactly what blocks the repair: Win32Exception has no
    // inner-exception-carrying constructor to forward to, so fixing this is a public
    // base-class change in modules/component-model or modules/core — another component.
    //
    // WHEN #2092 IS APPROVED AND IMPLEMENTED, THIS PIN MUST FAIL.
    std::exception_ptr inner;
    try {
        throw System::InvalidOperationException("the causal exception");
    } catch (...) {
        inner = std::current_exception();
    }
    ASSERT_TRUE(static_cast<bool>(inner));

    const WebSocketException ex(WebSocketError::Faulted, "outer message", inner);
    EXPECT_EQ(std::string(ex.what()).find("outer message") != std::string::npos, true);
    EXPECT_FALSE(static_cast<bool>(ex.getInnerExceptionProperty()))
        << "SR-AUD-250: the inner exception is currently DISCARDED. If this now holds the "
           "causal exception, #2092 landed and this pin must be updated in that change.";
}

// ===========================================================================
// SR-AUD-251 → #2093, BLOCKED: every CancellationToken is ignored
// ===========================================================================

TEST(WebSocketsGatedBehaviourPins, Fix2093_AnAlreadyCancelledTokenPreventsTheOperation) {
    // #2093 LANDED. This pin used to assert that the token was IGNORED; it now asserts the
    // contract, and the contract is .NET's: an already-cancelled token means the operation never
    // runs, and a cancelled operation reports OperationCanceledException.
    //
    // The design the ticket was blocked on turned out not to be transport-level at all. .NET
    // registers Abort() on the token -- `cancellationToken.Register(static s =>
    // ((ManagedWebSocket)s!).Abort(), this)`, ManagedWebSocket.cs:608,789 -- so cancelling ANY
    // WebSocket operation aborts the whole WebSocket. No socket timeout, no poll loop, no change
    // to modules/net-sockets.
    System::Threading::CancellationTokenSource cts;
    cts.Cancel();

    RunAgainstServer(
        noConfig,
        [&](Socket& server) { server.Send(std::vector<bytecs>{0x81, 0x02, 'o', 'k'}); },
        [&](ClientWebSocket& client) {
            std::vector<bytecs> buffer(1024);
            EXPECT_THROW((void)client.ReceiveAsync(buffer, 0, 1024, cts.getTokenProperty())
                             .getResultProperty(),
                         System::OperationCanceledException);
        });
}

TEST(WebSocketsGatedBehaviourPins, Fix2093_ANonCancelledTokenChangesNothing) {
    // Invariance, and the row that matters most: wiring cancellation in must not disturb the
    // ordinary path. A live-but-unfired token behaves exactly as the default None token did.
    System::Threading::CancellationTokenSource cts;

    RunAgainstServer(
        noConfig,
        [&](Socket& server) { server.Send(std::vector<bytecs>{0x81, 0x02, 'o', 'k'}); },
        [&](ClientWebSocket& client) {
            std::vector<bytecs> buffer(1024);
            auto result = client.ReceiveAsync(buffer, 0, 1024, cts.getTokenProperty()).getResultProperty();
            EXPECT_EQ(result.getCountProperty(), 2);
            EXPECT_EQ(client.getStateProperty(), WebSocketState::Open)
                << "a live but unfired token must not abort anything";
        });
}

TEST(WebSocketsGatedBehaviourPins, Fix2093_CancellingMidOperationAbortsTheWebSocket) {
    // The half a pre-cancelled token cannot show: a token that fires WHILE a receive is parked in
    // the blocking read. .NET's contract is that this aborts the WebSocket, and the abort is also
    // what unblocks this port -- Abort() shuts the socket down under #2096's shared ownership.
    //
    // Without the repair this hangs, because the server deliberately never sends a frame.
    System::Threading::CancellationTokenSource cts;

    RunAgainstServer(
        noConfig,
        [&](Socket& server) {
            std::this_thread::sleep_for(std::chrono::milliseconds(400));
            (void)server;
        },
        [&](ClientWebSocket& client) {
            std::vector<bytecs> buffer(1024);
            auto task = client.ReceiveAsync(buffer, 0, 1024, cts.getTokenProperty());
            std::thread canceller([&] {
                std::this_thread::sleep_for(std::chrono::milliseconds(60));
                cts.Cancel();
            });
            EXPECT_THROW((void)task.getResultProperty(), System::OperationCanceledException);
            canceller.join();
            EXPECT_EQ(client.getStateProperty(), WebSocketState::Aborted)
                << "cancellation aborts the whole WebSocket, which is .NET's documented contract";
        });
}

TEST(WebSocketsGatedBehaviourPins, Fix2093_EveryOneOfTheFiveMembersHonoursTheToken) {
    // SR-AUD-251 names all five. A repair that reached only the one with a test would be exactly
    // the "one-off partial fix for just this one" the old CloseAsync comment warned against.
    System::Threading::CancellationTokenSource cts;
    cts.Cancel();
    const auto token = cts.getTokenProperty();

    RunAgainstServer(
        noConfig,
        [&](Socket& server) { (void)server; },
        [&](ClientWebSocket& client) {
            std::vector<bytecs> buffer(64, 0);
            EXPECT_THROW(client.SendAsync(buffer, 0, 4, WebSocketMessageType::Binary, true, token).Wait(),
                         System::OperationCanceledException);
            EXPECT_THROW((void)client.ReceiveAsync(buffer, 0, 4, token).getResultProperty(),
                         System::OperationCanceledException);
            EXPECT_THROW(client.CloseOutputAsync(WebSocketCloseStatus::NormalClosure, std::nullopt, token).Wait(),
                         System::OperationCanceledException);
            EXPECT_THROW(client.CloseAsync(WebSocketCloseStatus::NormalClosure, std::nullopt, token).Wait(),
                         System::OperationCanceledException);
        });

    // ConnectAsync is the fifth, and the one the ticket's own example names: a pre-cancelled
    // connect must report cancellation rather than the PlatformNotSupportedException a `wss` URI
    // would otherwise produce.
    ClientWebSocket fresh;
    EXPECT_THROW(fresh.ConnectAsync(System::Uri("wss://example.invalid/"), token).Wait(),
                 System::OperationCanceledException);
}

// ===========================================================================
// SR-AUD-252 → #2094, BLOCKED: KeepAliveInterval/Timeout are inert
// ===========================================================================

TEST(WebSocketsGatedBehaviourPins, Fix2094_TheDefaultStrategyIsAnUnsolicitedPong) {
    // #2094 LANDED. This pin used to assert that NO PING IS EVER SENT; it now asserts which
    // frame is sent, and the answer is a Pong, not a Ping.
    //
    // .NET picks between two strategies by whether KeepAliveTimeout is positive
    // (ManagedWebSocket.cs:169-198). The DEFAULT timeout is Timeout.InfiniteTimeSpan
    // (WebSocketDefaults.cs:17), which this port already matched, so the default strategy is
    // UNSOLICITED PONG: send an empty Pong every interval and expect nothing back. It therefore
    // cannot fault, which is what makes it safe as a default.
    std::vector<SharpRuntime::bytecs> opcodes;
    RunAgainstServer(
        [](ClientWebSocketOptions& o) {
            o.setKeepAliveIntervalProperty(System::TimeSpan::FromMilliseconds(40));
        },
        [&](Socket& server) {
            SharpRuntime::bytecs opcode = 0;
            std::vector<bytecs> payload;
            for (int i = 0; i < 3 && readClientFrame(server, opcode, payload); ++i) {
                opcodes.push_back(opcode);
            }
        },
        [&](ClientWebSocket& client) {
            (void)client;
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        });

    ASSERT_FALSE(opcodes.empty()) << "no keep-alive frame arrived at all";
    for (auto opcode : opcodes) {
        EXPECT_EQ(opcode, 0xA) << "the default strategy sends Pong (0xA), never Ping (0x9)";
    }
}

TEST(WebSocketsGatedBehaviourPins, Fix2094_SettingATimeoutSwitchesToPingPong) {
    // With a positive KeepAliveTimeout, .NET creates a KeepAlivePingState and sends PINGs
    // carrying an 8-byte big-endian counter (ManagedWebSocket.KeepAlive.cs:108-123). The payload
    // matters: only a Pong echoing it clears the outstanding Ping.
    std::vector<SharpRuntime::bytecs> opcodes;
    std::vector<std::vector<bytecs>>  payloads;
    RunAgainstServer(
        [](ClientWebSocketOptions& o) {
            o.setKeepAliveIntervalProperty(System::TimeSpan::FromMilliseconds(40));
            o.setKeepAliveTimeoutProperty(System::TimeSpan::FromSeconds(30));
        },
        [&](Socket& server) {
            SharpRuntime::bytecs opcode = 0;
            std::vector<bytecs> payload;
            for (int i = 0; i < 2 && readClientFrame(server, opcode, payload); ++i) {
                opcodes.push_back(opcode);
                payloads.push_back(payload);
                // Echo it back so the ping never times out during this test.
                sendServerFrame(server, 0xA, payload);
            }
        },
        [&](ClientWebSocket& client) {
            (void)client;
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        });

    ASSERT_FALSE(opcodes.empty()) << "no keep-alive frame arrived at all";
    EXPECT_EQ(opcodes.front(), 0x9) << "a positive timeout selects the Ping/Pong strategy";
    ASSERT_FALSE(payloads.empty());
    EXPECT_EQ(payloads.front().size(), 8u) << "the ping payload is an 8-byte counter";
}

TEST(WebSocketsGatedBehaviourPins, Fix2094_APingThatIsNeverAnsweredFaultsTheConnection) {
    // The half that makes KeepAliveTimeout mean anything: a server that reads the Ping and never
    // answers must lose the connection, and the caller must be told WHY -- not merely that the
    // socket became unusable. .NET surfaces the same cause (ManagedWebSocket.cs:1016-1021).
    RunAgainstServer(
        [](ClientWebSocketOptions& o) {
            o.setKeepAliveIntervalProperty(System::TimeSpan::FromMilliseconds(40));
            o.setKeepAliveTimeoutProperty(System::TimeSpan::FromMilliseconds(80));
        },
        [&](Socket& server) {
            // Read whatever arrives and answer nothing at all.
            SharpRuntime::bytecs opcode = 0;
            std::vector<bytecs> payload;
            while (readClientFrame(server, opcode, payload)) {
            }
        },
        [&](ClientWebSocket& client) {
            std::vector<bytecs> buffer(64);
            // The receive parks; the heartbeat times out and aborts underneath it.
            EXPECT_THROW((void)client.ReceiveAsync(buffer, 0, 64).getResultProperty(),
                         WebSocketException);
            EXPECT_EQ(client.getStateProperty(), WebSocketState::Aborted);
        });
}

TEST(WebSocketsGatedBehaviourPins, Fix2094_AnAnsweredPingKeepsTheConnectionAliveWhileReceiving) {
    // The invariance row, and the one that proves the payload comparison is real rather than
    // "any Pong clears any Ping": the server echoes each Ping's payload and the connection
    // survives well past several timeouts.
    //
    // The client must be RECEIVING for this to work, and that is not a quirk of the test -- see
    // the next case, which pins the limitation. A Pong is only observed inside ReceiveAsync,
    // because this port has no independent receive pump and neither does .NET's
    // ManagedWebSocket, which also processes pongs inside ReceiveAsyncPrivate.
    RunAgainstServer(
        [](ClientWebSocketOptions& o) {
            o.setKeepAliveIntervalProperty(System::TimeSpan::FromMilliseconds(30));
            o.setKeepAliveTimeoutProperty(System::TimeSpan::FromMilliseconds(120));
        },
        [&](Socket& server) {
            SharpRuntime::bytecs opcode = 0;
            std::vector<bytecs> payload;
            const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
            while (std::chrono::steady_clock::now() < until && readClientFrame(server, opcode, payload)) {
                if (opcode == 0x9) sendServerFrame(server, 0xA, payload);
            }
        },
        [&](ClientWebSocket& client) {
            // A receive that will never complete on its own -- the server sends no data frame --
            // but which keeps the pong-reading path running for its whole duration.
            std::vector<bytecs> buffer(64);
            auto pending = client.ReceiveAsync(buffer, 0, 64);
            std::this_thread::sleep_for(std::chrono::milliseconds(400));
            EXPECT_EQ(client.getStateProperty(), WebSocketState::Open)
                << "an answered ping must not fault the connection";
            EXPECT_FALSE(pending.getIsCompletedProperty())
                << "the receive is still waiting, which is what makes this a live connection";
        });
}

TEST(WebSocketsGatedBehaviourPins, Fix2094_PingPongNeedsAReceiverAndThatLimitIsPinnedNotHidden) {
    // THE LIMITATION, ASSERTED RATHER THAN ONLY COMMENTED. A Pong is observed only inside
    // ReceiveAsync. So a caller who enables the Ping/Pong strategy and then never receives will
    // lose the connection even against a perfectly healthy server that answers every Ping.
    //
    // This is why the DEFAULT strategy is unsolicited Pong (KeepAliveTimeout defaults to
    // Timeout.InfiniteTimeSpan in .NET and here): it expects nothing back, so it cannot fault.
    // Closing this gap needs an independent receive pump, which is new concurrency and new
    // buffering, and is deliberately not in #2094.
    RunAgainstServer(
        [](ClientWebSocketOptions& o) {
            o.setKeepAliveIntervalProperty(System::TimeSpan::FromMilliseconds(30));
            o.setKeepAliveTimeoutProperty(System::TimeSpan::FromMilliseconds(60));
        },
        [&](Socket& server) {
            // A COOPERATIVE server: it answers every single Ping correctly.
            SharpRuntime::bytecs opcode = 0;
            std::vector<bytecs> payload;
            const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(400);
            while (std::chrono::steady_clock::now() < until && readClientFrame(server, opcode, payload)) {
                if (opcode == 0x9) sendServerFrame(server, 0xA, payload);
            }
        },
        [&](ClientWebSocket& client) {
            // ...and a client that never receives.
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            EXPECT_EQ(client.getStateProperty(), WebSocketState::Aborted)
                << "if this is Open, an independent receive pump was added and this pin, the "
                   "doc-comment on ClientWebSocket::KeepAlive and the migration note must all "
                   "be updated in that change";
        });
}

TEST(WebSocketsGatedBehaviourPins, Fix2094_AWrongPongPayloadDoesNotClearTheOutstandingPing) {
    // OnPongResponseReceived compares the payload (ManagedWebSocket.KeepAlive.cs:174-184): only a
    // Pong echoing the outstanding Ping's counter clears it. A server that answers every Ping
    // with a DIFFERENT payload -- or an unsolicited Pong of its own -- must not keep the
    // connection alive, or the timeout means nothing.
    RunAgainstServer(
        [](ClientWebSocketOptions& o) {
            o.setKeepAliveIntervalProperty(System::TimeSpan::FromMilliseconds(30));
            o.setKeepAliveTimeoutProperty(System::TimeSpan::FromMilliseconds(60));
        },
        [&](Socket& server) {
            SharpRuntime::bytecs opcode = 0;
            std::vector<bytecs> payload;
            const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(400);
            while (std::chrono::steady_clock::now() < until && readClientFrame(server, opcode, payload)) {
                if (opcode == 0x9) sendServerFrame(server, 0xA, std::vector<bytecs>(8, 0x00));
            }
        },
        [&](ClientWebSocket& client) {
            std::vector<bytecs> buffer(64);
            auto pending = client.ReceiveAsync(buffer, 0, 64);
            EXPECT_THROW((void)pending.getResultProperty(), WebSocketException);
            EXPECT_EQ(client.getStateProperty(), WebSocketState::Aborted)
                << "a mismatched pong must not count as an answer";
        });
}

TEST(WebSocketsGatedBehaviourPins, Fix2094_DestroyingTheSocketReturnsEvenIfTheHeartbeatIsParkedInSend) {
    // The heartbeat can be blocked inside a blocking Send when the peer has stopped reading, so
    // setting its stop flag is not enough to reach the join. stopKeepAlive() shuts the socket
    // down first -- the same thing waitForAsyncOperations() does, and the reason #2358 had to
    // make a send to a dead peer raise an ERROR rather than SIGPIPE before this could work.
    //
    // Deliberately NOT using RunAgainstServer: that harness calls Dispose() before the object
    // dies, which shuts the socket down and hides the case. This destroys the object outright.
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
            if (keyPos == std::string::npos) return;
            const size_t keyStart = keyPos + std::string("Sec-WebSocket-Key: ").size();
            const std::string key = request.substr(keyStart, request.find("\r\n", keyStart) - keyStart);
            const auto digest = pinSha1(key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
            const std::string accept =
                System::Convert::ToBase64String(std::vector<bytecs>(digest.begin(), digest.end()));
            server->Send(toBytes("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\n"
                                 "Connection: Upgrade\r\nSec-WebSocket-Accept: " + accept + "\r\n\r\n"));
            // Read nothing at all, then hang up: the client's keep-alive frames pile up against a
            // peer that has stopped reading, which is what parks its Send.
            std::this_thread::sleep_for(std::chrono::milliseconds(120));
            server->Close();
        } catch (...) {
        }
    });

    const auto started = std::chrono::steady_clock::now();
    {
        auto client = std::make_unique<ClientWebSocket>();
        client->getOptionsProperty().setKeepAliveIntervalProperty(System::TimeSpan::FromMilliseconds(10));
        client->ConnectAsync(System::Uri("ws://127.0.0.1:" + std::to_string(port) + "/")).Wait();
        ASSERT_EQ(client->getStateProperty(), WebSocketState::Open);
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        client.reset();   // ~ClientWebSocket must join the heartbeat, not wait on it forever
    }
    const auto elapsed = std::chrono::steady_clock::now() - started;
    EXPECT_LT(std::chrono::duration_cast<std::chrono::seconds>(elapsed).count(), 10)
        << "the destructor could not join a heartbeat parked in send()";

    serverThread.join();
    listener.Close();
}

TEST(WebSocketsGatedBehaviourPins, Fix2094_AnInfiniteOrZeroIntervalStartsNoHeartbeatAtAll) {
    // ManagedWebSocket.cs:169 gates the whole timer on `keepAliveInterval > TimeSpan.Zero`, so
    // both spellings of "off" must produce no frames and no thread.
    for (auto interval : {System::TimeSpan::FromTicks(System::Threading::Timeout::InfiniteTimeSpan),
                          System::TimeSpan::FromTicks(0)}) {
        bool sawAnything = false;
        RunAgainstServer(
            [&](ClientWebSocketOptions& o) { o.setKeepAliveIntervalProperty(interval); },
            [&](Socket& server) {
                SharpRuntime::bytecs opcode = 0;
                std::vector<bytecs> payload;
                if (readClientFrame(server, opcode, payload)) sawAnything = true;
            },
            [&](ClientWebSocket& client) {
                (void)client;
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            });
        EXPECT_FALSE(sawAnything) << "a disabled keep-alive must send nothing";
    }
}

// ===========================================================================
// #2095, DEFERRED VERIFICATION: fragmentation ordering is never checked
// ===========================================================================

TEST(WebSocketsGatedBehaviourPins, Pin2095_AContinuationFrameWithNoMessageInProgressIsAccepted) {
    // Plan §7.9. ReceiveAsync never checks fragmentation ordering: a continuation frame (0x0)
    // arriving with NO message in progress is accepted and reported with `fragmentType_`'s
    // current value. The ticket is DEFERRED rather than compatible because exactly which
    // ordering violations .NET rejects, and with what error, cannot be determined without the
    // absent reference tree — so the measured behaviour is pinned instead of guessed at.
    //
    // MEASURED, and not what was first assumed: `fragmentType_` is declared
    // `= WebSocketMessageType::Binary` (ClientWebSocket.hpp:44), so a bare continuation arriving
    // on a fresh connection is reported as **Binary** — a message type chosen by a member
    // initialiser rather than by anything the server sent. Writing this pin against the assumed
    // default of Text made it fail, which is precisely what a pin is for.
    RunAgainstServer(
        noConfig,
        [&](Socket& server) {
            // A lone continuation frame, FIN=1, with nothing in progress.
            server.Send(std::vector<bytecs>{0x80, 0x02, 'h', 'i'});
        },
        [&](ClientWebSocket& client) {
            std::vector<bytecs> buffer(1024);
            auto result = client.ReceiveAsync(buffer, 0, 1024).getResultProperty();
            EXPECT_EQ(result.getCountProperty(), 2)
                << "#2095: a bare continuation frame is currently ACCEPTED.";
            EXPECT_EQ(result.getMessageTypeProperty(), WebSocketMessageType::Binary)
                << "#2095: it is reported with fragmentType_'s current value, whose member "
                   "initialiser is Binary. If this now throws, #2095 was resolved and this pin "
                   "must be updated in that change.";
        });
}

TEST(WebSocketsGatedBehaviourPins, Pin2095_ANewDataFrameArrivingMidFragmentIsAccepted) {
    // The other half of §7.9: a fresh data frame while a fragmented message is in progress.
    RunAgainstServer(
        noConfig,
        [&](Socket& server) {
            server.Send(std::vector<bytecs>{0x01, 0x02, 'a', 'b'});  // text, FIN=0
            server.Send(std::vector<bytecs>{0x82, 0x02, 'c', 'd'});  // binary, FIN=1, mid-fragment
        },
        [&](ClientWebSocket& client) {
            std::vector<bytecs> buffer(1024);
            auto first = client.ReceiveAsync(buffer, 0, 1024).getResultProperty();
            EXPECT_FALSE(first.getEndOfMessageProperty());
            auto second = client.ReceiveAsync(buffer, 0, 1024).getResultProperty();
            EXPECT_EQ(second.getMessageTypeProperty(), WebSocketMessageType::Binary)
                << "#2095: message boundaries are currently server-controllable. If this now "
                   "throws, #2095 was resolved and this pin must be updated.";
        });
}

// ===========================================================================
// #2096, BLOCKED: what is deliberately NOT pinned, and why
// ===========================================================================

// §7.11 — `state_` is written from task threads and read by `getStateProperty()` with no
// synchronisation, and `sendFrame`/`readFrame` dereference `socket_` with no null check after
// `Dispose()` has reset it.
//
// **No behavioural pin is written for #2096, deliberately.** A data race and a null dereference
// are undefined behaviour, not behaviour: a test that "passes" today would be asserting on the
// outcome of UB, which is neither stable nor meaningful, and a test that races on purpose would
// be flaky by construction. This is the same reasoning plan §4.6 gives for SR-AUD-247, whose pin
// is therefore a compile-time `static_assert` on the ownership MODEL
// (`ClientWebSocketFrameValidationTests`) rather than a behavioural assertion.
//
// #2096 is recorded here so that a reader looking for its pin finds this explanation instead of
// concluding one was forgotten.


// ===========================================================================================
// #2088 / SR-AUD-247 (CCF-019) — the liveness boundary, REPAIRED 2026-08-17
//
// Every *Async member returns a task whose body captures raw `this` and runs on a std::async
// thread, with nothing keeping this object alive until it ran. ~ClientWebSocket now waits.
//
// A pending ReceiveAsync is blocked in recv() waiting for a frame the peer may never send, so
// the boundary shuts the socket down before waiting -- otherwise it would turn a use-after-free
// into a hang. This is the case where shutdown() works; #2134 records the one where it does not.
// ===========================================================================================

TEST(WebSocketsGatedBehaviourPins, Fix2088_DestroyingTheSocketWaitsForAPendingReceive) {
    Socket listener(System::Net::Sockets::AddressFamily::InterNetwork,
                    System::Net::Sockets::SocketType::Stream,
                    System::Net::Sockets::ProtocolType::Tcp);
    listener.Bind(IPEndPoint(IPAddress::Loopback, 0));
    listener.Listen();
    auto local = std::dynamic_pointer_cast<IPEndPoint>(listener.getLocalEndPointProperty());
    ASSERT_NE(local, nullptr);
    const SharpRuntime::intcs port = local->getPortProperty();

    // The server completes the handshake and then sends NOTHING, so the client's receive stays
    // blocked -- which is the case the boundary has to be able to cross.
    std::thread serverThread([&]() {
        try {
            auto server = listener.Accept();
            const std::string request = readHeaders(*server);
            const size_t keyPos = request.find("Sec-WebSocket-Key: ");
            if (keyPos == std::string::npos) return;
            const size_t keyStart = keyPos + std::string("Sec-WebSocket-Key: ").size();
            const std::string key = request.substr(keyStart, request.find("\r\n", keyStart) - keyStart);
            const auto digest = pinSha1(key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
            const std::string accept =
                System::Convert::ToBase64String(std::vector<bytecs>(digest.begin(), digest.end()));
            server->Send(toBytes("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\n"
                                 "Connection: Upgrade\r\nSec-WebSocket-Accept: " + accept + "\r\n\r\n"));
            // Hold the connection open without ever sending a frame.
            std::this_thread::sleep_for(std::chrono::milliseconds(600));
            server->Close();
        } catch (...) {
        }
    });

    std::vector<bytecs> buffer(64);
    std::optional<System::Threading::Tasks::TaskT<WebSocketReceiveResult>> pending;
    const auto started = std::chrono::steady_clock::now();
    {
        ClientWebSocket client;
        System::Uri uri("ws://127.0.0.1:" + std::to_string(port) + "/");
        client.ConnectAsync(uri).Wait();
        ASSERT_EQ(client.getStateProperty(), WebSocketState::Open);
        pending = client.ReceiveAsync(buffer, 0, static_cast<SharpRuntime::intcs>(buffer.size()));
        std::this_thread::sleep_for(std::chrono::milliseconds(60));
    }   // ~ClientWebSocket must shut the socket down and WAIT here

    const auto elapsed = std::chrono::steady_clock::now() - started;
    EXPECT_LT(std::chrono::duration_cast<std::chrono::seconds>(elapsed).count(), 10)
        << "the destructor could not cross its own boundary";
    // ASSERT rather than EXPECT: without the boundary this case would hang at teardown instead
    // of failing, because the orphaned body would still be reading a destroyed object.
    ASSERT_TRUE(pending->getIsCompletedProperty())
        << "the destructor returned while an async receive was still running";

    serverThread.join();
    listener.Close();
}

TEST(WebSocketsGatedBehaviourPins, Fix2088_SendAsyncAcceptsATemporaryBuffer) {
    // The finding is wider than its headline: SendAsync captured `&buffer`, so the CALLER's
    // vector was a second borrowed object the task could outlive -- and the destructor boundary
    // cannot help there, because the buffer is not this object. Send only reads it, so #2088
    // copies the bytes it will actually send.
    //
    // HONEST LIMIT OF THIS CASE. The copy is guaranteed BY CONSTRUCTION, not by this assertion.
    // TaskT dispatches with std::async(std::launch::async), which starts the body immediately,
    // so a test that mutated the caller's buffer after the call and expected the OLD bytes on
    // the wire is a RACE: it passes whenever the body wins, which it usually does. That version
    // was written, measured against the borrowing mutation, found to pass anyway, and removed
    // rather than kept as a test that cannot fail. What is asserted here instead is the shape a
    // caller actually writes -- a temporary argument, which under borrowing is destroyed before
    // the body can read it -- and that the right bytes arrive.
    std::vector<bytecs> sent;
    RunAgainstServer(
        [](ClientWebSocketOptions&) {},
        [&sent](Socket& server) {
            std::vector<bytecs> frame(64);
            const auto n = server.Receive(frame);
            sent.assign(frame.begin(), frame.begin() + n);
        },
        [](ClientWebSocket& client) {
            client.SendAsync(std::vector<bytecs>{'a', 'b', 'c'}, 0, 3,
                             WebSocketMessageType::Binary, true).Wait();
        });

    ASSERT_GE(sent.size(), 7u);
    const std::size_t maskOffset = sent.size() - 3 - 4;
    std::vector<bytecs> unmasked;
    for (std::size_t i = 0; i < 3; ++i) {
        unmasked.push_back(static_cast<bytecs>(sent[sent.size() - 3 + i] ^ sent[maskOffset + i]));
    }
    EXPECT_EQ(unmasked, (std::vector<bytecs>{'a', 'b', 'c'}));
}
