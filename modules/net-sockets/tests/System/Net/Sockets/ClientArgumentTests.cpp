// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// #2137 / SR-AUD-266 (endpoint half) + SR-AUD-267 -- two constructors discarded the caller's
// argument. Cause NS-C.
//
// The port half is an argument domain: RFC 793's 16-bit port field settles that a value outside
// [0, 65535] is not a port, so no .NET reference is needed for it. The endpoint half is measured
// against the kernel: the test asks TcpClient for a specific local port and then asks
// getsockname() which port the connection actually used, because "the constructor stores it" is
// not the claim -- "the connection uses it" is.
//
// This is the ticket in the module that must prove a NEGATIVE about descriptors: a rejected port
// must not leak the socket that would otherwise have been created. LSan cannot answer that, so
// /proc/self/fd is read directly, with a deliberate leak as the discrimination control.
#include <gtest/gtest.h>

#include <limits>
#include <string>
#include <thread>

#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Net/IPAddress.hpp"
#include "System/Net/IPEndPoint.hpp"
#include "System/Net/Sockets/SocketException.hpp"
#include "System/Net/Sockets/TcpClient.hpp"
#include "System/Net/Sockets/UdpClient.hpp"

using System::Net::IPAddress;
using System::Net::IPEndPoint;
using System::Net::Sockets::TcpClient;
using System::Net::Sockets::TcpListener;
using System::Net::Sockets::UdpClient;

namespace {
constexpr int kIntMin = std::numeric_limits<int>::min();
constexpr int kIntMax = std::numeric_limits<int>::max();

// Every port value that is not a port. 65536 is the first one past the 16-bit field, 70000 is the
// review's example (it used to bind 4464), and INT_MIN/INT_MAX are the extremes that must not
// reach a cast.
const int kInvalidPorts[] = {-1, -2, 65536, 70000, kIntMin, kIntMax};
}  // namespace

// ===========================================================================================
// SR-AUD-267 -- the port domain, at every bare-int door
// ===========================================================================================

TEST(ClientPortDomainTests, UdpClientRejectsEveryPortThatIsNotAPort) {
    for (const int port : kInvalidPorts) {
        try {
            UdpClient client(port);
            ADD_FAILURE() << "UdpClient(" << port << ") was accepted";
        } catch (const System::ArgumentOutOfRangeException& e) {
            EXPECT_EQ(e.getParamNameProperty(), "port") << "port " << port;
        } catch (const std::exception& e) {
            ADD_FAILURE() << "UdpClient(" << port << ") threw the wrong type: " << e.what();
        }
    }
}

// The two Connect(host, port) doors were worse than permissive: measured before the repair, a
// NEGATIVE port was rejected by getaddrinfo as "DNS failed: Servname not supported", blaming name
// resolution for an argument the caller controls, while 65536 and 70000 were truncated and
// connected to the wrong port. Both now name the parameter that is wrong.
TEST(ClientPortDomainTests, ConnectByHostnameRejectsEveryPortThatIsNotAPort) {
    for (const int port : kInvalidPorts) {
        try {
            UdpClient client;
            client.Connect("127.0.0.1", port);
            ADD_FAILURE() << "UdpClient::Connect(host, " << port << ") was accepted";
        } catch (const System::ArgumentOutOfRangeException& e) {
            EXPECT_EQ(e.getParamNameProperty(), "port") << "port " << port;
        } catch (const std::exception& e) {
            ADD_FAILURE() << "UdpClient::Connect(host, " << port << "): " << e.what();
        }

        try {
            TcpClient client;
            client.Connect("127.0.0.1", port);
            ADD_FAILURE() << "TcpClient::Connect(host, " << port << ") was accepted";
        } catch (const System::ArgumentOutOfRangeException& e) {
            EXPECT_EQ(e.getParamNameProperty(), "port") << "port " << port;
        } catch (const std::exception& e) {
            ADD_FAILURE() << "TcpClient::Connect(host, " << port << "): " << e.what();
        }
    }
}

// THE CONTROL. The boundary values are ports and stay accepted -- without this, rejecting
// everything would pass the tests above.
TEST(ClientPortDomainTests, THECONTROLTheBoundaryPortsAreStillAccepted) {
    EXPECT_NO_THROW(UdpClient{0});
    EXPECT_NO_THROW(UdpClient{65535});

    // 65535 is a legal port to name in a connect, whether or not anything answers there. UDP
    // connect() only records the default remote, so it succeeds outright.
    UdpClient udp;
    EXPECT_NO_THROW(udp.Connect("127.0.0.1", 65535));
    EXPECT_NO_THROW(udp.Connect("127.0.0.1", 0));

    // TCP must get past the argument check and fail for a NETWORK reason instead -- proof that
    // the value was accepted as a port rather than rejected as an argument.
    TcpClient tcp;
    EXPECT_THROW(tcp.Connect("127.0.0.1", 1), System::Net::Sockets::SocketException);
}

// PIN: the IPEndPoint-taking overloads were always sound, because System::Net::IPEndPoint
// validates its own port. The repair must not have moved that rule or duplicated it.
TEST(ClientPortDomainTests, THEPINTheEndpointOverloadsValidateThroughIPEndPointAsBefore) {
    EXPECT_THROW(IPEndPoint(IPAddress::Loopback, -1), System::ArgumentOutOfRangeException);
    EXPECT_THROW(IPEndPoint(IPAddress::Loopback, 65536), System::ArgumentOutOfRangeException);
    EXPECT_NO_THROW(IPEndPoint(IPAddress::Loopback, 65535));
    EXPECT_THROW(TcpListener(IPAddress::Loopback, 65536), System::ArgumentOutOfRangeException);
    EXPECT_NO_THROW(UdpClient{IPEndPoint(IPAddress::Any, 0)});
}

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)

#  include <dirent.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <sys/socket.h>
#  include <unistd.h>

namespace {
int OpenDescriptorCount() {
    DIR* dir = ::opendir("/proc/self/fd");
    if (dir == nullptr) return -1;
    int entries = 0;
    while (::readdir(dir) != nullptr) ++entries;
    ::closedir(dir);
    return entries - 3;
}

// Binds an ephemeral loopback port, learns its number, then releases it -- giving a port number
// that is very likely free and is definitely not the one the OS would hand out next by default.
int ReserveThenReleaseALoopbackPort() {
    const int probe = ::socket(AF_INET, SOCK_STREAM, 0);
    if (probe < 0) return -1;
    struct sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    if (::bind(probe, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
        ::close(probe);
        return -1;
    }
    socklen_t len = sizeof(addr);
    ::getsockname(probe, reinterpret_cast<struct sockaddr*>(&addr), &len);
    const int port = ntohs(addr.sin_port);
    ::close(probe);
    return port;
}
}  // namespace

// The AC's hard half: a rejected port must not leak the socket already created. The check was
// deliberately placed BEFORE socket creation, so the socket never exists -- but that is a claim
// about the implementation, and this measures the consequence.
TEST(ClientPortDomainTests, ARejectedPortLeaksNoDescriptor) {
    const int before = OpenDescriptorCount();
    ASSERT_GT(before, 0);

    for (int repeat = 0; repeat < 64; ++repeat) {
        for (const int port : kInvalidPorts) {
            EXPECT_THROW(UdpClient{port}, System::ArgumentOutOfRangeException);
            EXPECT_THROW(
                [port] { UdpClient c; c.Connect("127.0.0.1", port); }(),
                System::ArgumentOutOfRangeException);
            EXPECT_THROW(
                [port] { TcpClient c; c.Connect("127.0.0.1", port); }(),
                System::ArgumentOutOfRangeException);
        }
    }

    EXPECT_EQ(before, OpenDescriptorCount()) << "1,152 rejected ports changed the descriptor count";
}

TEST(ClientPortDomainTests, THECONTROLTheDescriptorInstrumentDetectsADeliberateLeak) {
    const int before = OpenDescriptorCount();
    const int leaked = ::socket(AF_INET, SOCK_DGRAM, 0);
    ASSERT_GE(leaked, 0);
    EXPECT_EQ(before + 1, OpenDescriptorCount());
    ::close(leaked);
    EXPECT_EQ(before, OpenDescriptorCount());
}

// ===========================================================================================
// SR-AUD-266, endpoint half -- the local endpoint is honoured, measured against the kernel
// ===========================================================================================

TEST(TcpClientLocalEndpointTests, TheLocalEndpointIsActuallyUsedByTheConnection) {
    TcpListener listener(IPEndPoint(IPAddress::Loopback, 0));
    listener.Start();
    const int servicePort =
        static_cast<int>(listener.getLocalEndpointProperty().getPortProperty());
    ASSERT_GT(servicePort, 0);

    const int wantedLocalPort = ReserveThenReleaseALoopbackPort();
    ASSERT_GT(wantedLocalPort, 0);

    TcpClient client(IPEndPoint(IPAddress::Loopback, wantedLocalPort));
    EXPECT_FALSE(client.getConnectedProperty()) << "bound is not connected";

    std::thread server([&] { (void)listener.AcceptTcpClient(); });
    client.Connect(IPEndPoint(IPAddress::Loopback, servicePort));
    server.join();
    listener.Stop();

    ASSERT_TRUE(client.getConnectedProperty());
    auto stream = client.GetStream();
    struct sockaddr_in actual {};
    socklen_t len = sizeof(actual);
    ASSERT_EQ(0, ::getsockname(stream->getFd(), reinterpret_cast<struct sockaddr*>(&actual), &len));
    // Before #2137 this reported an ephemeral port unrelated to the request (measured: asked for
    // 40601, got 40998).
    EXPECT_EQ(wantedLocalPort, ntohs(actual.sin_port))
        << "the local endpoint was discarded again";
}

// The same claim for the hostname overload, which takes a different code path (getaddrinfo, and a
// socket created from the resolved family) and could easily honour the endpoint in one and not
// the other.
TEST(TcpClientLocalEndpointTests, TheLocalEndpointSurvivesTheHostnameConnectPathToo) {
    TcpListener listener(IPEndPoint(IPAddress::Loopback, 0));
    listener.Start();
    const int servicePort =
        static_cast<int>(listener.getLocalEndpointProperty().getPortProperty());
    const int wantedLocalPort = ReserveThenReleaseALoopbackPort();
    ASSERT_GT(wantedLocalPort, 0);

    TcpClient client(IPEndPoint(IPAddress::Loopback, wantedLocalPort));
    std::thread server([&] { (void)listener.AcceptTcpClient(); });
    client.Connect("127.0.0.1", servicePort);
    server.join();
    listener.Stop();

    auto stream = client.GetStream();
    struct sockaddr_in actual {};
    socklen_t len = sizeof(actual);
    ASSERT_EQ(0, ::getsockname(stream->getFd(), reinterpret_cast<struct sockaddr*>(&actual), &len));
    EXPECT_EQ(wantedLocalPort, ntohs(actual.sin_port));
}

// A local endpoint that cannot be bound now fails at construction, where the caller asked for it,
// rather than being silently ignored. Port 1 is privileged-or-taken often enough that this uses a
// port already held instead -- a deterministic conflict rather than a probabilistic one.
TEST(TcpClientLocalEndpointTests, AnUnavailableLocalEndpointFailsAtConstruction) {
    const int held = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(held, 0);
    struct sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    ASSERT_EQ(0, ::bind(held, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)));
    socklen_t len = sizeof(addr);
    ASSERT_EQ(0, ::getsockname(held, reinterpret_cast<struct sockaddr*>(&addr), &len));
    ASSERT_EQ(0, ::listen(held, 1));
    const int takenPort = ntohs(addr.sin_port);

    const int before = OpenDescriptorCount();
    EXPECT_THROW(TcpClient(IPEndPoint(IPAddress::Loopback, takenPort)),
                 System::Net::Sockets::SocketException);
    EXPECT_EQ(before, OpenDescriptorCount()) << "a failed bind leaked its socket";

    ::close(held);
}

// A failed Connect on a BOUND client must neither leak nor double-close: the object still owns
// exactly one descriptor afterwards, and destroying it releases exactly that one.
TEST(TcpClientLocalEndpointTests, AFailedConnectOnABoundClientNeitherLeaksNorDoubleCloses) {
    const int wantedLocalPort = ReserveThenReleaseALoopbackPort();
    ASSERT_GT(wantedLocalPort, 0);

    const int before = OpenDescriptorCount();
    {
        TcpClient client(IPEndPoint(IPAddress::Loopback, wantedLocalPort));
        EXPECT_EQ(before + 1, OpenDescriptorCount()) << "the bound socket is owned";
        // Port 1 on loopback is not listening.
        EXPECT_THROW(client.Connect(IPEndPoint(IPAddress::Loopback, 1)),
                     System::Net::Sockets::SocketException);
        EXPECT_FALSE(client.getConnectedProperty());
        EXPECT_EQ(before + 1, OpenDescriptorCount()) << "a failed connect changed the count";
    }
    EXPECT_EQ(before, OpenDescriptorCount()) << "destruction released exactly the bound socket";
}

// PIN: a DEFAULT-constructed client owns nothing until it connects, and a connect that fails
// leaves it owning nothing. This is the path the bound branch must not have disturbed.
TEST(TcpClientLocalEndpointTests, THEPINADefaultClientOwnsNothingAndCleansUpAfterAFailedConnect) {
    const int before = OpenDescriptorCount();
    {
        TcpClient client;
        EXPECT_EQ(before, OpenDescriptorCount());
        EXPECT_THROW(client.Connect("127.0.0.1", 1), System::Net::Sockets::SocketException);
        EXPECT_EQ(before, OpenDescriptorCount()) << "a failed connect leaked its socket";
        EXPECT_FALSE(client.getConnectedProperty());
    }
    EXPECT_EQ(before, OpenDescriptorCount());
}

#endif  // !_WIN32 && !__EMSCRIPTEN__
