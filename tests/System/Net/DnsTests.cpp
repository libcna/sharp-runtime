// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/Net/Dns.hpp"
#include "System/Net/IPHostEntry.hpp"
#include "System/Net/Sockets/SocketError.hpp"
#include "System/Net/Sockets/SocketException.hpp"

using System::Net::Dns;
using System::Net::IPAddress;
using System::Net::IPHostEntry;
using System::Net::Sockets::AddressFamily;
using System::Net::Sockets::SocketError;
using System::Net::Sockets::SocketException;

// ===========================================================================
// IPHostEntry
// ===========================================================================

TEST(IPHostEntryTests, DefaultConstructor_EmptyFields) {
    IPHostEntry entry;
    EXPECT_EQ(entry.getHostNameProperty(), "");
    EXPECT_TRUE(entry.getAliasesProperty().empty());
    EXPECT_TRUE(entry.getAddressListProperty().empty());
}

TEST(IPHostEntryTests, Setters_UpdateFields) {
    IPHostEntry entry;
    entry.setHostNameProperty("example.com");
    entry.setAliasesProperty({"www.example.com"});
    entry.setAddressListProperty({IPAddress::Loopback});
    EXPECT_EQ(entry.getHostNameProperty(), "example.com");
    EXPECT_EQ(entry.getAliasesProperty().size(), 1u);
    EXPECT_EQ(entry.getAddressListProperty().size(), 1u);
    EXPECT_EQ(entry.getAddressListProperty()[0], IPAddress::Loopback);
}

// ===========================================================================
// SocketError / SocketException
// ===========================================================================

TEST(SocketErrorTests, Success_IsZero) {
    EXPECT_EQ(static_cast<int>(SocketError::Success), 0);
}

TEST(SocketExceptionTests, Constructor_StoresErrorCode) {
    SocketException ex(SocketError::HostNotFound);
    EXPECT_EQ(ex.getSocketErrorCodeProperty(), SocketError::HostNotFound);
}

TEST(SocketExceptionTests, IntConstructor_StoresErrorCode) {
    SocketException ex(static_cast<SharpRuntime::intcs>(SocketError::TimedOut));
    EXPECT_EQ(ex.getSocketErrorCodeProperty(), SocketError::TimedOut);
}

// ===========================================================================
// Dns
// ===========================================================================

TEST(DnsTests, GetHostName_ReturnsNonEmptyString) {
    std::string name = Dns::GetHostName();
    EXPECT_FALSE(name.empty());
}

TEST(DnsTests, GetHostAddresses_LiteralIPv4_ReturnsSameAddress) {
    auto addrs = Dns::GetHostAddresses("127.0.0.1");
    ASSERT_EQ(addrs.size(), 1u);
    EXPECT_EQ(addrs[0], IPAddress::Loopback);
}

TEST(DnsTests, GetHostAddresses_RequestingIPv6Only_ReturnsEmpty) {
    auto addrs = Dns::GetHostAddresses("127.0.0.1", AddressFamily::InterNetworkV6);
    EXPECT_TRUE(addrs.empty());
}

TEST(DnsTests, GetHostEntry_LiteralIPv4_ReturnsEntryWithSameAddress) {
    IPHostEntry entry = Dns::GetHostEntry("127.0.0.1");
    ASSERT_FALSE(entry.getAddressListProperty().empty());
    EXPECT_EQ(entry.getAddressListProperty()[0], IPAddress::Loopback);
}

TEST(DnsTests, GetHostEntry_ByAddress_Loopback_ResolvesSomeName) {
    IPHostEntry entry = Dns::GetHostEntry(IPAddress::Loopback);
    EXPECT_FALSE(entry.getHostNameProperty().empty());
}

TEST(DnsTests, GetHostAddresses_UnresolvableHost_Throws) {
    EXPECT_THROW(Dns::GetHostAddresses("this-host-should-not-exist.invalid"), SocketException);
}
