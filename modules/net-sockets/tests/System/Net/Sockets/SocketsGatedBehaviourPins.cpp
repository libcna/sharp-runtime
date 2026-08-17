// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// #2139 -- the System::Net::Sockets review's gated and measured behaviours, pinned.
//
// Two kinds of pin live here, and they mean opposite things:
//
//   * A GATED pin records behaviour that is currently WRONG and is waiting on a decision --
//     #2134 (CCF-019, unapproved) and #2138 (how far this port carries IPv6, needs_user). A
//     failure here does NOT mean something regressed; it means somebody took the decision, and
//     the pin is what makes that visible instead of silent.
//   * A MEASURED-POSITIVE pin records something the review checked and found correct, so a later
//     change cannot quietly undo it while everyone assumes it was covered.
//
// #2134 is pinned by SHAPE, deliberately. A racing use-after-free reproduction of a lifetime bug
// is flaky by construction (the reason #2096 already recorded), and a data race is undefined
// behaviour rather than behaviour. What can be pinned exactly is that the ownership model has not
// changed -- which is precisely what the CCF-019 repair would change.
#include <gtest/gtest.h>
#include <optional>
#include <thread>
#include <chrono>

#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "System/ArgumentException.hpp"
#include "System/Exception.hpp"
#include "System/Net/IPAddress.hpp"
#include "System/Net/IPEndPoint.hpp"
#include "System/Net/Sockets/NetworkStream.hpp"
#include "System/Net/Sockets/SendPacketsElement.hpp"
#include "System/Net/Sockets/Socket.hpp"
#include "System/Net/Sockets/SocketException.hpp"
#include "System/Net/Sockets/TcpClient.hpp"
#include "System/Net/Sockets/UdpClient.hpp"

using System::Net::IPAddress;
using System::Net::IPEndPoint;
using System::Net::Sockets::NetworkStream;
using System::Net::Sockets::SendPacketsElement;
using System::Net::Sockets::Socket;
using System::Net::Sockets::TcpClient;
using System::Net::Sockets::TcpListener;
using System::Net::Sockets::UdpClient;

// ===========================================================================================
// GATED — #2134 / SR-AUD-263 / CCF-019: the ownership model, pinned by shape
// ===========================================================================================
//
// Every repair option changes one of these four facts:
//   * shared ownership (`enable_shared_from_this<Socket>`) changes the base list;
//   * a joining `~Socket` gains a throw path, so the destructor stops being noexcept;
//   * move-assignment is what makes "destroyed or moved while a worker runs" reachable at all;
//   * handing back `shared_ptr<Socket>` from `AcceptAsync` is the current signature, and a
//     shared-ownership redesign is the thing most likely to alter it.
static_assert(!std::is_base_of_v<std::enable_shared_from_this<Socket>, Socket>,
              "#2134/CCF-019 pin: Socket has gained shared self-ownership -- the CCF-019 repair "
              "landed. Update this pin deliberately; it is not a regression.");
static_assert(std::is_nothrow_destructible_v<Socket>,
              "#2134/CCF-019 pin: ~Socket has gained a throw path, which is what a joining "
              "destructor would do. Update this pin deliberately.");
static_assert(std::is_move_assignable_v<Socket>,
              "#2134/CCF-019 pin: Socket is no longer move-assignable. Move-assignment while an "
              "async worker runs is the reachability half of SR-AUD-263.");
static_assert(!std::is_copy_constructible_v<Socket> && !std::is_copy_assignable_v<Socket>,
              "Socket must stay non-copyable: two owners of one descriptor is a double close.");

TEST(SocketsGatedBehaviourPins, THEGATEDPINTheAsyncMembersStillReturnTheirCurrentShapes) {
    // A shared-ownership redesign is expected to change at least one of these.
    static_assert(std::is_same_v<decltype(std::declval<Socket&>().AcceptAsync()),
                                 System::Threading::Tasks::TaskT<std::shared_ptr<Socket>>>);
    static_assert(std::is_same_v<decltype(std::declval<Socket&>().SendAsync(
                                     std::vector<SharpRuntime::bytecs>{})),
                                 System::Threading::Tasks::TaskT<SharpRuntime::intcs>>);
    SUCCEED() << "SR-AUD-263 is UNREPAIRED and blocked on CCF-019 approval; the four async "
                 "members still capture a raw `this` with no liveness boundary. Socket.hpp's "
                 "own @warning states the caller-side contract that stands in for a fix.";
}

// ===========================================================================================
// GATED — #2138 / SR-AUD-266 family half / NS-F: every path is AF_INET only
// ===========================================================================================

// A PREMISE CORRECTION, measured rather than assumed (build-probe/2139_probe1_v6.log). The review
// described the IPv6 limitation as "misrepresented" and #2138's option (b) offers to make it
// "loud rather than silent". Measured, IT IS ALREADY LOUD, and by an accident of a different
// module: every AF_INET path reaches IPAddress::getAddressProperty(), which throws
// SocketException(OperationNotSupported) -- "The requested property is not supported for the
// 'InterNetworkV6' AddressFamily." -- for a v6 address. Nothing connects over IPv4 while
// pretending to be IPv6, and no address is silently truncated. What IS true is that the refusal
// names an IPAddress property rather than the operation the caller attempted, and that the two
// hostname paths refuse for a THIRD reason (hints.ai_family = AF_INET, so getaddrinfo never
// resolves the literal). Three different refusals, none of them the operation's own.
//
// These pins assert the exact refusals, because "it did not connect" would pass for the wrong
// reason: nothing is listening on [::1]:80 either way.

TEST(SocketsGatedBehaviourPins, THEGATEDPINAnIPv6LiteralIsRefusedByDNSBecauseTheHintsAreAF_INET) {
    for (auto call : {std::function<void()>([] { TcpClient c; c.Connect("::1", 80); }),
                      std::function<void()>([] { UdpClient c; c.Connect("::1", 80); })}) {
        try {
            call();
            ADD_FAILURE() << "an IPv6 literal now resolves -- #2138 was decided";
        } catch (const System::Net::Sockets::SocketException& e) {
            EXPECT_EQ(e.getSocketErrorCodeProperty(), System::Net::Sockets::SocketError::HostNotFound)
                << "the refusal is supposed to come from getaddrinfo, not from connect()";
            EXPECT_NE(std::string(e.what()).find("DNS failed"), std::string::npos) << e.what();
        }
    }
}

TEST(SocketsGatedBehaviourPins, THEGATEDPINAnIPv6EndpointIsRefusedByIPAddressNotSilentlyNarrowed) {
    const IPAddress v6 = IPAddress::Parse("::1");
    ASSERT_TRUE(v6.getIsIPv6Property());

    // The endpoint itself is perfectly representable -- it is only unusable.
    EXPECT_EQ(IPEndPoint(v6, 80).ToString(), "[::1]:80");

    const std::vector<std::pair<const char*, std::function<void()>>> ipv4OnlyPaths = {
        {"TcpClient::Connect(endpoint)", [&] { TcpClient c; c.Connect(IPEndPoint(v6, 80)); }},
        {"TcpListener::Start()", [&] { TcpListener l(IPEndPoint(v6, 0)); l.Start(); }},
        {"UdpClient(endpoint)", [&] { UdpClient c(IPEndPoint(v6, 0)); }},
    };

    for (const auto& [name, call] : ipv4OnlyPaths) {
        try {
            call();
            ADD_FAILURE() << name << " accepted an IPv6 endpoint -- #2138 was decided";
        } catch (const System::Net::Sockets::SocketException& e) {
            EXPECT_EQ(e.getSocketErrorCodeProperty(),
                      System::Net::Sockets::SocketError::OperationNotSupported)
                << name << ": " << e.what();
            EXPECT_NE(std::string(e.what()).find("InterNetworkV6"), std::string::npos)
                << name << ": " << e.what();
        }
    }
}

// ===========================================================================================
// MEASURED POSITIVES — plan §6.2, so a later change cannot quietly undo them
// ===========================================================================================

// §6.1: an EMPTY file path is accepted by SendPacketsElement. Recorded and deliberately NOT
// ticketed, because the only consumer -- Socket::SendPacketsAsync -- is absent from this port, so
// there is no door at which the empty path can do anything. Pinned so that if that consumer is
// ever added, this shows up as a decision rather than a surprise.
TEST(SocketsGatedBehaviourPins, THERECORDEDPINAnEmptySendPacketsFilePathIsStillAccepted) {
    EXPECT_NO_THROW(SendPacketsElement(std::string("")));
}

// §6.2: no std:: exception escapes a public door. Every rejection is a System:: exception, which
// is what lets a caller write one catch clause. System::Exception derives from std::exception, so
// the System:: clause must come first -- an escaping std:: exception is one that reaches the
// second clause.
TEST(SocketsGatedBehaviourPins, THEPINNoStdExceptionEscapesAPublicDoor) {
    auto classify = [](const std::function<void()>& call) -> const char* {
        try {
            call();
        } catch (const System::Exception&) {
            return "System";
        } catch (const std::exception&) {
            return "std";
        } catch (...) {
            return "unknown";
        }
        return "none";
    };

    const std::vector<std::pair<const char*, std::function<void()>>> doors = {
        {"SendPacketsElement negative count", [] { SendPacketsElement({1, 2, 3}, 0, -1); }},
        {"SendPacketsElement negative offset", [] { SendPacketsElement({1, 2, 3}, -1, 1); }},
        {"SendPacketsElement count past end", [] { SendPacketsElement({1, 2, 3}, 0, 4); }},
        {"UdpClient negative port", [] { UdpClient c(-1); }},
        {"UdpClient port past 65535", [] { UdpClient c(65536); }},
        {"TcpClient::Connect bad port", [] { TcpClient c; c.Connect("127.0.0.1", 70000); }},
        {"TcpClient::Connect bad host", [] { TcpClient c; c.Connect("no.such.host.invalid", 80); }},
        {"TcpClient::GetStream unconnected", [] { TcpClient c; (void)c.GetStream(); }},
        {"TcpListener::Accept unstarted",
         [] { TcpListener l(IPEndPoint(IPAddress::Loopback, 0)); (void)l.AcceptTcpClient(); }},
        {"IPEndPoint bad port", [] { IPEndPoint e(IPAddress::Loopback, -1); }},
        {"UdpClient::Send without Connect",
         [] { UdpClient c; std::vector<SharpRuntime::bytecs> d{1}; (void)c.Send(d, 1); }},
    };

    for (const auto& [name, call] : doors) {
        const std::string kind = classify(call);
        EXPECT_EQ(kind, "System") << name << " produced a " << kind << " exception";
    }
}

// §6.2: NetworkStream::getLengthProperty answers "does not support seeking" rather than
// fabricating a length. Pinned in NetworkStreamStateTests for the open and closed cases; this is
// the corresponding statement for the type as a whole -- the stream is not seekable and does not
// pretend otherwise.
TEST(SocketsGatedBehaviourPins, THEPINTheDescriptorOwningTypesAreAllNonCopyable) {
    // Two owners of one descriptor is a double close. Pinned here as one statement about the
    // module rather than three separate assertions scattered across suites.
    static_assert(!std::is_copy_constructible_v<NetworkStream>);
    static_assert(!std::is_copy_assignable_v<NetworkStream>);
    static_assert(!std::is_copy_constructible_v<TcpClient>);
    static_assert(!std::is_copy_assignable_v<TcpClient>);
    static_assert(!std::is_copy_constructible_v<TcpListener>);
    static_assert(!std::is_copy_assignable_v<TcpListener>);
    static_assert(!std::is_copy_constructible_v<UdpClient>);
    static_assert(!std::is_copy_assignable_v<UdpClient>);
    SUCCEED();
}

// ===========================================================================================
// #2134 / SR-AUD-263 (CCF-019) — the liveness boundary, REPAIRED 2026-08-17
//
// The four *Async members each build a TaskT from a lambda capturing raw `this`, and TaskT
// dispatches with std::async(std::launch::async) immediately. Socket is move-assignable and
// destructible with no join, no shared_from_this and no retention of any kind, so destroying or
// move-assigning one while a body was running read freed storage.
//
// .NET needs no boundary because the GC keeps the object alive for as long as the captured
// delegate can reach it. C++ has no such mechanism, so this port takes the RAII answer: the
// object outlives the work because its destructor waits for it -- the same shape #2347 gave
// FileSystemWatcher. Every one of #2139's four pins above survives unchanged, which is the point
// of this design over the alternative: Socket does NOT become enable_shared_from_this, ~Socket
// stays noexcept, and the async return types are untouched.
//
// The awkward case is AcceptAsync. shutdown() reliably unblocks recv()/send(), but on Linux it
// does NOT unblock accept() on a listening socket, and close() is unsafe while another thread is
// inside the syscall -- so a naive boundary would turn a use-after-free into a HANG. That member
// therefore waits on this type's own portable Poll() in 50 ms slices and re-checks a stop flag
// between them.
// ===========================================================================================

TEST(SocketsGatedBehaviourPins, Fix2134_DestroyingASocketWaitsForAPendingAccept) {
    // Before the repair this either raced or, with a naive shutdown-based boundary, hung. The
    // deadline is the assertion: the destructor must cross the boundary in bounded time.
    const auto started = std::chrono::steady_clock::now();
    std::optional<System::Threading::Tasks::TaskT<std::shared_ptr<Socket>>> pending;
    {
        Socket listener(System::Net::Sockets::AddressFamily::InterNetwork,
                        System::Net::Sockets::SocketType::Stream,
                        System::Net::Sockets::ProtocolType::Tcp);
        listener.Bind(IPEndPoint(IPAddress::Loopback, 0));
        listener.Listen(1);
        pending = listener.AcceptAsync();
        // Give the body time to reach its wait, so the destructor really does meet an in-flight
        // operation rather than winning a race against one that has not started.
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
    }   // ~Socket runs here and must WAIT for the accept body

    const auto elapsed = std::chrono::steady_clock::now() - started;
    EXPECT_LT(std::chrono::duration_cast<std::chrono::seconds>(elapsed).count(), 10)
        << "the destructor did not cross its own boundary in bounded time";

    // THE assertion, and the one that makes this mutation-sensitive without a sanitizer: the
    // destructor must not RETURN until the work is finished. Checked here, before anything else
    // gets a chance to let the body catch up, so a missing boundary shows up as a failed
    // expectation rather than as a hang somewhere later.
    // ASSERT rather than EXPECT, deliberately: getResultProperty() below BLOCKS until the body
    // finishes, so without the boundary this case would hang instead of failing. Stopping here
    // converts a missing boundary into a reportable failure.
    ASSERT_TRUE(pending->getIsCompletedProperty())
        << "the destructor returned while an async body was still running";

    // The body reports the abort rather than returning a socket built from a dead descriptor.
    EXPECT_THROW((void)pending->getResultProperty(), System::Net::Sockets::SocketException);
}

TEST(SocketsGatedBehaviourPins, Fix2134_APendingAcceptStillCompletesNormally) {
    // The control the case above needs: the boundary must not have been satisfied by making
    // AcceptAsync never work. A connection that actually arrives is still accepted.
    Socket listener(System::Net::Sockets::AddressFamily::InterNetwork,
                    System::Net::Sockets::SocketType::Stream,
                    System::Net::Sockets::ProtocolType::Tcp);
    listener.Bind(IPEndPoint(IPAddress::Loopback, 0));
    listener.Listen(1);
    const auto local = std::dynamic_pointer_cast<IPEndPoint>(listener.getLocalEndPointProperty());
    ASSERT_NE(local, nullptr);

    auto pending = listener.AcceptAsync();

    Socket client(System::Net::Sockets::AddressFamily::InterNetwork,
                  System::Net::Sockets::SocketType::Stream,
                  System::Net::Sockets::ProtocolType::Tcp);
    client.Connect(IPEndPoint(IPAddress::Loopback, local->getPortProperty()));

    std::shared_ptr<Socket> accepted = pending.getResultProperty();
    EXPECT_NE(accepted, nullptr);
}

TEST(SocketsGatedBehaviourPins, Fix2134_MoveAssignmentAlsoCrossesTheBoundary) {
    // Move-assignment replaces the descriptor and every field while a body may still be reading
    // them, so it needs the same boundary as the destructor -- and asserting it separately is
    // what stops a repair that only guarded ~Socket.
    Socket target(System::Net::Sockets::AddressFamily::InterNetwork,
                  System::Net::Sockets::SocketType::Stream,
                  System::Net::Sockets::ProtocolType::Tcp);
    target.Bind(IPEndPoint(IPAddress::Loopback, 0));
    target.Listen(1);
    auto pending = target.AcceptAsync();
    std::this_thread::sleep_for(std::chrono::milliseconds(80));

    const auto started = std::chrono::steady_clock::now();
    target = Socket(System::Net::Sockets::AddressFamily::InterNetwork,
                    System::Net::Sockets::SocketType::Stream,
                    System::Net::Sockets::ProtocolType::Tcp);
    const auto elapsed = std::chrono::steady_clock::now() - started;

    EXPECT_LT(std::chrono::duration_cast<std::chrono::seconds>(elapsed).count(), 10);
    ASSERT_TRUE(pending.getIsCompletedProperty())
        << "move-assignment returned while an async body was still running";
    EXPECT_THROW((void)pending.getResultProperty(), System::Net::Sockets::SocketException);
}

TEST(SocketsGatedBehaviourPins, Fix2134_LayoutPin_TheCostOfTheBoundary) {
    // SA-3 requires the before/after sizeof to be pinned rather than asserted in prose. Pinned
    // against SHADOW STRUCTS rather than literals, the way IoLayoutPinTests does, so a change to
    // any member's own size shows up as a shadow mismatch instead of a bare number to re-guess.
    //
    // The boundary costs exactly one shared_ptr -- and the shadows are what make "exactly" real,
    // because the scalar block also gains alignment padding once an 8-byte member joins it: the
    // arithmetic sum of the parts is NOT the answer, which is why this is not written as one.
    struct WithoutBoundary {
        SharpRuntime::intcs fd;
        System::Net::Sockets::AddressFamily family;
        System::Net::Sockets::SocketType type;
        System::Net::Sockets::ProtocolType protocol;
        bool connected;
        bool bound;
        bool blocking;
    };
    struct WithBoundary : WithoutBoundary {
        std::shared_ptr<void> asyncOps;
    };
    EXPECT_EQ(sizeof(Socket), sizeof(WithBoundary));
    EXPECT_EQ(alignof(Socket), alignof(WithBoundary));
    EXPECT_GT(sizeof(WithBoundary), sizeof(WithoutBoundary))
        << "the shadow pair must actually differ, or this pin asserts nothing";
}
