// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// #2139 -- the System::Net::Sockets review's gated and measured behaviours, pinned.
//
// This file began as two kinds of pin with opposite meanings:
//
//   * A GATED pin records behaviour that is currently WRONG and is waiting on a decision --
//     #2134 (CCF-019, then unapproved) and #2138 (how far this port carries IPv6). A
//     failure here does NOT mean something regressed; it means somebody took the decision, and
//     the pin is what makes that visible instead of silent.
//   * A MEASURED-POSITIVE pin records something the review checked and found correct, so a later
//     change cannot quietly undo it while everyone assumes it was covered.
//
// Both gated decisions later landed. The sections are retained in place but their assertions are
// now the positive repaired contracts. Ticket #2417 adds deterministic move-source and task-start
// exception regressions after a final review found two residual holes in #2134's boundary.
#include <gtest/gtest.h>
#include <atomic>
#include <optional>
#include <thread>
#include <chrono>

#include <functional>
#include <memory>
#include <stdexcept>
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
using System::Net::Sockets::AddressFamily;
using System::Net::Sockets::NetworkStream;
using System::Net::Sockets::SocketError;
using System::Net::Sockets::SendPacketsElement;
using System::Net::Sockets::Socket;
using System::Net::Sockets::TcpClient;
using System::Net::Sockets::TcpListener;
using System::Net::Sockets::UdpClient;

namespace SharpRuntime::Testing {

template <>
struct SocketAsyncStartAccess<System::Net::Sockets::Socket> {
    static void setBeforeTaskHook(void (*hook)()) {
        System::Net::Sockets::Socket::beforeAsyncTaskTestHook_.store(hook);
    }

    static int inFlight(const System::Net::Sockets::Socket& socket) {
        return socket.asyncOperationCountForTesting();
    }
};

} // namespace SharpRuntime::Testing

namespace {

void throwBeforeSocketTaskConstruction() {
    throw std::runtime_error("injected async task construction failure");
}

class ScopedSocketAsyncTaskHook final {
public:
    ScopedSocketAsyncTaskHook() {
        SharpRuntime::Testing::SocketAsyncStartAccess<Socket>::setBeforeTaskHook(
            &throwBeforeSocketTaskConstruction);
    }

    ~ScopedSocketAsyncTaskHook() {
        SharpRuntime::Testing::SocketAsyncStartAccess<Socket>::setBeforeTaskHook(nullptr);
    }

    void clear() {
        SharpRuntime::Testing::SocketAsyncStartAccess<Socket>::setBeforeTaskHook(nullptr);
    }

    ScopedSocketAsyncTaskHook(const ScopedSocketAsyncTaskHook&) = delete;
    ScopedSocketAsyncTaskHook& operator=(const ScopedSocketAsyncTaskHook&) = delete;
};

} // namespace

// ===========================================================================================
// GATED — #2134 / SR-AUD-263 / CCF-019: the ownership model, pinned by shape
// ===========================================================================================
//
// Ticket #2134 selected an internal RAII in-flight-work boundary, preserving these four public
// shape facts while making destruction and move-assignment wait safely for async work.
static_assert(!std::is_base_of_v<std::enable_shared_from_this<Socket>, Socket>,
              "#2134/CCF-019 pin: the repair uses an internal RAII boundary, not shared self-ownership");
static_assert(std::is_nothrow_destructible_v<Socket>,
              "#2134/CCF-019 pin: the joining destructor remains noexcept");
static_assert(std::is_move_assignable_v<Socket>,
              "#2134/CCF-019 pin: move-assignment retains the same in-flight-work boundary");
static_assert(!std::is_copy_constructible_v<Socket> && !std::is_copy_assignable_v<Socket>,
              "Socket must stay non-copyable: two owners of one descriptor is a double close.");

TEST(SocketsGatedBehaviourPins, THEGATEDPINTheAsyncMembersStillReturnTheirCurrentShapes) {
    // The internal boundary did not need to change either public return type.
    static_assert(std::is_same_v<decltype(std::declval<Socket&>().AcceptAsync()),
                                 System::Threading::Tasks::TaskT<std::shared_ptr<Socket>>>);
    static_assert(std::is_same_v<decltype(std::declval<Socket&>().SendAsync(
                                     std::vector<SharpRuntime::bytecs>{})),
                                 System::Threading::Tasks::TaskT<SharpRuntime::intcs>>);
    SUCCEED() << "SR-AUD-263 was repaired by #2134's internal RAII liveness boundary while "
                 "preserving the async members' public shapes.";
}

// ===========================================================================================
// #2363 RESOLVED — IPv6 and dual-stack, and #2138's limitation is GONE rather than unreachable
//
// #2138's three pins that stood here are INVERTED, not deleted. Each asserted something true of
// an AF_INET-only module; this ticket removed that property, so the honest record is the new
// statement in the old place.
//
// THREE PREMISE CORRECTIONS, all measured against /rv/tmp/runtime:
//
//   1. The ticket said .NET "resolves with AF_UNSPEC, walking the getaddrinfo result list". Half
//      right. `Socket.Connect(string host, int port)` calls `IPAddress.TryParse(host)` FIRST
//      (Socket.cs:919-923) and only then falls back to `Dns.GetHostAddresses` plus the array
//      overload. A LITERAL at a hostname parameter is deliberately a literal in .NET.
//
//   2. That settles the question #2138 deferred, and settles it AWAY from #2359. #2138 left the
//      hostname door alone because "is `::1` at a hostname parameter a literal or a name" looked
//      like `System::Uri`'s host-grammar question. It is not: a URI authority has bracket syntax
//      and a socket hostname parameter does not, and .NET answers the socket question inside the
//      socket code. #2359 is untouched by this ticket.
//
//   3. The ticket said every AF_INET constant in the module must become the endpoint's or the
//      resolved family. Two must NOT: `UdpClient()` and `UdpClient(int port)` are
//      `AddressFamily.InterNetwork` in .NET (UDPClient.cs:24,47), unlike `TcpClient()`, which is
//      dual-mode IPv6 (TCPClient.cs:368-383). Making UDP's default dual-stack would be a
//      deviation dressed as a repair; a case below pins that it was not made.
// ===========================================================================================

namespace {
    /// True when this machine can actually bind an IPv6 loopback socket.
    ///
    /// SA-6 says a test that passes only because of a property of the machine is a defect in the
    /// test. For a LIVE socket case that cannot be made to disappear -- an IPv6 connection needs
    /// IPv6 -- so it is made explicit instead: assertions that need no network run
    /// unconditionally, and only the live rows consult this.
    bool IPv6LoopbackAvailable() {
        try {
            TcpListener probe(IPAddress::IPv6Loopback, 0);
            probe.Start();
            probe.Stop();
            return true;
        } catch (...) {
            return false;
        }
    }
}  // namespace

TEST(SocketsGatedBehaviourPins, Fix2363_EveryEndpointDoorNowCarriesIPv6) {
    const IPAddress v6 = IPAddress::Parse("::1");
    ASSERT_TRUE(v6.getIsIPv6Property());
    EXPECT_EQ(IPEndPoint(v6, 80).ToString(), "[::1]:80");
    ASSERT_TRUE(IPv6LoopbackAvailable())
        << "this machine has no IPv6 loopback, so these rows cannot be evidence here";

    // These four doors CREATE the socket from the endpoint they were handed, so there is nothing
    // for them to disagree with and #2138's refusal is gone outright. Construction alone is the
    // assertion -- before this ticket every one of them threw ArgumentException.
    EXPECT_NO_THROW((void)TcpClient(IPEndPoint(v6, 0)));
    EXPECT_NO_THROW((void)TcpListener(IPEndPoint(v6, 0)));
    EXPECT_NO_THROW((void)TcpListener(v6, 0));
    EXPECT_NO_THROW((void)UdpClient(IPEndPoint(v6, 0)));

    // ...and the family really is IPv6, not an IPv4 socket that merely accepted the argument.
    TcpClient bound(IPEndPoint(v6, 0));
    EXPECT_EQ(bound.getAddressFamilyProperty(), AddressFamily::InterNetworkV6);
    UdpClient udp(IPEndPoint(v6, 0));
    EXPECT_EQ(udp.getAddressFamilyProperty(), AddressFamily::InterNetworkV6);
}

TEST(SocketsGatedBehaviourPins, Fix2363_AnIPv6EndpointConnectsBindsAndAccepts) {
    ASSERT_TRUE(IPv6LoopbackAvailable());
    // The acceptance criterion, end to end. No part of this case was reachable before the ticket.
    TcpListener listener(IPAddress::IPv6Loopback, 0);
    listener.Start();
    const auto port = listener.getLocalEndpointProperty().getPortProperty();
    ASSERT_GT(port, 0) << "Start() must report the ephemeral port from an AF_INET6 getsockname";

    // Connect FIRST and accept afterwards -- the listen backlog holds the connection, so no
    // second thread is needed. That is deliberate: a thread that blocks in AcceptTcpClient turns
    // a failed connect into a HANG, and a mutation caught only as a hang is a mutation whose
    // evidence is a timeout rather than a named failing test.
    TcpClient c;
    ASSERT_NO_THROW(c.Connect(IPEndPoint(IPAddress::IPv6Loopback, port)));
    EXPECT_EQ(c.getAddressFamilyProperty(), AddressFamily::InterNetworkV6);

    TcpClient accepted = listener.AcceptTcpClient();
    // THE accept-path criterion: the peer's REAL family, not a constant. With the old
    // sockaddr_in accept an IPv6 peer was reported as InterNetwork.
    EXPECT_EQ(accepted.getAddressFamilyProperty(), AddressFamily::InterNetworkV6);
    listener.Stop();
}

TEST(SocketsGatedBehaviourPins, Fix2363_AnIPv6LiteralAtAHostnameParameterResolves) {
    // The inversion of THEGATEDPINAnIPv6LiteralIsRefusedByDNSBecauseTheHintsAreAF_INET, whose own
    // text said it would invert "when #2363 was decided". It is decided.
    ASSERT_TRUE(IPv6LoopbackAvailable());
    TcpListener listener(IPAddress::IPv6Loopback, 0);
    listener.Start();
    const auto port = listener.getLocalEndpointProperty().getPortProperty();

    TcpClient c;
    // The backlog absorbs the connection, so nothing has to be accepted for this to complete --
    // see the note in Fix2363_AnIPv6EndpointConnectsBindsAndAccepts for why no thread is used.
    ASSERT_NO_THROW(c.Connect("::1", static_cast<SharpRuntime::intcs>(port)));
    EXPECT_TRUE(c.getConnectedProperty());
    EXPECT_EQ(c.getAddressFamilyProperty(), AddressFamily::InterNetworkV6);
    listener.Stop();
}

TEST(SocketsGatedBehaviourPins, Fix2363_TheFamilyRefusalSurvivesOnlyWhereASocketAlreadyExists) {
    const IPAddress v6 = IPAddress::Parse("::1");
    ASSERT_TRUE(IPv6LoopbackAvailable());

    // .NET's check, in .NET's place: a socket that ALREADY EXISTS has a family, and an endpoint
    // of another family cannot be used with it (Socket.cs:1757-1759). Two doors reach it.
    //
    // A UdpClient always owns a socket, and the default one is IPv4.
    {
        UdpClient c;
        ASSERT_EQ(c.getAddressFamilyProperty(), AddressFamily::InterNetwork);
        try {
            c.Connect(IPEndPoint(v6, 80));
            ADD_FAILURE() << "an IPv6 endpoint was used with an IPv4 socket";
        } catch (const System::ArgumentException& e) {
            const std::string what = e.what();
            // The sentence is UNCHANGED from #2138 -- only the condition narrowed -- and both
            // families are now filled in rather than one of them being a literal.
            EXPECT_NE(what.find("The supplied EndPoint of AddressFamily InterNetworkV6 is not "
                                "valid for this Socket, use InterNetwork instead."),
                      std::string::npos) << what;
            EXPECT_NE(what.find("Parameter 'remoteEP'"), std::string::npos) << what;
        }
    }
    // A TcpClient BOUND to an IPv4 local endpoint owns one too.
    {
        TcpClient bound(IPEndPoint(IPAddress::Loopback, 0));
        EXPECT_THROW(bound.Connect(IPEndPoint(v6, 80)), System::ArgumentException);
    }
    // ...and the mirror image, which is the half proving the check reads the SOCKET rather than
    // hard-coding IPv4 as the one acceptable family.
    {
        TcpClient bound(IPEndPoint(v6, 0));
        EXPECT_THROW(bound.Connect(IPEndPoint(IPAddress::Loopback, 80)), System::ArgumentException);
    }
    // An UNBOUND TcpClient creates the socket from the endpoint, so it has nothing to disagree
    // with and must NOT refuse. This is the row that fails if an unconditional check comes back.
    {
        TcpClient fresh;
        try {
            fresh.Connect(IPEndPoint(v6, 1));   // nothing listens on port 1
            ADD_FAILURE() << "connect to a closed IPv6 port unexpectedly succeeded";
        } catch (const System::ArgumentException& e) {
            ADD_FAILURE() << "an unbound client still refuses IPv6 at the door: " << e.what();
        } catch (const System::Net::Sockets::SocketException&) {
            SUCCEED() << "refused by connect(), which is the transport -- not by an argument check";
        }
    }
}

TEST(SocketsGatedBehaviourPins, Fix2363_TheResolverResultListIsWALKEDNotJustItsHead) {
    // Taking only `res` -- the first getaddrinfo result -- meant a host with several addresses
    // failed outright whenever the first was unreachable. This machine's `localhost` resolves to
    // `::1` FIRST and `127.0.0.1` second (verified: `getent hosts localhost`), so an IPv4-only
    // listener plus `Connect("localhost", port)` is exactly that shape: the head of the list is
    // refused and the second entry is the one that works.
    //
    // SA-6 applies and is answered rather than ignored: the case asserts only what it establishes
    // on a machine that has BOTH families, and says so if it does not.
    ASSERT_TRUE(IPv6LoopbackAvailable())
        << "a one-family machine cannot exercise a result-list walk";
    TcpListener listener(IPAddress::Loopback, 0);   // IPv4 ONLY, deliberately
    listener.Start();
    const auto port = listener.getLocalEndpointProperty().getPortProperty();

    TcpClient c;
    ASSERT_NO_THROW(c.Connect("localhost", static_cast<SharpRuntime::intcs>(port)));
    EXPECT_TRUE(c.getConnectedProperty());
    // ...and it landed on the IPv4 entry, which is the only one anything is listening on.
    EXPECT_EQ(c.getAddressFamilyProperty(), AddressFamily::InterNetwork);
    listener.Stop();
}

TEST(SocketsGatedBehaviourPins, Fix2363_UdpReceiveReportsAnIPv6SenderRatherThanFabricatingOne) {
    // The one genuinely SILENT misreport in this family. Every send and bind path failed loudly
    // when handed IPv6, but `Receive` read the sender through a `sockaddr_in` overlaid on a
    // 28-byte `sockaddr_in6`: the four bytes at the IPv4 address offset land inside the IPv6
    // address, so the caller was handed a plausible, fabricated IPv4 address for a datagram that
    // came from somewhere else entirely.
    ASSERT_TRUE(IPv6LoopbackAvailable());
    UdpClient receiver(IPEndPoint(IPAddress::IPv6Loopback, 0));
    ASSERT_EQ(receiver.getAddressFamilyProperty(), AddressFamily::InterNetworkV6);

    // Discover the bound port through a second socket's view is not available here, so bind a
    // known-free one by asking the OS for an ephemeral TCP port first and reusing the number.
    SharpRuntime::intcs port = 0;
    {
        TcpListener probe(IPAddress::IPv6Loopback, 0);
        probe.Start();
        port = probe.getLocalEndpointProperty().getPortProperty();
        probe.Stop();
    }
    UdpClient bound(IPEndPoint(IPAddress::IPv6Loopback, port));

    UdpClient sender(IPEndPoint(IPAddress::IPv6Loopback, 0));
    sender.Connect(IPEndPoint(IPAddress::IPv6Loopback, port));
    std::vector<SharpRuntime::bytecs> payload{7, 8, 9};
    ASSERT_EQ(sender.Send(payload, 3), 3);

    IPEndPoint from(IPAddress::Any, 0);
    const auto got = bound.Receive(from);
    EXPECT_EQ(got, payload);
    // THE assertion: the sender is reported as IPv6, and as the loopback address it really was.
    EXPECT_TRUE(from.getAddressProperty().getIsIPv6Property())
        << "the sender was reported as " << from.ToString();
    EXPECT_EQ(from.getAddressProperty().ToString(), "::1");
    EXPECT_GT(from.getPortProperty(), 0);
}

TEST(SocketsGatedBehaviourPins, Fix2363_UdpClientsDefaultStaysIPv4BecauseDotNetsDoes) {
    // PREMISE CORRECTION 3, pinned. A mutation that "finishes the job" by making these two
    // dual-stack is caught here rather than shipping as an improvement.
    EXPECT_EQ(UdpClient().getAddressFamilyProperty(), AddressFamily::InterNetwork);
    EXPECT_EQ(UdpClient(0).getAddressFamilyProperty(), AddressFamily::InterNetwork);

    // The consequence .NET has and this port now reproduces: a default UdpClient cannot reach an
    // IPv6-only peer, because `IsAddressFamilyCompatible` (UDPClient.cs:743-745) filters the
    // resolved addresses down to the socket's own family. The refusal is no longer "DNS failed"
    // -- the resolver SUCCEEDS now, and the filter is what says no.
    UdpClient c;
    try {
        c.Connect("::1", 80);
        ADD_FAILURE() << "an IPv4 UDP socket connected to an IPv6 literal";
    } catch (const System::Net::Sockets::SocketException& e) {
        EXPECT_EQ(e.getSocketErrorCodeProperty(), SocketError::AddressFamilyNotSupported)
            << "the resolver succeeded; the FILTER must be what refuses: " << e.what();
        // ...and the FILTER must be what refuses, not connect() being handed a sockaddr_in6.
        // Both routes produce EAFNOSUPPORT, so the error code alone cannot tell them apart --
        // only the sentence can, which is why this line is here.
        EXPECT_NE(std::string(e.what()).find("this client's socket cannot carry"),
                  std::string::npos)
            << "the address was handed to connect() instead of being filtered out: " << e.what();
    }
}

TEST(SocketsGatedBehaviourPins, Fix2363_LayoutPin_TheFamilyStateIsFree) {
    // SA-3: the before/after sizeof is pinned rather than asserted in prose, and here it pins
    // that the cost was ZERO -- which is a design decision, not luck.
    //
    // MEASURED, AND THE FIRST CUT WAS WRONG. Storing the family as an `AddressFamily` grew
    // sizeof(TcpClient) from 24 to 32: the three bytes of padding after `connected_` cannot hold
    // a 4-byte, 4-aligned enum, so the whole scalar block rounds up and the shared_ptr moves. The
    // shadows below are what caught it. A bool fits, and gives up nothing -- this port's
    // `IPAddress` is `bool isIPv6_` with the family COMPUTED (IPAddress.cpp:326-328), so neither
    // type can carry a third family regardless of how the state is spelled.
    struct TcpClientShadow {
        int                   fd;
        bool                  connected;
        bool                  isIPv6;
        std::shared_ptr<void> stream;
    };
    struct TcpClientWithAnEnum {
        int                                 fd;
        bool                                connected;
        System::Net::Sockets::AddressFamily family;
        std::shared_ptr<void>               stream;
    };
    EXPECT_EQ(sizeof(TcpClient), sizeof(TcpClientShadow));
    EXPECT_EQ(alignof(TcpClient), alignof(TcpClientShadow));
    EXPECT_LT(sizeof(TcpClientShadow), sizeof(TcpClientWithAnEnum))
        << "the two spellings must actually differ, or this pin asserts nothing about the cost";
    EXPECT_EQ(sizeof(TcpClient), 24u) << "TcpClient grew; consumers would need a rebuild";

    // The same statement for UdpClient, whose trailing bool likewise costs nothing.
    struct UdpShadowWithout {
        int             fd;
        IPEndPoint      remote;
        bool            hasRemote;
    };
    struct UdpShadowWith {
        int             fd;
        IPEndPoint      remote;
        bool            hasRemote;
        bool            isIPv6;
    };
    EXPECT_EQ(sizeof(UdpClient), sizeof(UdpShadowWith));
    EXPECT_EQ(sizeof(UdpShadowWith), sizeof(UdpShadowWithout))
        << "UdpClient's family bool was supposed to land in existing padding";
}

TEST(SocketsGatedBehaviourPins, Fix2138_EveryIPv4PathIsUntouched) {
    // The narrowing must be exactly zero for IPv4. All six repaired doors accept an ordinary
    // address, and the endpoint that used to work still does.
    EXPECT_NO_THROW((void)TcpClient(IPEndPoint(IPAddress::Loopback, 0)));
    EXPECT_NO_THROW((void)TcpListener(IPEndPoint(IPAddress::Loopback, 0)));
    EXPECT_NO_THROW((void)TcpListener(IPAddress::Any, 0));
    EXPECT_NO_THROW((void)UdpClient(IPEndPoint(IPAddress::Loopback, 0)));
    // IPAddress::Any is InterNetwork, so the wildcard is NOT collateral damage -- the check is a
    // family test, not an "is it the loopback" test.
    EXPECT_EQ(IPAddress::Any.getAddressFamilyProperty(),
              System::Net::Sockets::AddressFamily::InterNetwork);
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

TEST(SocketsGatedBehaviourPins, Fix2417_MoveConstructorCrossesTheSourceBoundary) {
    // Every async body captures the SOURCE address. The original move constructor copied and
    // cleared its descriptor fields immediately, leaving source destruction to wait only later;
    // that is a data race while the pending AcceptAsync still calls Poll()/Accept() through the
    // source object. Moving must first abort and drain that work, then transfer the descriptor.
    Socket source(System::Net::Sockets::AddressFamily::InterNetwork,
                  System::Net::Sockets::SocketType::Stream,
                  System::Net::Sockets::ProtocolType::Tcp);
    source.Bind(IPEndPoint(IPAddress::Loopback, 0));
    source.Listen(1);
    const auto local = std::dynamic_pointer_cast<IPEndPoint>(source.getLocalEndPointProperty());
    ASSERT_NE(local, nullptr);
    const auto originalHandle = source.getHandleProperty();

    auto staleSourceWork = source.AcceptAsync();
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    Socket moved(std::move(source));

    EXPECT_EQ(SharpRuntime::Testing::SocketAsyncStartAccess<Socket>::inFlight(source), 0)
        << "move construction returned while source async work still owned the source object";
    EXPECT_THROW((void)staleSourceWork.getResultProperty(),
                 System::Net::Sockets::SocketException);
    EXPECT_EQ(moved.getHandleProperty(), originalHandle);

    // Draining the old source guard must not poison the destination's fresh guard or retire the
    // listening descriptor. The moved-to socket remains usable for new asynchronous work.
    auto accepted = moved.AcceptAsync();
    Socket client(System::Net::Sockets::AddressFamily::InterNetwork,
                  System::Net::Sockets::SocketType::Stream,
                  System::Net::Sockets::ProtocolType::Tcp);
    client.Connect(*local);
    EXPECT_NE(accepted.getResultProperty(), nullptr);
}

TEST(SocketsGatedBehaviourPins, Fix2417_MoveAssignmentDrainsSourceAndReopensDestination) {
    Socket source(System::Net::Sockets::AddressFamily::InterNetwork,
                  System::Net::Sockets::SocketType::Stream,
                  System::Net::Sockets::ProtocolType::Tcp);
    source.Bind(IPEndPoint(IPAddress::Loopback, 0));
    source.Listen(1);
    const auto local = std::dynamic_pointer_cast<IPEndPoint>(source.getLocalEndPointProperty());
    ASSERT_NE(local, nullptr);
    const auto originalHandle = source.getHandleProperty();

    auto staleSourceWork = source.AcceptAsync();
    std::this_thread::sleep_for(std::chrono::milliseconds(80));

    Socket destination(System::Net::Sockets::AddressFamily::InterNetwork,
                       System::Net::Sockets::SocketType::Dgram,
                       System::Net::Sockets::ProtocolType::Udp);
    destination = std::move(source);

    EXPECT_EQ(SharpRuntime::Testing::SocketAsyncStartAccess<Socket>::inFlight(source), 0)
        << "move assignment returned while source async work still owned the source object";
    EXPECT_THROW((void)staleSourceWork.getResultProperty(),
                 System::Net::Sockets::SocketException);
    EXPECT_EQ(destination.getHandleProperty(), originalHandle);

    // waitForAsyncOperations marks a guard stopping even when it had zero work. The destination
    // guard must be reopened after assignment; otherwise this AcceptAsync aborts immediately.
    auto accepted = destination.AcceptAsync();
    Socket client(System::Net::Sockets::AddressFamily::InterNetwork,
                  System::Net::Sockets::SocketType::Stream,
                  System::Net::Sockets::ProtocolType::Tcp);
    client.Connect(*local);
    EXPECT_NE(accepted.getResultProperty(), nullptr);
}

TEST(SocketsGatedBehaviourPins, Fix2417_SourceMovePreservesAConnectedSocketAfterPendingReceive) {
    // shutdown() is an acceptable way to cross the boundary only when the descriptor will be
    // discarded. The first source-move repair used it for ReceiveAsync and then transferred the
    // irreversibly shutdown descriptor. Hold a receive pending, prove move waits naturally, feed
    // that receive, and finally exercise the moved-to connection in BOTH directions.
    Socket listener(System::Net::Sockets::AddressFamily::InterNetwork,
                    System::Net::Sockets::SocketType::Stream,
                    System::Net::Sockets::ProtocolType::Tcp);
    listener.Bind(IPEndPoint(IPAddress::Loopback, 0));
    listener.Listen(1);
    const auto local = std::dynamic_pointer_cast<IPEndPoint>(listener.getLocalEndPointProperty());
    ASSERT_NE(local, nullptr);

    Socket peer(System::Net::Sockets::AddressFamily::InterNetwork,
                System::Net::Sockets::SocketType::Stream,
                System::Net::Sockets::ProtocolType::Tcp);
    peer.Connect(*local);
    auto source = listener.Accept();
    ASSERT_NE(source, nullptr);

    auto firstBuffer = std::make_shared<std::vector<SharpRuntime::bytecs>>(1);
    auto pendingReceive = source->ReceiveAsync(firstBuffer);
    std::this_thread::sleep_for(std::chrono::milliseconds(80));

    std::optional<Socket> moved;
    std::atomic<bool> moveReturned{false};
    std::thread mover([&] {
        moved.emplace(std::move(*source));
        moveReturned.store(true);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(80));

    // On the broken shutdown-based source boundary, move has already returned here with a
    // disabled descriptor. Join before reporting that path so a failed assertion never strands
    // a joinable test thread.
    if (moveReturned.load()) {
        mover.join();
        ADD_FAILURE() << "source move interrupted pending ReceiveAsync instead of preserving the socket";
        return;
    }

    const std::vector<SharpRuntime::bytecs> firstByte{0x31};
    int sent = -1;
    try {
        sent = peer.Send(firstByte);
    } catch (...) {
        // Closing the peer still releases the pending receive, so the mover can always be joined
        // before this test reports a socket-preservation failure.
        peer.Close();
    }
    int received = -1;
    try {
        received = pendingReceive.getResultProperty();
    } catch (...) {
    }
    mover.join();
    ASSERT_EQ(sent, 1);
    ASSERT_EQ(received, 1);
    EXPECT_EQ((*firstBuffer)[0], firstByte[0]);
    ASSERT_TRUE(moved.has_value());

    const std::vector<SharpRuntime::bytecs> fromMoved{0x42};
    std::vector<SharpRuntime::bytecs> atPeer(1);
    ASSERT_EQ(moved->Send(fromMoved), 1);
    ASSERT_EQ(peer.Receive(atPeer), 1);
    EXPECT_EQ(atPeer[0], fromMoved[0]);

    const std::vector<SharpRuntime::bytecs> fromPeer{0x53};
    std::vector<SharpRuntime::bytecs> atMoved(1);
    ASSERT_EQ(peer.Send(fromPeer), 1);
    ASSERT_EQ(moved->Receive(atMoved), 1);
    EXPECT_EQ(atMoved[0], fromPeer[0]);
}

TEST(SocketsGatedBehaviourPins, Fix2417_CloseDrainsPendingAsyncWorkBeforeRetiringTheDescriptor) {
    // Public Close used to bypass the destructor's boundary and retire fd_ while ReceiveAsync
    // still used this object. Linux commonly leaves that already-entered recv blocked even after
    // close, making the leaked in-flight registration directly observable.
    Socket listener(System::Net::Sockets::AddressFamily::InterNetwork,
                    System::Net::Sockets::SocketType::Stream,
                    System::Net::Sockets::ProtocolType::Tcp);
    listener.Bind(IPEndPoint(IPAddress::Loopback, 0));
    listener.Listen(1);
    const auto local = std::dynamic_pointer_cast<IPEndPoint>(listener.getLocalEndPointProperty());
    ASSERT_NE(local, nullptr);

    Socket peer(System::Net::Sockets::AddressFamily::InterNetwork,
                System::Net::Sockets::SocketType::Stream,
                System::Net::Sockets::ProtocolType::Tcp);
    peer.Connect(*local);
    auto closing = listener.Accept();
    ASSERT_NE(closing, nullptr);

    auto buffer = std::make_shared<std::vector<SharpRuntime::bytecs>>(1);
    auto pendingReceive = closing->ReceiveAsync(buffer);
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    ASSERT_EQ(SharpRuntime::Testing::SocketAsyncStartAccess<Socket>::inFlight(*closing), 1);

    closing->Close();
    const int afterClose =
        SharpRuntime::Testing::SocketAsyncStartAccess<Socket>::inFlight(*closing);

    // Keep this regression safe if the boundary is removed: wake a recv that a plain close may
    // have left alive before asking the Task for its result or destroying its owner.
    if (afterClose != 0) {
        try {
            (void)peer.Send(std::vector<SharpRuntime::bytecs>{0x64});
        } catch (...) {
        }
    }
    try {
        (void)pendingReceive.getResultProperty();
    } catch (const System::Net::Sockets::SocketException&) {
    }
    EXPECT_EQ(afterClose, 0)
        << "Close returned while raw-this async work still owned the socket";
    EXPECT_EQ(closing->getHandleProperty(), -1);
}

TEST(SocketsGatedBehaviourPins, Fix2417_FailedTaskStartRollsBackItsRegistration) {
    Socket listener(System::Net::Sockets::AddressFamily::InterNetwork,
                    System::Net::Sockets::SocketType::Stream,
                    System::Net::Sockets::ProtocolType::Tcp);
    listener.Bind(IPEndPoint(IPAddress::Loopback, 0));
    listener.Listen(1);
    const auto local = std::dynamic_pointer_cast<IPEndPoint>(listener.getLocalEndPointProperty());
    ASSERT_NE(local, nullptr);

    ScopedSocketAsyncTaskHook hook;
    EXPECT_THROW((void)listener.AcceptAsync(), std::runtime_error);
    EXPECT_EQ(SharpRuntime::Testing::SocketAsyncStartAccess<Socket>::inFlight(listener), 0)
        << "a Task construction failure leaked its caller-side async registration";

    // The rollback is not merely an internal count: after clearing the seam the same socket can
    // start and complete real work, and its eventual destructor has nothing stale to wait for.
    hook.clear();
    auto accepted = listener.AcceptAsync();
    Socket client(System::Net::Sockets::AddressFamily::InterNetwork,
                  System::Net::Sockets::SocketType::Stream,
                  System::Net::Sockets::ProtocolType::Tcp);
    client.Connect(*local);
    EXPECT_NE(accepted.getResultProperty(), nullptr);
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
