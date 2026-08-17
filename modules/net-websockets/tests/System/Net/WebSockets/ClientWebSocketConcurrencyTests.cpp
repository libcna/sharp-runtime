// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Ticket #2096 (cause W-F, post-audit defect, no SR-AUD identifier).
//
// TWO DEFECTS, both reproduced here against a real loopback socket rather than argued about:
//
//   1. `state_` was written from task threads (ConnectAsync, ReceiveAsync, CloseAsync) and read
//      by the public `getStateProperty()` with no synchronisation -- a data race on a public
//      property, which is undefined behaviour and not merely a stale read. Measured, FOUR
//      members had that shape where the finding named one: `closeStatus_`,
//      `closeStatusDescription_` and `subProtocol_` too.
//
//   2. `Dispose()`/`Abort()` called `socket_->Close()` and reset the owning `unique_ptr` while a
//      task thread could be inside `socket_->Send` or `readExact(*socket_, …)`. That is a
//      use-after-free on the Socket object AND a closed descriptor number the process is free to
//      hand to something else while a worker is still syscalling on it. The
//      `ThrowOnInvalidState` check in `SendAsync`/`ReceiveAsync` is a TOCTOU against it, not a
//      guard.
//
// THE HARNESS. Reaching defect 2 does not need a WebSocket server: it needs a socket the client
// is BLOCKED on. A plain TCP listener that accepts the connection and then says nothing parks
// `performHandshake` inside `socket->Receive(one)` reading for the HTTP response, which is
// exactly the window `Dispose()` used to free the socket in. No handshake, no SHA-1, no
// protocol -- just the blocking read the repair is about.
//
// Run this suite under ThreadSanitizer to see defect 1; it is a race, so a passing run without
// TSan proves nothing about it and this file says so rather than implying otherwise.
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include "System/Net/IPAddress.hpp"
#include "System/Net/IPEndPoint.hpp"
#include "System/Net/Sockets/Socket.hpp"
#include "System/Net/WebSockets/ClientWebSocket.hpp"
#include "System/Net/WebSockets/WebSocketException.hpp"
#include "System/Net/WebSockets/WebSocketState.hpp"
#include "System/Uri.hpp"

using System::Net::IPAddress;
using System::Net::IPEndPoint;
using System::Net::Sockets::AddressFamily;
using System::Net::Sockets::ProtocolType;
using System::Net::Sockets::Socket;
using System::Net::Sockets::SocketType;
using System::Net::WebSockets::ClientWebSocket;
using System::Net::WebSockets::WebSocketException;
using System::Net::WebSockets::WebSocketState;

namespace {

/// A listener that accepts one connection and then deliberately never answers, so the client
/// stays parked in the handshake's blocking read for as long as this object lives.
class SilentServer {
public:
    SilentServer()
        : listener_(AddressFamily::InterNetwork, SocketType::Stream, ProtocolType::Tcp) {
        listener_.Bind(IPEndPoint(IPAddress::Loopback, 0));
        listener_.Listen(1);
        auto local = listener_.getLocalEndPointProperty();
        port_ = dynamic_cast<IPEndPoint*>(local.get())->getPortProperty();
        thread_ = std::thread([this] {
            try {
                accepted_ = listener_.Accept();
                acceptedFlag_.store(true, std::memory_order_release);
            } catch (...) {
                // The listener was closed before a client arrived; nothing to serve.
            }
        });
    }

    ~SilentServer() {
        try {
            listener_.Close();
        } catch (...) {
        }
        if (thread_.joinable()) thread_.join();
        accepted_.reset();
    }

    [[nodiscard]] SharpRuntime::intcs port() const { return port_; }

    /// Waits until the client's connection has actually been accepted, so a test never races the
    /// server's own startup. Returns false on timeout rather than hanging the suite.
    bool waitForConnection(std::chrono::milliseconds timeout = std::chrono::seconds(5)) const {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (acceptedFlag_.load(std::memory_order_acquire)) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return false;
    }

private:
    Socket                  listener_;
    std::shared_ptr<Socket> accepted_;
    std::atomic<bool>       acceptedFlag_{false};
    std::thread             thread_;
    SharpRuntime::intcs     port_ = 0;
};

std::string wsUriFor(SharpRuntime::intcs port) {
    return "ws://127.0.0.1:" + std::to_string(static_cast<int>(port)) + "/";
}

} // namespace

TEST(ClientWebSocketConcurrencyTests, Fix2096_DisposeDuringAnInFlightHandshakeDoesNotFreeTheSocketUnderIt) {
    // THE HEADLINE REPRODUCTION. Before #2096 this freed the Socket object while the task thread
    // was inside its Receive(), which ASan reports as a heap-use-after-free and which in a
    // release build is a null dereference on the next call. The repair is shared ownership: the
    // worker holds a strong reference for the whole handshake, so Dispose() hands its own
    // reference over instead of deleting.
    SilentServer server;
    ClientWebSocket ws;
    auto task = ws.ConnectAsync(System::Uri(wsUriFor(server.port())));

    ASSERT_TRUE(server.waitForConnection()) << "the client never reached the server";
    // The client is now parked in the handshake's blocking read. Give it a moment to be sure it
    // is inside Receive rather than merely past Connect.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    ws.Dispose();

    // The task must FINISH -- an unblocked failure, not a hang and not a crash.
    EXPECT_THROW(task.Wait(), System::Exception);
    EXPECT_EQ(ws.getStateProperty(), WebSocketState::Closed);
}

TEST(ClientWebSocketConcurrencyTests, Fix2096_AbortDuringAnInFlightHandshakeIsTheSameRepair) {
    // Abort() reaches Dispose() through a different door and used to have the identical defect.
    SilentServer server;
    ClientWebSocket ws;
    auto task = ws.ConnectAsync(System::Uri(wsUriFor(server.port())));

    ASSERT_TRUE(server.waitForConnection());
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    ws.Abort();

    EXPECT_THROW(task.Wait(), System::Exception);
    // Abort's state survives Dispose's own write -- unchanged behaviour, asserted because the
    // ordering now happens inside one critical section rather than across two.
    EXPECT_EQ(ws.getStateProperty(), WebSocketState::Aborted);
}

TEST(ClientWebSocketConcurrencyTests, Fix2096_TheStatePropertyIsSafeToReadWhileATaskThreadWritesIt) {
    // DEFECT 1. ConnectAsync publishes Connecting on the caller's thread and the task thread then
    // publishes Closed (via the failure path's Dispose), while this thread reads the public
    // property in a loop. Before #2096 that was an unsynchronised read of a non-atomic enum
    // written by another thread -- a data race, which ThreadSanitizer reports and which the
    // as-if rule permits a compiler to miscompile.
    //
    // WITHOUT TSAN THIS TEST CANNOT FAIL FOR THE RIGHT REASON. It is here so the scenario runs
    // under the sanitizer build, and so the reader is told plainly that a green run of the plain
    // build is not evidence about the race.
    SilentServer server;
    ClientWebSocket ws;
    auto task = ws.ConnectAsync(System::Uri(wsUriFor(server.port())));

    ASSERT_TRUE(server.waitForConnection());

    std::atomic<bool> stop{false};
    std::vector<WebSocketState> observed;
    std::thread reader([&] {
        while (!stop.load(std::memory_order_acquire)) {
            observed.push_back(ws.getStateProperty());
            (void)ws.getCloseStatusProperty();
            (void)ws.getCloseStatusDescriptionProperty();
            (void)ws.getSubProtocolProperty();
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ws.Dispose();
    EXPECT_THROW(task.Wait(), System::Exception);
    stop.store(true, std::memory_order_release);
    reader.join();

    ASSERT_FALSE(observed.empty());
    // Every observation must be one of the states this sequence can legally produce. A torn or
    // invented value would fail here even without TSan.
    for (WebSocketState s : observed) {
        EXPECT_TRUE(s == WebSocketState::Connecting || s == WebSocketState::Closed ||
                    s == WebSocketState::None)
            << "unexpected observed state " << static_cast<int>(s);
    }
    EXPECT_EQ(ws.getStateProperty(), WebSocketState::Closed);
}

TEST(ClientWebSocketConcurrencyTests, Fix2096_AnOperationAfterDisposeThrowsRatherThanDereferencingNull) {
    // The non-racy half of the same door: once Dispose() has taken the socket away, every
    // operation must raise, and it must raise the SAME exception whichever check catches it
    // first. socketForIo() deliberately reuses WebSocketException(InvalidState) for that reason
    // -- see its doc-comment, and ticket #2357 for whether the whole family should be
    // ObjectDisposedException the way .NET's is.
    SilentServer server;
    ClientWebSocket ws;
    auto task = ws.ConnectAsync(System::Uri(wsUriFor(server.port())));
    ASSERT_TRUE(server.waitForConnection());
    ws.Dispose();
    EXPECT_THROW(task.Wait(), System::Exception);

    std::vector<SharpRuntime::bytecs> buffer(16, 0);
    auto send = ws.SendAsync(buffer, 0, 4, System::Net::WebSockets::WebSocketMessageType::Binary, true);
    EXPECT_THROW(send.Wait(), WebSocketException);

    auto receive = ws.ReceiveAsync(buffer, 0, 4);
    EXPECT_THROW((void)receive.getResultProperty(), WebSocketException);

    auto close = ws.CloseAsync(System::Net::WebSockets::WebSocketCloseStatus::NormalClosure, std::nullopt);
    EXPECT_NO_THROW(close.Wait()) << "a close on an already-closed socket is a no-op, not a fault";
}

TEST(ClientWebSocketConcurrencyTests, Fix2096_AllFiveAsyncMembersJoinTheLivenessBoundary) {
    // #2096 also closes a gap #2088 left: only SendAsync and ReceiveAsync called
    // beginAsyncOperation(), so ConnectAsync, CloseAsync and CloseOutputAsync captured raw `this`
    // with no owner liveness at all -- the very defect #2088 was written to remove.
    //
    // Destroying the ClientWebSocket while its ConnectAsync body is still parked in the
    // handshake read is the shape that used to be a use-after-free on `this`. It must now
    // return, not crash and not hang.
    SilentServer server;
    {
        auto ws = std::make_unique<ClientWebSocket>();
        auto task = ws->ConnectAsync(System::Uri(wsUriFor(server.port())));
        ASSERT_TRUE(server.waitForConnection());
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        ws.reset();  // ~ClientWebSocket waits for the in-flight ConnectAsync
        EXPECT_THROW(task.Wait(), System::Exception);
    }
    SUCCEED() << "the destructor returned rather than racing the handshake thread";
}
