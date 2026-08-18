// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/Net/Dns.hpp"
#include "System/Net/IPAddress.hpp"
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

// Regression test for a wave-3 audit finding: requesting AddressFamily::InterNetworkV6 for
// any host unconditionally returned an empty result -- IPv6 resolution was effectively
// disabled entirely, silently, with no distinction between "no IPv6 address exists" and
// "IPv6 support is turned off". Asking for an IPv6-only resolution of an IPv4-only literal
// address has no valid answer, so it now surfaces as a SocketException(HostNotFound) --
// consistent with how every other unresolvable-address case in this API already behaves --
// instead of a silent empty vector indistinguishable from "checked and found nothing".
TEST(DnsTests, GetHostAddresses_RequestingIPv6Only_MismatchedIPv4Literal_Throws) {
    EXPECT_THROW(Dns::GetHostAddresses("127.0.0.1", AddressFamily::InterNetworkV6), SocketException);
}

TEST(DnsTests, GetHostAddresses_LiteralIPv6_ReturnsSameAddress) {
    auto addrs = Dns::GetHostAddresses("::1");
    ASSERT_FALSE(addrs.empty());
    for (const auto& a : addrs) {
        EXPECT_TRUE(a.getIsIPv6Property());
        EXPECT_EQ(a, IPAddress::IPv6Loopback);
    }
}

TEST(DnsTests, GetHostAddresses_RequestingIPv6_LiteralIPv6_ReturnsAddress) {
    auto addrs = Dns::GetHostAddresses("::1", AddressFamily::InterNetworkV6);
    ASSERT_FALSE(addrs.empty());
    EXPECT_EQ(addrs[0], IPAddress::IPv6Loopback);
}

// Verified against the real getaddrinfo() behavior of this build host: requesting AF_INET6
// for the numeric literal "127.0.0.1" fails with EAI_ADDRFAMILY, and requesting AF_INET for
// the numeric literal "::1" is likewise rejected -- an IPv4-only request for an IPv6 literal
// has no valid answer either.
TEST(DnsTests, GetHostAddresses_RequestingIPv4Only_MismatchedIPv6Literal_Throws) {
    EXPECT_THROW(Dns::GetHostAddresses("::1", AddressFamily::InterNetwork), SocketException);
}

TEST(DnsTests, GetHostEntry_LiteralIPv4_ReturnsEntryWithSameAddress) {
    IPHostEntry entry = Dns::GetHostEntry("127.0.0.1");
    ASSERT_FALSE(entry.getAddressListProperty().empty());
    EXPECT_EQ(entry.getAddressListProperty()[0], IPAddress::Loopback);
}

TEST(DnsTests, GetHostEntry_LiteralIPv6_ReturnsEntryWithSameAddress) {
    IPHostEntry entry = Dns::GetHostEntry("::1", AddressFamily::InterNetworkV6);
    ASSERT_FALSE(entry.getAddressListProperty().empty());
    EXPECT_EQ(entry.getAddressListProperty()[0], IPAddress::IPv6Loopback);
}

TEST(DnsTests, GetHostEntry_ByAddress_Loopback_ResolvesSomeName) {
    IPHostEntry entry = Dns::GetHostEntry(IPAddress::Loopback);
    EXPECT_FALSE(entry.getHostNameProperty().empty());
}

// Regression test: GetHostEntry(const IPAddress&) previously called address.getAddressProperty()
// unconditionally, which throws SocketException for any IPv6 address (per its documented
// contract) -- there was no code path at all for a reverse lookup of an IPv6 address.
TEST(DnsTests, GetHostEntry_ByAddress_IPv6Loopback_ResolvesSomeName) {
    IPHostEntry entry = Dns::GetHostEntry(IPAddress::IPv6Loopback);
    EXPECT_FALSE(entry.getHostNameProperty().empty());
}

// Ticket #1961: GetHostEntry(const IPAddress&) unconditionally fed getnameinfo's output back
// into GetHostEntry(string, family). getnameinfo() without NI_NAMEREQD does not fail when an
// address has no reverse mapping -- it SUCCEEDS and writes the address back in numeric form --
// so the string overload re-parsed it as a literal and called straight back in with the same
// address. Unbounded mutual recursion, stack overflow, SIGSEGV, from ordinary public input.
//
// Measured on a container whose /etc/hosts has `127.0.0.1 localhost` but no `::1` line:
// getnameinfo(127.0.0.1) returned "localhost" and terminated, getnameinfo(::1) returned "::1"
// and killed the process -- which is why only the IPv6 case crashed.
//
// The addresses below are the RFC 5737 TEST-NET-1 and RFC 3849 documentation ranges, which are
// reserved precisely so that they resolve to nothing anywhere, so this exercises the
// no-reverse-mapping branch even on a host that does have an ::1 entry.
//
// CORRECTION (ticket #2375, 2026-08-18). This used to require that the call RETURN an entry,
// which is a third possible outcome the resolver gets to choose among, not a property of this
// port. Caught by the full gate: one run raised `Temporary failure in name resolution`
// (EAI_AGAIN) for 192.0.2.1 while five isolated runs passed. The finding is about
// TERMINATION -- an unbounded mutual recursion overflows the stack and takes the process
// down, so it can neither return nor throw -- and termination is what is asserted now.
// A resolver that answers, one that says "no such name", and one that is briefly unreachable
// are all acceptable; a stack overflow is not.
TEST(DnsTests, GetHostEntry_ByAddress_WithNoReverseMapping_TerminatesInsteadOfRecursing) {
    const auto terminatesForAddressLiteral = [](const char* text) {
        const IPAddress address = IPAddress::Parse(text);
        try {
            const IPHostEntry entry = Dns::GetHostEntry(address);
            // The resolver answered. Whatever it said, the entry must be well formed.
            EXPECT_FALSE(entry.getHostNameProperty().empty()) << text;
            if (entry.getHostNameProperty() == address.ToString()) {
                // No reverse mapping: the port reports the address it was asked about,
                // exactly once, rather than calling back in with it.
                ASSERT_EQ(entry.getAddressListProperty().size(), 1u) << text;
                EXPECT_EQ(entry.getAddressListProperty()[0], address) << text;
            }
        } catch (const SocketException&) {
            // The resolver declined, or was momentarily unreachable. Reaching this line is
            // itself the assertion: the call came back.
            SUCCEED();
        }
    };
    terminatesForAddressLiteral("192.0.2.1");
    terminatesForAddressLiteral("2001:db8::1");
}

// The same termination guarantee for the loopback addresses the suite already uses, stated as
// its own case so that a regression shows up as a failure here rather than as a crash that
// takes the whole executable -- and every test after it -- down with it.
TEST(DnsTests, GetHostEntry_ByAddress_Loopback_TerminatesForBothFamilies) {
    EXPECT_FALSE(Dns::GetHostEntry(IPAddress::Loopback).getHostNameProperty().empty());
    EXPECT_FALSE(Dns::GetHostEntry(IPAddress::IPv6Loopback).getHostNameProperty().empty());
}

TEST(DnsTests, GetHostAddresses_UnresolvableHost_Throws) {
    EXPECT_THROW(Dns::GetHostAddresses("this-host-should-not-exist.invalid"), SocketException);
}
