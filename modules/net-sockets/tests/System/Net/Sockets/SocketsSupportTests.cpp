// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <limits>
#include "System/ArgumentException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Net/Sockets/IPPacketInformation.hpp"
#include "System/Net/Sockets/LingerOption.hpp"
#include "System/Net/Sockets/MulticastOption.hpp"
#include "System/Net/Sockets/ProtocolType.hpp"
#include "System/Net/Sockets/SelectMode.hpp"
#include "System/Net/Sockets/SendPacketsElement.hpp"
#include "System/Net/Sockets/SocketFlags.hpp"
#include "System/Net/Sockets/SocketOptionLevel.hpp"
#include "System/Net/Sockets/SocketOptionName.hpp"
#include "System/Net/Sockets/SocketReceiveFromResult.hpp"
#include "System/Net/Sockets/SocketReceiveMessageFromResult.hpp"
#include "System/Net/Sockets/SocketShutdown.hpp"
#include "System/Net/Sockets/SocketType.hpp"
#include "System/Net/Sockets/UdpReceiveResult.hpp"
#include "System/Net/Sockets/UnixDomainSocketEndPoint.hpp"

using namespace System::Net::Sockets;
using System::Net::IPAddress;
using System::Net::IPEndPoint;

// --- Enums -----------------------------------------------------------------------------------

TEST(ProtocolTypeTests, Values) {
    EXPECT_EQ(static_cast<int>(ProtocolType::Tcp), 6);
    EXPECT_EQ(static_cast<int>(ProtocolType::Udp), 17);
    EXPECT_EQ(static_cast<int>(ProtocolType::Unknown), -1);
}

TEST(SelectModeTests, Values) {
    EXPECT_EQ(static_cast<int>(SelectMode::SelectRead), 0);
    EXPECT_EQ(static_cast<int>(SelectMode::SelectWrite), 1);
    EXPECT_EQ(static_cast<int>(SelectMode::SelectError), 2);
}

TEST(SocketShutdownTests, Values) {
    EXPECT_EQ(static_cast<int>(SocketShutdown::Receive), 0x00);
    EXPECT_EQ(static_cast<int>(SocketShutdown::Send), 0x01);
    EXPECT_EQ(static_cast<int>(SocketShutdown::Both), 0x02);
}

TEST(SocketTypeTests, Values) {
    EXPECT_EQ(static_cast<int>(SocketType::Stream), 1);
    EXPECT_EQ(static_cast<int>(SocketType::Dgram), 2);
    EXPECT_EQ(static_cast<int>(SocketType::Unknown), -1);
}

TEST(SocketFlagsTests, BitwiseOperators) {
    auto combined = SocketFlags::Peek | SocketFlags::DontRoute;
    EXPECT_EQ(static_cast<int>(combined), 0x0006);
    EXPECT_EQ(combined & SocketFlags::Peek, SocketFlags::Peek);
}

TEST(SocketOptionLevelTests, Values) {
    EXPECT_EQ(static_cast<int>(SocketOptionLevel::Socket), 0xffff);
    EXPECT_EQ(static_cast<int>(SocketOptionLevel::Tcp), 6);
    EXPECT_EQ(static_cast<int>(SocketOptionLevel::Udp), 17);
}

TEST(SocketOptionNameTests, Values) {
    EXPECT_EQ(static_cast<int>(SocketOptionName::ReuseAddress), 0x0004);
    EXPECT_EQ(static_cast<int>(SocketOptionName::Linger), 0x0080);
    EXPECT_EQ(static_cast<int>(SocketOptionName::NoDelay), 1);
}

// --- Structs ---------------------------------------------------------------------------------

TEST(IPPacketInformationTests, BasicAccessors) {
    IPPacketInformation info(IPAddress::Loopback, 3);
    EXPECT_EQ(info.getAddressProperty(), IPAddress::Loopback);
    EXPECT_EQ(info.getInterfaceProperty(), 3);
    EXPECT_EQ(info, IPPacketInformation(IPAddress::Loopback, 3));
    EXPECT_NE(info, IPPacketInformation(IPAddress::Loopback, 4));
}

TEST(IPPacketInformationTests, GetHashCode_EqualInstancesMatch) {
    IPPacketInformation a(IPAddress::Loopback, 3);
    IPPacketInformation b(IPAddress::Loopback, 3);
    EXPECT_EQ(a.GetHashCode(), b.GetHashCode());
}

TEST(IPPacketInformationTests, GetHashCode_DifferingInterfaceDiffers) {
    IPPacketInformation a(IPAddress::Loopback, 3);
    IPPacketInformation b(IPAddress::Loopback, 4);
    EXPECT_NE(a.GetHashCode(), b.GetHashCode());
}

TEST(SocketReceiveFromResultTests, DefaultAndAssign) {
    SocketReceiveFromResult result;
    result.ReceivedBytes = 42;
    EXPECT_EQ(result.ReceivedBytes, 42);
    EXPECT_EQ(result.RemoteEndPoint, nullptr);
}

TEST(SocketReceiveMessageFromResultTests, DefaultAndAssign) {
    SocketReceiveMessageFromResult result;
    result.ReceivedBytes = 10;
    result.SocketFlags = SocketFlags::Truncated;
    EXPECT_EQ(result.ReceivedBytes, 10);
    EXPECT_EQ(result.SocketFlags, SocketFlags::Truncated);
}

TEST(UdpReceiveResultTests, BasicAccessors) {
    std::vector<SharpRuntime::bytecs> buffer{1, 2, 3};
    IPEndPoint remote(IPAddress::Loopback, 1234);
    UdpReceiveResult result(buffer, remote);
    EXPECT_EQ(result.getBufferProperty(), buffer);
    EXPECT_EQ(result.getRemoteEndPointProperty(), remote);
    EXPECT_EQ(result, UdpReceiveResult(buffer, remote));
}

TEST(UdpReceiveResultTests, GetHashCode_EqualInstancesMatch) {
    std::vector<SharpRuntime::bytecs> buffer{1, 2, 3};
    IPEndPoint remote(IPAddress::Loopback, 1234);
    UdpReceiveResult a(buffer, remote);
    UdpReceiveResult b(buffer, remote);
    EXPECT_EQ(a.GetHashCode(), b.GetHashCode());
}

TEST(UdpReceiveResultTests, GetHashCode_DifferingBufferDiffers) {
    IPEndPoint remote(IPAddress::Loopback, 1234);
    UdpReceiveResult a(std::vector<SharpRuntime::bytecs>{1, 2, 3}, remote);
    UdpReceiveResult b(std::vector<SharpRuntime::bytecs>{4, 5, 6}, remote);
    EXPECT_NE(a.GetHashCode(), b.GetHashCode());
}

// --- LingerOption / MulticastOption -----------------------------------------------------------

TEST(LingerOptionTests, BasicAccessors) {
    LingerOption option(true, 30);
    EXPECT_TRUE(option.getEnabledProperty());
    EXPECT_EQ(option.getLingerTimeProperty(), 30);
    option.setEnabledProperty(false);
    option.setLingerTimeProperty(10);
    EXPECT_FALSE(option.getEnabledProperty());
    EXPECT_EQ(option.getLingerTimeProperty(), 10);
    EXPECT_EQ(option, LingerOption(false, 10));
}

TEST(LingerOptionTests, GetHashCode_EqualInstancesMatch) {
    LingerOption a(true, 30);
    LingerOption b(true, 30);
    EXPECT_EQ(a.GetHashCode(), b.GetHashCode());
}

TEST(LingerOptionTests, GetHashCode_DifferingTimeDiffers) {
    LingerOption a(true, 30);
    LingerOption b(true, 45);
    EXPECT_NE(a.GetHashCode(), b.GetHashCode());
}

TEST(MulticastOptionTests, GroupOnly) {
    MulticastOption option(IPAddress::Loopback);
    EXPECT_EQ(option.getGroupProperty(), IPAddress::Loopback);
    EXPECT_EQ(option.getLocalAddressProperty(), IPAddress::Any);
}

TEST(MulticastOptionTests, WithInterfaceIndex) {
    MulticastOption option(IPAddress::Loopback, 5);
    EXPECT_EQ(option.getInterfaceIndexProperty(), 5);
}

TEST(MulticastOptionTests, NegativeInterfaceIndex_Throws) {
    EXPECT_THROW((MulticastOption(IPAddress::Loopback, -1)), System::ArgumentOutOfRangeException);
}

TEST(IPv6MulticastOptionTests, BasicAccessors) {
    IPv6MulticastOption option(IPAddress::IPv6Loopback, 7);
    EXPECT_EQ(option.getGroupProperty(), IPAddress::IPv6Loopback);
    EXPECT_EQ(option.getInterfaceIndexProperty(), 7);
}

// --- SendPacketsElement ------------------------------------------------------------------------

TEST(SendPacketsElementTests, FilePathElement) {
    SendPacketsElement element(std::string("/tmp/foo.bin"));
    ASSERT_TRUE(element.getFilePathProperty().has_value());
    EXPECT_EQ(*element.getFilePathProperty(), "/tmp/foo.bin");
    EXPECT_EQ(element.getCountProperty(), 0);
    EXPECT_FALSE(element.getEndOfPacketProperty());
}

TEST(SendPacketsElementTests, BufferElement_WholeBuffer) {
    std::vector<SharpRuntime::bytecs> buffer{1, 2, 3, 4};
    SendPacketsElement element(buffer);
    EXPECT_EQ(element.getBufferProperty(), buffer);
    EXPECT_EQ(element.getCountProperty(), 4);
    EXPECT_EQ(element.getOffsetProperty(), 0);
}

TEST(SendPacketsElementTests, BufferElement_PartialRange) {
    std::vector<SharpRuntime::bytecs> buffer{1, 2, 3, 4, 5};
    SendPacketsElement element(buffer, 1, 2, true);
    EXPECT_EQ(element.getCountProperty(), 2);
    EXPECT_EQ(element.getOffsetProperty(), 1);
    EXPECT_TRUE(element.getEndOfPacketProperty());
}

TEST(SendPacketsElementTests, BufferElement_OutOfRange_Throws) {
    std::vector<SharpRuntime::bytecs> buffer{1, 2, 3};
    EXPECT_THROW((SendPacketsElement(buffer, 0, 10)), System::ArgumentOutOfRangeException);
}

// --- UnixDomainSocketEndPoint ------------------------------------------------------------------

TEST(UnixDomainSocketEndPointTests, BasicPath) {
    UnixDomainSocketEndPoint ep("/tmp/my.sock");
    EXPECT_EQ(ep.getPathProperty(), "/tmp/my.sock");
    EXPECT_EQ(ep.ToString(), "/tmp/my.sock");
    EXPECT_EQ(ep.getAddressFamilyProperty(), AddressFamily::Unix);
}

TEST(UnixDomainSocketEndPointTests, EmptyPath_Throws) {
    EXPECT_THROW(UnixDomainSocketEndPoint(std::string("")), System::ArgumentOutOfRangeException);
}

TEST(UnixDomainSocketEndPointTests, TooLongPath_Throws) {
    std::string longPath(200, 'a');
    EXPECT_THROW(UnixDomainSocketEndPoint{longPath}, System::ArgumentOutOfRangeException);
}

TEST(UnixDomainSocketEndPointTests, SerializeRoundTrip) {
    UnixDomainSocketEndPoint ep("/tmp/roundtrip.sock");
    auto address = ep.Serialize();
    auto recreated = ep.Create(address);
    auto* uds = dynamic_cast<UnixDomainSocketEndPoint*>(recreated.get());
    ASSERT_NE(uds, nullptr);
    EXPECT_EQ(uds->getPathProperty(), "/tmp/roundtrip.sock");
}

TEST(UnixDomainSocketEndPointTests, Equality) {
    UnixDomainSocketEndPoint a("/tmp/a.sock");
    UnixDomainSocketEndPoint b("/tmp/a.sock");
    UnixDomainSocketEndPoint c("/tmp/b.sock");
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

TEST(UnixDomainSocketEndPointTests, GetHashCode_EqualInstancesMatch) {
    UnixDomainSocketEndPoint a("/tmp/a.sock");
    UnixDomainSocketEndPoint b("/tmp/a.sock");
    EXPECT_EQ(a.GetHashCode(), b.GetHashCode());
}

TEST(UnixDomainSocketEndPointTests, GetHashCode_DifferingPathDiffers) {
    UnixDomainSocketEndPoint a("/tmp/a.sock");
    UnixDomainSocketEndPoint c("/tmp/b.sock");
    EXPECT_NE(a.GetHashCode(), c.GetHashCode());
}

// --- Ticket #2135 / SR-AUD-264 / cause NS-D -----------------------------------------------------
//
// A negative `count` used to mean "the whole buffer": SendPacketsElement({1,2,3}, 0, -2) was
// accepted with count == 3, and so were -1, -100 and INT_MIN. The cause was that
// `count >= 0 ? count : buffer_.size()` ran BEFORE any range check, so the sign was consumed by
// the ternary and never reached one.
//
// PREMISE CORRECTION (docs/SystemNetSocketsNamespaceReviewPlan.md §4.2): the negative OFFSET was
// already rejected, by the unsigned-cast idiom two lines below the ternary, and the FILE-PATH
// overload already called ThrowIfNegative. So this was one overload's ternary, not a type-wide gap.

TEST(SendPacketsElementCountTests, EveryNegativeCountIsRejected) {
    for (SharpRuntime::intcs count : {static_cast<SharpRuntime::intcs>(-1),
                                      static_cast<SharpRuntime::intcs>(-2),
                                      static_cast<SharpRuntime::intcs>(-100),
                                      std::numeric_limits<SharpRuntime::intcs>::min()}) {
        try {
            System::Net::Sockets::SendPacketsElement element(
                std::vector<SharpRuntime::bytecs>{1, 2, 3}, 0, count);
            ADD_FAILURE() << "count " << count << " was accepted";
        } catch (const System::ArgumentOutOfRangeException& e) {
            EXPECT_NE(std::string(e.what()).find("count"), std::string::npos) << e.what();
        }
    }
}

TEST(SendPacketsElementCountTests, THECONTROLTheValidCountsAreUnchanged) {
    // The whole-buffer meaning now lives in the one-argument constructor, where it is meant.
    System::Net::Sockets::SendPacketsElement whole(std::vector<SharpRuntime::bytecs>{1, 2, 3});
    EXPECT_EQ(whole.getCountProperty(), 3);

    for (SharpRuntime::intcs count : {0, 1, 3}) {
        System::Net::Sockets::SendPacketsElement element(
            std::vector<SharpRuntime::bytecs>{1, 2, 3}, 0, count);
        EXPECT_EQ(element.getCountProperty(), count);
    }
    // ...and the upper bound still rejects, so the repair did not remove the check it sits beside.
    EXPECT_THROW((void)System::Net::Sockets::SendPacketsElement(
                     std::vector<SharpRuntime::bytecs>{1, 2, 3}, 0, 4),
                 System::ArgumentOutOfRangeException);
    EXPECT_THROW((void)System::Net::Sockets::SendPacketsElement(
                     std::vector<SharpRuntime::bytecs>{1, 2, 3}, 4, 1),
                 System::ArgumentOutOfRangeException);
    EXPECT_THROW((void)System::Net::Sockets::SendPacketsElement(
                     std::vector<SharpRuntime::bytecs>{1, 2, 3}, 3, 1),
                 System::ArgumentOutOfRangeException);
}

TEST(SendPacketsElementCountTests, ARejectionNamesTheCallersValueNotItsUnsignedReinterpretation) {
    // The diagnostic defect neither the finding nor the review brief names: a caller who passed
    // offset = -1 was told "'offset' must be less than or equal to 3. Actual value was 4294967295."
    try {
        System::Net::Sockets::SendPacketsElement element(
            std::vector<SharpRuntime::bytecs>{1, 2, 3}, -1, 1);
        ADD_FAILURE() << "a negative offset was accepted";
    } catch (const System::ArgumentOutOfRangeException& e) {
        const std::string message = e.what();
        EXPECT_EQ(message.find("4294967295"), std::string::npos)
            << "the message must not report the unsigned reinterpretation: " << message;
        EXPECT_NE(message.find("-1"), std::string::npos) << message;
    }
}

TEST(SendPacketsElementCountTests, THEPINTheFilePathOverloadWasAlreadyCorrect) {
    EXPECT_THROW((void)System::Net::Sockets::SendPacketsElement("/x", 5, -1),
                 System::ArgumentOutOfRangeException);
    EXPECT_THROW((void)System::Net::Sockets::SendPacketsElement("/x", -1, 1),
                 System::ArgumentOutOfRangeException);
    System::Net::Sockets::SendPacketsElement ok("/x", 5, 1);
    EXPECT_EQ(ok.getOffsetLongProperty(), 5);
    EXPECT_EQ(ok.getCountProperty(), 1);
}
