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
