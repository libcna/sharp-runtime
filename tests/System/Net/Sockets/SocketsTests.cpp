// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Note: TcpClient, TcpListener, UdpClient are stubs — all network methods throw
// NotImplementedException (awaiting POSIX sockets or Winsock2 integration).
#include <gtest/gtest.h>
#include <vector>
#include "System/NotImplementedException.hpp"
#include "System/Net/IPAddress.hpp"
#include "System/Net/IPEndPoint.hpp"
#include "System/Net/Sockets/TcpClient.hpp"
#include "System/Net/Sockets/UdpClient.hpp"

using System::NotImplementedException;
using System::Net::IPAddress;
using System::Net::IPEndPoint;
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

TEST(TcpClientTests, Connect_HostPort_ThrowsNotImplemented) {
    TcpClient client;
    EXPECT_THROW(client.Connect("localhost", 80), NotImplementedException);
}

TEST(TcpClientTests, Connect_EndPoint_ThrowsNotImplemented) {
    TcpClient client;
    IPEndPoint ep(IPAddress::Parse("127.0.0.1"), 80);
    EXPECT_THROW(client.Connect(ep), NotImplementedException);
}

TEST(TcpClientTests, Close_NoThrow) {
    TcpClient client;
    EXPECT_NO_THROW(client.Close());
}

TEST(TcpClientTests, Connected_ReturnsFalse) {
    TcpClient client;
    EXPECT_FALSE(client.getConnectedProperty());
}

TEST(TcpClientTests, Available_ThrowsNotImplemented) {
    TcpClient client;
    EXPECT_THROW((void)client.Available(), NotImplementedException);
}

// ===========================================================================
// TcpListener
// ===========================================================================

TEST(TcpListenerTests, EndPointConstructor_NoThrow) {
    IPEndPoint ep(IPAddress::Parse("0.0.0.0"), 9090);
    EXPECT_NO_THROW(TcpListener{ep});
}

TEST(TcpListenerTests, AddressPortConstructor_NoThrow) {
    IPAddress addr = IPAddress::Parse("0.0.0.0");
    EXPECT_NO_THROW(TcpListener(addr, 9090));
}

TEST(TcpListenerTests, Start_ThrowsNotImplemented) {
    IPEndPoint ep(IPAddress::Parse("0.0.0.0"), 9090);
    TcpListener listener(ep);
    EXPECT_THROW(listener.Start(), NotImplementedException);
}

TEST(TcpListenerTests, AcceptTcpClient_ThrowsNotImplemented) {
    IPEndPoint ep(IPAddress::Parse("0.0.0.0"), 9090);
    TcpListener listener(ep);
    EXPECT_THROW((void)listener.AcceptTcpClient(), NotImplementedException);
}

TEST(TcpListenerTests, Stop_NoThrow) {
    IPEndPoint ep(IPAddress::Parse("0.0.0.0"), 9090);
    TcpListener listener(ep);
    EXPECT_NO_THROW(listener.Stop());
}

// ===========================================================================
// UdpClient
// ===========================================================================

TEST(UdpClientTests, DefaultConstructor_NoThrow) {
    EXPECT_NO_THROW(UdpClient{});
}

TEST(UdpClientTests, PortConstructor_NoThrow) {
    EXPECT_NO_THROW(UdpClient{5000});
}

TEST(UdpClientTests, EndPointConstructor_NoThrow) {
    IPEndPoint ep(IPAddress::Parse("0.0.0.0"), 5000);
    EXPECT_NO_THROW(UdpClient{ep});
}

TEST(UdpClientTests, Connect_HostPort_ThrowsNotImplemented) {
    UdpClient client;
    EXPECT_THROW(client.Connect("localhost", 5000), NotImplementedException);
}

TEST(UdpClientTests, Connect_EndPoint_ThrowsNotImplemented) {
    UdpClient client;
    IPEndPoint ep(IPAddress::Parse("127.0.0.1"), 5000);
    EXPECT_THROW(client.Connect(ep), NotImplementedException);
}

TEST(UdpClientTests, Send_ThrowsNotImplemented) {
    UdpClient client;
    std::vector<SharpRuntime::bytecs> data = {0x01, 0x02, 0x03};
    EXPECT_THROW((void)client.Send(data, 3), NotImplementedException);
}

TEST(UdpClientTests, Receive_ThrowsNotImplemented) {
    UdpClient client;
    IPEndPoint ep(IPAddress::Parse("0.0.0.0"), 0);
    EXPECT_THROW((void)client.Receive(ep), NotImplementedException);
}

TEST(UdpClientTests, Close_NoThrow) {
    UdpClient client;
    EXPECT_NO_THROW(client.Close());
}

TEST(UdpClientTests, ClientProperty_ReturnsFalse) {
    UdpClient client;
    EXPECT_FALSE(client.getClientProperty());
}
