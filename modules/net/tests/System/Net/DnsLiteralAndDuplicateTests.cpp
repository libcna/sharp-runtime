// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Ticket #2039 -- SR-AUD-304, cause N-C of docs/SystemNetNamespaceReviewPlan.md.
//
// Four defects at one door, measured before the repair
// (build-probe/2039_probe1_before_after.log):
//
//  1. A bespoke sscanf("%u.%u.%u.%u%c") IPv4 parser sat beside IPAddress::TryParse and
//     disagreed with it about VALID input, not merely invalid input: "0177.0.0.1" read as
//     DECIMAL 177.0.0.1 here while IPAddress::Parse of the same text yields OCTAL 127.0.0.1.
//     It also accepted "-0.0.0.1", "+1.2.3.4" and " 1.2.3.4" (%u takes a sign and skips
//     leading whitespace) and declined "1.2.3" entirely.
//  2. Duplicate results -- and NOT from that parser, which is what the review plan's cause N-C
//     said. Measured directly against getaddrinfo, hints.ai_socktype = 0 asks for one addrinfo
//     PER SOCKET TYPE, so EVERY answer came back three times: "localhost", "runsc", "vm",
//     "127.0.0.1" and "1.2.3" alike, and GetHostEntry(8.8.8.8) returned SIX entries for two
//     distinct addresses. Replacing the literal parser alone would have fixed the finding's one
//     named input by accident and left every resolved NAME still tripled.
//  3. The requested family was not applied to a literal: the two shortcuts were guarded by
//     `family != InterNetworkV6` / `family != InterNetwork`, so Unix, Unknown and Max all passed
//     both guards and got the literal back.
//  4. Every failure surfaced as the fabricated message "Win32 error 11001" on a platform with
//     no Winsock.
//
// These tests use only IP literals and names this container resolves from /etc/hosts, so none
// of them needs a network. The one place a real DNS answer would be required is guarded.
#include <gtest/gtest.h>
#include <algorithm>
#include <string>
#include <vector>
#include "System/Net/Dns.hpp"
#include "System/Net/IPAddress.hpp"
#include "System/Net/IPHostEntry.hpp"
#include "System/Net/Sockets/AddressFamily.hpp"
#include "System/Net/Sockets/SocketError.hpp"
#include "System/Net/Sockets/SocketException.hpp"

using System::Net::Dns;
using System::Net::IPAddress;
using System::Net::IPHostEntry;
using System::Net::Sockets::AddressFamily;
using System::Net::Sockets::SocketError;
using System::Net::Sockets::SocketException;

namespace {

    bool resolves(const std::string& host) {
        try {
            return !Dns::GetHostAddresses(host).empty();
        } catch (const SocketException&) {
            return false;
        }
    }

    bool hasDuplicates(const std::vector<IPAddress>& addresses) {
        for (size_t i = 0; i < addresses.size(); ++i)
            for (size_t j = i + 1; j < addresses.size(); ++j)
                if (addresses[i] == addresses[j]) return true;
        return false;
    }

} // namespace

// ---------------------------------------------------------------------------
// 1. One literal parser for the module.
// ---------------------------------------------------------------------------

TEST(DnsLiteralTests, SignedAndSpacedIPv4Text_IsRejected) {
    // "-0.0.0.1" is the finding's own case. "+1.2.3.4" and " 1.2.3.4" are the same %u
    // permissiveness and are not named by it.
    for (const char* text : {"-0.0.0.1", "-1.2.3.4", "+1.2.3.4", " 1.2.3.4"}) {
        EXPECT_THROW((void)Dns::GetHostAddresses(text), SocketException) << text;
    }
}

TEST(DnsLiteralTests, LiteralIsInterpretedExactlyAsIPAddressParseDoes) {
    // The sharp case: two parsers in one module returned two DIFFERENT addresses for one valid
    // literal. "0177.0.0.1" was 177.0.0.1 through Dns and 127.0.0.1 through IPAddress.
    struct Row { const char* text; const char* expected; };
    const Row rows[] = {
        {"0177.0.0.1", "127.0.0.1"},   // octal, as IPAddress::Parse has always read it
        {"0x7f.0.0.1", "127.0.0.1"},   // hexadecimal
        {"1.2.3",      "1.2.0.3"},     // three-part short form
        {"1.2",        "1.0.0.2"},     // two-part short form
        {"3232235777", "192.168.1.1"}, // single 32-bit value
        {"127.0.0.1",  "127.0.0.1"},
    };
    for (const Row& row : rows) {
        const std::vector<IPAddress> addresses = Dns::GetHostAddresses(row.text);
        ASSERT_EQ(addresses.size(), 1u) << row.text;
        EXPECT_EQ(addresses[0], IPAddress::Parse(row.expected)) << row.text;
        // ...and the two doors agree, which is the property that was violated.
        EXPECT_EQ(addresses[0], IPAddress::Parse(row.text)) << row.text;
    }
}

TEST(DnsLiteralTests, MalformedLiteralTextIsStillRejected) {
    for (const char* text : {"1.2.3.4.5", "1.2.3.", "256.1.1.1", "1.2.3.4 ", "", "999.999.999.999"}) {
        EXPECT_THROW((void)Dns::GetHostAddresses(text), SocketException) << text;
    }
}

// ---------------------------------------------------------------------------
// 2. Duplicate results -- literals AND names.
// ---------------------------------------------------------------------------

TEST(DnsDuplicateResultTests, LiteralIsReturnedExactlyOnce) {
    for (const char* text : {"1.2.3", "1.2", "3232235777", "0x7f.0.0.1", "127.0.0.1", "::1"}) {
        const std::vector<IPAddress> addresses = Dns::GetHostAddresses(text);
        EXPECT_EQ(addresses.size(), 1u) << text;
        EXPECT_FALSE(hasDuplicates(addresses)) << text;
    }
}

TEST(DnsDuplicateResultTests, ResolvedNameHasNoDuplicates) {
    // The half the finding does not name: before the repair "localhost" came back three times.
    if (!resolves("localhost")) GTEST_SKIP() << "this host does not resolve 'localhost'";
    const std::vector<IPAddress> addresses = Dns::GetHostAddresses("localhost");
    ASSERT_FALSE(addresses.empty());
    EXPECT_FALSE(hasDuplicates(addresses));
}

TEST(DnsDuplicateResultTests, ResolvedNameHasNoDuplicates_ThroughGetHostEntry) {
    if (!resolves("localhost")) GTEST_SKIP() << "this host does not resolve 'localhost'";
    const IPHostEntry entry = Dns::GetHostEntry("localhost");
    EXPECT_FALSE(entry.getAddressListProperty().empty());
    EXPECT_FALSE(hasDuplicates(entry.getAddressListProperty()));
}

TEST(DnsDuplicateResultTests, ReverseLookupResultHasNoDuplicates) {
    const IPHostEntry entry = Dns::GetHostEntry(IPAddress::Loopback);
    EXPECT_FALSE(entry.getAddressListProperty().empty());
    EXPECT_FALSE(hasDuplicates(entry.getAddressListProperty()));
}

// ---------------------------------------------------------------------------
// Deduplication must not merge addresses that are merely SIMILAR. The equality
// used is binary IPAddress equality -- family, bits and (IPv6) scope id -- and
// no canonicalisation is performed.
// ---------------------------------------------------------------------------

TEST(DnsDuplicateResultTests, MappedAndPlainIPv4AreDistinct_AndNeitherIsCanonicalised) {
    const std::vector<IPAddress> plain = Dns::GetHostAddresses("1.2.3.4");
    const std::vector<IPAddress> mapped = Dns::GetHostAddresses("::ffff:1.2.3.4");
    ASSERT_EQ(plain.size(), 1u);
    ASSERT_EQ(mapped.size(), 1u);
    EXPECT_EQ(plain[0].getAddressFamilyProperty(), AddressFamily::InterNetwork);
    EXPECT_EQ(mapped[0].getAddressFamilyProperty(), AddressFamily::InterNetworkV6);
    EXPECT_NE(plain[0], mapped[0]);
    EXPECT_EQ(plain[0].ToString(), "1.2.3.4");
    EXPECT_EQ(mapped[0].ToString(), "::ffff:1.2.3.4");
}

TEST(DnsDuplicateResultTests, ScopedIPv6AddressesOnDifferentLinksAreDistinct) {
    const std::vector<IPAddress> link7 = Dns::GetHostAddresses("fe80::1%7");
    const std::vector<IPAddress> link9 = Dns::GetHostAddresses("fe80::1%9");
    ASSERT_EQ(link7.size(), 1u);
    ASSERT_EQ(link9.size(), 1u);
    EXPECT_NE(link7[0], link9[0]);
    EXPECT_EQ(link7[0].getScopeIdProperty(), 7);
    EXPECT_EQ(link9[0].getScopeIdProperty(), 9);
}

// ---------------------------------------------------------------------------
// 3. The requested family is applied to a literal.
//
// A family-mismatched literal raises SocketException(HostNotFound) rather than
// returning an empty vector. That is this repository's OWN established contract:
// DnsTests.GetHostAddresses_RequestingIPv6Only_MismatchedIPv4Literal_Throws and
// ..._RequestingIPv4Only_MismatchedIPv6Literal_Throws already pin it, and their
// comment records the reasoning -- an empty vector is "indistinguishable from
// 'checked and found nothing'". Plan section 7.3 predicted an empty result for
// the Unix row; that prediction is corrected in section 17.6, not followed.
// ---------------------------------------------------------------------------

TEST(DnsFamilyFilterTests, NonIPFamilyRejectsAnyLiteral) {
    for (const char* text : {"127.0.0.1", "::1", "fe80::1%7", "1.2.3"}) {
        for (AddressFamily family : {AddressFamily::Unix, AddressFamily::Unknown,
                                      AddressFamily::Max}) {
            EXPECT_THROW((void)Dns::GetHostAddresses(text, family), SocketException)
                << text << " family " << static_cast<int>(family);
            EXPECT_THROW((void)Dns::GetHostEntry(text, family), SocketException)
                << text << " family " << static_cast<int>(family);
        }
    }
}

TEST(DnsFamilyFilterTests, MismatchedIPFamilyRejectsALiteral) {
    EXPECT_THROW((void)Dns::GetHostAddresses("127.0.0.1", AddressFamily::InterNetworkV6),
                 SocketException);
    EXPECT_THROW((void)Dns::GetHostAddresses("::1", AddressFamily::InterNetwork), SocketException);
    EXPECT_THROW((void)Dns::GetHostAddresses("fe80::1%7", AddressFamily::InterNetwork),
                 SocketException);
    EXPECT_THROW((void)Dns::GetHostEntry("127.0.0.1", AddressFamily::InterNetworkV6),
                 SocketException);
}

TEST(DnsFamilyFilterTests, MatchingAndUnspecifiedFamiliesStillReturnTheLiteral) {
    struct Row { const char* text; AddressFamily family; };
    const Row rows[] = {
        {"127.0.0.1", AddressFamily::Unspecified},
        {"127.0.0.1", AddressFamily::InterNetwork},
        {"::1",       AddressFamily::Unspecified},
        {"::1",       AddressFamily::InterNetworkV6},
        {"fe80::1%7", AddressFamily::Unspecified},
        {"fe80::1%7", AddressFamily::InterNetworkV6},
    };
    for (const Row& row : rows) {
        const std::vector<IPAddress> addresses = Dns::GetHostAddresses(row.text, row.family);
        ASSERT_EQ(addresses.size(), 1u) << row.text;
        EXPECT_EQ(addresses[0], IPAddress::Parse(row.text)) << row.text;
    }
}

TEST(DnsFamilyFilterTests, RejectionCarriesHostNotFoundAndNamesTheFamily) {
    try {
        (void)Dns::GetHostAddresses("127.0.0.1", AddressFamily::Unix);
        ADD_FAILURE() << "expected a SocketException";
    } catch (const SocketException& ex) {
        EXPECT_EQ(ex.getSocketErrorCodeProperty(), SocketError::HostNotFound);
        const std::string what(ex.what());
        EXPECT_NE(what.find("Unix"), std::string::npos) << what;
        EXPECT_NE(what.find("127.0.0.1"), std::string::npos) << what;
    }
}

// ---------------------------------------------------------------------------
// 4. The failure message no longer fabricates a Win32 error on POSIX.
//
// The exact native text is the platform resolver's own and is deliberately NOT
// asserted -- it varies between C libraries. What is asserted is the contract:
// the exception type, the SocketError, the native code, the absence of "Win32",
// and that the message names the host that failed.
// ---------------------------------------------------------------------------

TEST(DnsErrorMessageTests, ResolutionFailureDoesNotNameAWin32Error) {
    for (const char* host : {"definitely-not-a-real-host.invalid", "999.999.999.999",
                             "-0.0.0.1", "1.2.3.4.5"}) {
        try {
            (void)Dns::GetHostAddresses(host);
            ADD_FAILURE() << "expected a SocketException for " << host;
        } catch (const SocketException& ex) {
            const std::string what(ex.what());
            EXPECT_EQ(what.find("Win32"), std::string::npos) << what;
            EXPECT_NE(what.find(host), std::string::npos) << what;
            EXPECT_EQ(ex.getSocketErrorCodeProperty(), SocketError::HostNotFound) << host;
            // The native code is part of the contract and is unchanged.
            EXPECT_EQ(ex.getErrorCodeProperty(), static_cast<SharpRuntime::intcs>(SocketError::HostNotFound))
                << host;
        }
    }
}

TEST(DnsErrorMessageTests, GetHostEntryFailureDoesNotNameAWin32Error) {
    try {
        (void)Dns::GetHostEntry("definitely-not-a-real-host.invalid");
        ADD_FAILURE() << "expected a SocketException";
    } catch (const SocketException& ex) {
        const std::string what(ex.what());
        EXPECT_EQ(what.find("Win32"), std::string::npos) << what;
        EXPECT_EQ(ex.getSocketErrorCodeProperty(), SocketError::HostNotFound);
    }
}

// ---------------------------------------------------------------------------
// The wildcard is DELIBERATELY unchanged -- it is the gated ticket #2043's half
// of SR-AUD-304, and this pin exists so that ticket cannot land silently.
// ---------------------------------------------------------------------------

TEST(DnsWildcardPinTests, WildcardLiteralsStillResolveToThemselves_SeeTicket2043) {
    const std::vector<IPAddress> v4 = Dns::GetHostAddresses("0.0.0.0");
    ASSERT_EQ(v4.size(), 1u);
    EXPECT_EQ(v4[0], IPAddress::Any);

    const std::vector<IPAddress> v6 = Dns::GetHostAddresses("::");
    ASSERT_EQ(v6.size(), 1u);
    EXPECT_EQ(v6[0], IPAddress::IPv6Any);

    const std::vector<IPAddress> v4Family =
        Dns::GetHostAddresses("0.0.0.0", AddressFamily::InterNetwork);
    ASSERT_EQ(v4Family.size(), 1u);
    EXPECT_EQ(v4Family[0], IPAddress::Any);
}
