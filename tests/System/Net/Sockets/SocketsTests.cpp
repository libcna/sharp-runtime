// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <vector>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/un.h>
#include "System/NotSupportedException.hpp"
#include "System/Net/IPAddress.hpp"
#include "System/Net/IPEndPoint.hpp"
#include "System/Net/Sockets/NetworkStream.hpp"
#include "System/Net/Sockets/TcpClient.hpp"
#include "System/Net/Sockets/UdpClient.hpp"

using System::Net::IPAddress;
using System::Net::IPEndPoint;
using System::Net::Sockets::NetworkStream;
using System::Net::Sockets::TcpClient;
using System::Net::Sockets::TcpListener;
using System::Net::Sockets::UdpClient;

// ===========================================================================
// TcpClient
// ===========================================================================

TEST(TcpClientTests, DefaultConstructor_NoThrow) {
    EXPECT_NO_THROW(TcpClient{});
}

TEST(TcpClientTests, EndPointConstructor_NoThrow) {
    IPEndPoint ep(IPAddress::Parse("127.0.0.1"), 8080);
    EXPECT_NO_THROW(TcpClient{ep});
}

TEST(TcpClientTests, Connect_InvalidHostname_Throws) {
    TcpClient client;
    EXPECT_THROW(client.Connect("this.host.does.not.exist.invalid.example", 80), std::runtime_error);
}

TEST(TcpClientTests, Connect_ConnectionRefused_Throws) {
    // Port 1 on loopback is almost certainly not listening.
    TcpClient client;
    EXPECT_THROW(client.Connect("127.0.0.1", 1), std::runtime_error);
}

TEST(TcpClientTests, Connect_EndPoint_ConnectionRefused_Throws) {
    TcpClient client;
    IPEndPoint ep(IPAddress::Loopback, 1);
    EXPECT_THROW(client.Connect(ep), std::runtime_error);
}

TEST(TcpClientTests, Close_NoThrow) {
    TcpClient client;
    EXPECT_NO_THROW(client.Close());
}

TEST(TcpClientTests, Connected_ReturnsFalse_WhenNotConnected) {
    TcpClient client;
    EXPECT_FALSE(client.getConnectedProperty());
}

TEST(TcpClientTests, Available_ReturnsZero_WhenNotConnected) {
    TcpClient client;
    EXPECT_EQ(0, client.Available());
}

TEST(TcpClientTests, GetStream_Throws_WhenNotConnected) {
    TcpClient client;
    EXPECT_THROW((void)client.GetStream(), std::runtime_error);
}

// ===========================================================================
// TcpListener
// ===========================================================================

TEST(TcpListenerTests, EndPointConstructor_NoThrow) {
    IPEndPoint ep(IPAddress::Parse("0.0.0.0"), 0);
    EXPECT_NO_THROW(TcpListener{ep});
}

TEST(TcpListenerTests, AddressPortConstructor_NoThrow) {
    IPAddress addr = IPAddress::Parse("0.0.0.0");
    EXPECT_NO_THROW(TcpListener(addr, 0));
}

TEST(TcpListenerTests, Start_Stop_NoThrow) {
    IPEndPoint ep(IPAddress::Parse("0.0.0.0"), 0);
    TcpListener listener(ep);
    EXPECT_NO_THROW(listener.Start());
    EXPECT_NO_THROW(listener.Stop());
}

TEST(TcpListenerTests, Start_AssignsPort_WhenPortZero) {
    IPEndPoint ep(IPAddress::Parse("0.0.0.0"), 0);
    TcpListener listener(ep);
    listener.Start();
    EXPECT_GT(listener.getLocalEndpointProperty().getPortProperty(), 0);
    listener.Stop();
}

TEST(TcpListenerTests, Stop_NoThrow_WhenNotStarted) {
    IPEndPoint ep(IPAddress::Parse("0.0.0.0"), 0);
    TcpListener listener(ep);
    EXPECT_NO_THROW(listener.Stop());
}

// ===========================================================================
// UdpClient
// ===========================================================================

TEST(UdpClientTests, DefaultConstructor_NoThrow) {
    EXPECT_NO_THROW(UdpClient{});
}

TEST(UdpClientTests, PortConstructor_BindsToFreePort) {
    // Port 0 → OS picks a free port.
    EXPECT_NO_THROW(UdpClient{0});
}

TEST(UdpClientTests, EndPointConstructor_NoThrow) {
    IPEndPoint ep(IPAddress::Parse("0.0.0.0"), 0);
    EXPECT_NO_THROW(UdpClient{ep});
}

TEST(UdpClientTests, ClientProperty_TrueAfterConstruction) {
    UdpClient client;
    EXPECT_TRUE(client.getClientProperty());
}

TEST(UdpClientTests, ClientProperty_FalseAfterClose) {
    UdpClient client;
    client.Close();
    EXPECT_FALSE(client.getClientProperty());
}

TEST(UdpClientTests, Connect_EndPoint_NoThrow) {
    // UDP connect just sets the default remote — no real connection established.
    UdpClient client;
    IPEndPoint ep(IPAddress::Loopback, 12345);
    EXPECT_NO_THROW(client.Connect(ep));
}

TEST(UdpClientTests, Connect_InvalidHostname_Throws) {
    UdpClient client;
    EXPECT_THROW(client.Connect("this.host.does.not.exist.invalid.example", 5000), std::runtime_error);
}

TEST(UdpClientTests, Send_ThrowsWithoutConnect) {
    UdpClient client;
    std::vector<SharpRuntime::bytecs> data = {0x01, 0x02, 0x03};
    EXPECT_THROW((void)client.Send(data, 3), std::runtime_error);
}

TEST(UdpClientTests, Close_NoThrow) {
    UdpClient client;
    EXPECT_NO_THROW(client.Close());
}

// ===========================================================================
// NetworkStream
// ===========================================================================

TEST(NetworkStreamTests, CanRead_True) {
    // Use a socket pair so we have valid fds.
    int fds[2];
    ASSERT_EQ(0, ::socketpair(AF_UNIX, SOCK_STREAM, 0, fds));
    NetworkStream ns(fds[0]);
    EXPECT_TRUE(ns.getCanReadProperty());
    ::close(fds[1]);
    ns.Close();
}

TEST(NetworkStreamTests, CanWrite_True) {
    int fds[2];
    ASSERT_EQ(0, ::socketpair(AF_UNIX, SOCK_STREAM, 0, fds));
    NetworkStream ns(fds[0]);
    EXPECT_TRUE(ns.getCanWriteProperty());
    ::close(fds[1]);
    ns.Close();
}

TEST(NetworkStreamTests, ReadWrite_RoundTrip) {
    int fds[2];
    ASSERT_EQ(0, ::socketpair(AF_UNIX, SOCK_STREAM, 0, fds));
    NetworkStream writer(fds[0]);
    NetworkStream reader(fds[1]);

    SharpRuntime::bytecs out[3] = {0xAA, 0xBB, 0xCC};
    writer.Write(out, 0, 3);

    SharpRuntime::bytecs in[3] = {};
    auto n = reader.Read(in, 0, 3);
    EXPECT_EQ(3, n);
    EXPECT_EQ(0xAA, in[0]);
    EXPECT_EQ(0xBB, in[1]);
    EXPECT_EQ(0xCC, in[2]);
}

TEST(NetworkStreamTests, GetLength_Throws) {
    int fds[2];
    ASSERT_EQ(0, ::socketpair(AF_UNIX, SOCK_STREAM, 0, fds));
    NetworkStream ns(fds[0]);
    EXPECT_THROW((void)ns.getLengthProperty(), System::NotSupportedException);
    ::close(fds[1]);
}

TEST(NetworkStreamTests, CanRead_False_AfterClose) {
    int fds[2];
    ASSERT_EQ(0, ::socketpair(AF_UNIX, SOCK_STREAM, 0, fds));
    NetworkStream ns(fds[0]);
    ::close(fds[1]);
    ns.Close();
    EXPECT_FALSE(ns.getCanReadProperty());
}
