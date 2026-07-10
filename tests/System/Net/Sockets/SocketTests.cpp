// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <thread>
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Net/IPEndPoint.hpp"
#include "System/Net/Sockets/Socket.hpp"

using namespace System::Net::Sockets;
using System::Net::IPAddress;
using System::Net::IPEndPoint;

TEST(SocketTests, ConstructAndProperties) {
    Socket socket(AddressFamily::InterNetwork, SocketType::Stream, ProtocolType::Tcp);
    EXPECT_EQ(socket.getAddressFamilyProperty(), AddressFamily::InterNetwork);
    EXPECT_EQ(socket.getSocketTypeProperty(), SocketType::Stream);
    EXPECT_EQ(socket.getProtocolTypeProperty(), ProtocolType::Tcp);
    EXPECT_FALSE(socket.getConnectedProperty());
    EXPECT_FALSE(socket.getIsBoundProperty());
    EXPECT_GE(socket.getHandleProperty(), 0);
}

TEST(SocketTests, TcpBindListenAcceptConnectSendReceive) {
    Socket listener(AddressFamily::InterNetwork, SocketType::Stream, ProtocolType::Tcp);
    listener.Bind(IPEndPoint(IPAddress::Loopback, 0));
    listener.Listen();
    EXPECT_TRUE(listener.getIsBoundProperty());

    auto local = listener.getLocalEndPointProperty();
    ASSERT_NE(local, nullptr);
    auto* localIp = dynamic_cast<IPEndPoint*>(local.get());
    ASSERT_NE(localIp, nullptr);
    intcs port = localIp->getPortProperty();
    ASSERT_GT(port, 0);

    std::shared_ptr<Socket> acceptedSocket;
    std::thread serverThread([&]() { acceptedSocket = listener.Accept(); });

    Socket client(AddressFamily::InterNetwork, SocketType::Stream, ProtocolType::Tcp);
    client.Connect(IPAddress::Loopback, port);
    serverThread.join();

    ASSERT_NE(acceptedSocket, nullptr);
    EXPECT_TRUE(client.getConnectedProperty());

    std::vector<SharpRuntime::bytecs> sendBuf{'h', 'i', '!'};
    intcs sent = client.Send(sendBuf);
    EXPECT_EQ(sent, 3);

    std::vector<SharpRuntime::bytecs> recvBuf(16);
    intcs received = acceptedSocket->Receive(recvBuf);
    ASSERT_EQ(received, 3);
    EXPECT_EQ(recvBuf[0], 'h');
    EXPECT_EQ(recvBuf[1], 'i');
    EXPECT_EQ(recvBuf[2], '!');

    client.Close();
    acceptedSocket->Close();
}

// Regression test for a wave-3 audit finding: Send/Receive/SendTo/ReceiveFrom used to do
// buffer.data() + offset with no bounds check against buffer.size() -- a genuine
// out-of-bounds heap read (Send) or write (Receive) when offset+count exceeded the buffer,
// instead of a clean ArgumentOutOfRangeException. Verified against Socket.Tasks.cs's
// ValidateBufferArguments.
TEST(SocketTests, Send_OffsetCountOutOfRange_Throws) {
    Socket listener(AddressFamily::InterNetwork, SocketType::Stream, ProtocolType::Tcp);
    listener.Bind(IPEndPoint(IPAddress::Loopback, 0));
    listener.Listen();
    auto local = listener.getLocalEndPointProperty();
    auto* localIp = dynamic_cast<IPEndPoint*>(local.get());
    intcs port = localIp->getPortProperty();

    std::shared_ptr<Socket> acceptedSocket;
    std::thread serverThread([&]() { acceptedSocket = listener.Accept(); });
    Socket client(AddressFamily::InterNetwork, SocketType::Stream, ProtocolType::Tcp);
    client.Connect(IPAddress::Loopback, port);
    serverThread.join();

    std::vector<SharpRuntime::bytecs> buf{'a', 'b', 'c'};
    EXPECT_THROW(client.Send(buf, 2, 5, SocketFlags::None), System::ArgumentOutOfRangeException); // offset+count > size
    EXPECT_THROW(client.Send(buf, 10, 1, SocketFlags::None), System::ArgumentOutOfRangeException); // offset > size
    EXPECT_THROW(client.Send(buf, -1, 1, SocketFlags::None), System::ArgumentOutOfRangeException); // negative offset
    EXPECT_THROW(client.Send(buf, 0, -1, SocketFlags::None), System::ArgumentOutOfRangeException); // negative count

    client.Close();
    acceptedSocket->Close();
}

TEST(SocketTests, Receive_OffsetCountOutOfRange_Throws) {
    Socket listener(AddressFamily::InterNetwork, SocketType::Stream, ProtocolType::Tcp);
    listener.Bind(IPEndPoint(IPAddress::Loopback, 0));
    listener.Listen();
    auto local = listener.getLocalEndPointProperty();
    auto* localIp = dynamic_cast<IPEndPoint*>(local.get());
    intcs port = localIp->getPortProperty();

    std::shared_ptr<Socket> acceptedSocket;
    std::thread serverThread([&]() { acceptedSocket = listener.Accept(); });
    Socket client(AddressFamily::InterNetwork, SocketType::Stream, ProtocolType::Tcp);
    client.Connect(IPAddress::Loopback, port);
    serverThread.join();

    std::vector<SharpRuntime::bytecs> buf(4);
    EXPECT_THROW(client.Receive(buf, 2, 5, SocketFlags::None), System::ArgumentOutOfRangeException);
    EXPECT_THROW(client.Receive(buf, -1, 1, SocketFlags::None), System::ArgumentOutOfRangeException);

    client.Close();
    acceptedSocket->Close();
}

TEST(SocketTests, UdpSendToReceiveFrom) {
    Socket receiver(AddressFamily::InterNetwork, SocketType::Dgram, ProtocolType::Udp);
    receiver.Bind(IPEndPoint(IPAddress::Loopback, 0));
    auto local = receiver.getLocalEndPointProperty();
    auto* localIp = dynamic_cast<IPEndPoint*>(local.get());
    ASSERT_NE(localIp, nullptr);
    intcs port = localIp->getPortProperty();

    Socket sender(AddressFamily::InterNetwork, SocketType::Dgram, ProtocolType::Udp);
    std::vector<SharpRuntime::bytecs> payload{9, 8, 7};
    IPEndPoint dest(IPAddress::Loopback, port);
    intcs sent = sender.SendTo(payload, dest);
    EXPECT_EQ(sent, 3);

    std::vector<SharpRuntime::bytecs> recvBuf(16);
    IPEndPoint anyTemplate;
    SocketReceiveFromResult result = receiver.ReceiveFrom(recvBuf, anyTemplate);
    EXPECT_EQ(result.ReceivedBytes, 3);
    ASSERT_NE(result.RemoteEndPoint, nullptr);
    auto* remoteIp = dynamic_cast<System::Net::IPEndPoint*>(result.RemoteEndPoint.get());
    ASSERT_NE(remoteIp, nullptr);
    EXPECT_EQ(remoteIp->getAddressProperty(), IPAddress::Loopback);
}

TEST(SocketTests, BlockingAndTimeoutProperties) {
    Socket socket(AddressFamily::InterNetwork, SocketType::Dgram, ProtocolType::Udp);
    EXPECT_TRUE(socket.getBlockingProperty());
    socket.setBlockingProperty(false);
    EXPECT_FALSE(socket.getBlockingProperty());

    socket.setReceiveTimeoutProperty(1234);
    // Linux rounds SO_RCVTIMEO to whole milliseconds via timeval; just verify it's set and sane.
    EXPECT_GE(socket.getReceiveTimeoutProperty(), 0);
}

TEST(SocketTests, SetSocketOption_ReuseAddress) {
    Socket socket(AddressFamily::InterNetwork, SocketType::Stream, ProtocolType::Tcp);
    socket.SetSocketOption(SocketOptionLevel::Socket, SocketOptionName::ReuseAddress, true);
    EXPECT_NE(socket.GetSocketOption(SocketOptionLevel::Socket, SocketOptionName::ReuseAddress), 0);
}

TEST(SocketTests, LingerState) {
    Socket socket(AddressFamily::InterNetwork, SocketType::Stream, ProtocolType::Tcp);
    LingerOption linger(true, 5);
    socket.setLingerStateProperty(linger);
    LingerOption readBack = socket.getLingerStateProperty();
    EXPECT_TRUE(readBack.getEnabledProperty());
    EXPECT_EQ(readBack.getLingerTimeProperty(), 5);
}

TEST(SocketTests, NoDelayProperty) {
    Socket socket(AddressFamily::InterNetwork, SocketType::Stream, ProtocolType::Tcp);
    socket.setNoDelayProperty(true);
    EXPECT_TRUE(socket.getNoDelayProperty());
    socket.setNoDelayProperty(false);
    EXPECT_FALSE(socket.getNoDelayProperty());
}

TEST(SocketTests, PollDetectsReadable) {
    Socket receiver(AddressFamily::InterNetwork, SocketType::Dgram, ProtocolType::Udp);
    receiver.Bind(IPEndPoint(IPAddress::Loopback, 0));
    auto local = std::dynamic_pointer_cast<IPEndPoint>(receiver.getLocalEndPointProperty());
    ASSERT_NE(local, nullptr);

    EXPECT_FALSE(receiver.Poll(1000, SelectMode::SelectRead));

    Socket sender(AddressFamily::InterNetwork, SocketType::Dgram, ProtocolType::Udp);
    std::vector<SharpRuntime::bytecs> payload{1};
    sender.SendTo(payload, *local);

    EXPECT_TRUE(receiver.Poll(2'000'000, SelectMode::SelectRead));
}

TEST(SocketTests, AcceptAsyncAndConnectAsync) {
    Socket listener(AddressFamily::InterNetwork, SocketType::Stream, ProtocolType::Tcp);
    listener.Bind(IPEndPoint(IPAddress::Loopback, 0));
    listener.Listen();
    auto local = std::dynamic_pointer_cast<IPEndPoint>(listener.getLocalEndPointProperty());
    ASSERT_NE(local, nullptr);

    auto acceptTask = listener.AcceptAsync();

    Socket client(AddressFamily::InterNetwork, SocketType::Stream, ProtocolType::Tcp);
    auto connectTask = client.ConnectAsync(*local);
    EXPECT_TRUE(connectTask.getResultProperty());

    auto acceptedSocket = acceptTask.getResultProperty();
    ASSERT_NE(acceptedSocket, nullptr);
}

TEST(SocketTests, StaticOSSupports) {
    EXPECT_TRUE(Socket::getOSSupportsIPv4Property());
}

TEST(SocketTests, MoveConstructor) {
    Socket original(AddressFamily::InterNetwork, SocketType::Dgram, ProtocolType::Udp);
    intcs originalHandle = original.getHandleProperty();
    Socket moved(std::move(original));
    EXPECT_EQ(moved.getHandleProperty(), originalHandle);
}
