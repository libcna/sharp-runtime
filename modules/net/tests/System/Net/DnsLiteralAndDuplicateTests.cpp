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
//
// CORRECTION (ticket #2375, 2026-08-18): that sentence was measured false for exactly one row.
// MalformedLiteralTextIsStillRejected asserted that six strings fail to resolve, which is not a
// property of this port at all -- it is the RESOLVER's opinion, and any wildcard DNS server can
// change it. On this container "1.2.3." takes a 13 ms round trip and comes back as 1.2.0.3,
// where "1.2.3" is answered by libc in 0.03 ms. The test now asks getaddrinfo itself and
// requires the port to AGREE with it, which is the property #2039 was actually about and is
// independent of what any resolver answers. See docs/DnsLiteralOracleTestDefect.md.
#include <gtest/gtest.h>
#include "System/ArgumentException.hpp"
#include <algorithm>
#include <string>
#include <vector>
#include "System/Net/Dns.hpp"
#include "System/Net/IPAddress.hpp"
#include "System/Net/IPHostEntry.hpp"
#include "System/Net/Sockets/AddressFamily.hpp"
#include "System/Net/Sockets/SocketError.hpp"
#include "System/Net/Sockets/SocketException.hpp"
#ifndef _WIN32
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#endif

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

// An independent oracle, in the shape #2351 established for tzdata: ask the system
// resolver the same question through a different door, and require the port to give
// the same answer. Nothing here asserts what that answer IS.
namespace {
struct ResolverAnswer {
    bool                     resolved = false;
    std::vector<std::string> addresses;
};

ResolverAnswer askTheSystemResolver(const char* text) {
    ResolverAnswer answer;
#ifdef _WIN32
    (void)text;   // the oracle is POSIX-only; see the guard at the call site
#else
    ::addrinfo hints{};
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    ::addrinfo* head  = nullptr;
    if (::getaddrinfo(text, nullptr, &hints, &head) != 0 || head == nullptr) return answer;
    answer.resolved = true;
    for (::addrinfo* it = head; it != nullptr; it = it->ai_next) {
        char buffer[INET6_ADDRSTRLEN] = {};
        if (it->ai_family == AF_INET) {
            ::inet_ntop(AF_INET,
                        &reinterpret_cast<::sockaddr_in*>(it->ai_addr)->sin_addr,
                        buffer, sizeof(buffer));
        } else if (it->ai_family == AF_INET6) {
            ::inet_ntop(AF_INET6,
                        &reinterpret_cast<::sockaddr_in6*>(it->ai_addr)->sin6_addr,
                        buffer, sizeof(buffer));
        } else {
            continue;
        }
        answer.addresses.emplace_back(buffer);
    }
    ::freeaddrinfo(head);
#endif
    return answer;
}
} // namespace

// FLIPPED by #2375 (2026-08-18). This used to assert that six strings throw, which made the
// suite depend on the container's DNS. What the port actually owes is narrower and testable:
// text that IPAddress::Parse rejects must never be given a literal reading, and the answer
// must be whatever the system resolver says -- including "nothing", which is what most
// resolvers say about most of these.
TEST(DnsLiteralTests, MalformedLiteralTextIsNeverGivenALiteralReading) {
    for (const char* text : {"1.2.3.4.5", "1.2.3.", "256.1.1.1", "1.2.3.4 ", "", "999.999.999.999"}) {
        // 1. The port must not read any of these as a literal. This is the whole finding,
        //    and it holds on every machine.
        IPAddress parsed;
        EXPECT_FALSE(IPAddress::TryParse(text, parsed)) << text;

#ifdef _WIN32
        // No oracle here; the literal claim above is still checked.
        continue;
#else
        // 2. Whatever happens next is the resolver's business, and the port must report it
        //    faithfully rather than inventing an answer.
        const ResolverAnswer oracle = askTheSystemResolver(text);
        if (!oracle.resolved) {
            EXPECT_THROW((void)Dns::GetHostAddresses(text), SocketException) << text;
            continue;
        }
        std::vector<std::string> got;
        ASSERT_NO_THROW({
            for (const IPAddress& address : Dns::GetHostAddresses(text))
                got.push_back(address.ToString());
        }) << text;
        std::vector<std::string> expected = oracle.addresses;
        std::sort(expected.begin(), expected.end());
        expected.erase(std::unique(expected.begin(), expected.end()), expected.end());
        std::sort(got.begin(), got.end());
        EXPECT_EQ(got, expected) << text;
#endif
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
// #2043 RESOLVED -- the wildcard literals are rejected.
//
// The ticket was split out of #2039 precisely because this is the one half of SR-AUD-304 that
// REMOVES A WORKING, MEANINGFUL RESULT, so it needed evidence rather than judgement. The
// reference supplies it: Dns.cs:686-690 on the string path, and :46-50 / :158-162 on the
// IPAddress overloads, all raise ArgumentException(SR.net_invalid_ip_addr) for IPAddress.Any or
// IPAddress.IPv6Any.
// ---------------------------------------------------------------------------

TEST(DnsWildcardPinTests, Fix2043_WildcardLiteralsAreRejected) {
    // .NET's message says WHY, and it is transcribed rather than paraphrased: these are
    // UNSPECIFIED addresses. They name "every local interface" to bind, and nothing at all to
    // connect -- so resolving one to itself hands a caller a target it cannot use.
    for (const char* wildcard : {"0.0.0.0", "::"}) {
        SCOPED_TRACE(wildcard);
        EXPECT_THROW((void)Dns::GetHostAddresses(wildcard), System::ArgumentException);
        EXPECT_THROW((void)Dns::GetHostAddresses(wildcard, AddressFamily::InterNetwork),
                     System::ArgumentException);
        EXPECT_THROW((void)Dns::GetHostAddresses(wildcard, AddressFamily::InterNetworkV6),
                     System::ArgumentException);
    }

    // The rejection comes BEFORE the family check, matching Dns.cs:686-690, which tests the
    // wildcard immediately after TryParse and before anything else looks at the value. So
    // "0.0.0.0" with an IPv6-only request is an ArgumentException, not a SocketException about
    // the family.
    try {
        (void)Dns::GetHostAddresses("0.0.0.0", AddressFamily::InterNetworkV6);
        ADD_FAILURE() << "expected an ArgumentException";
    } catch (const System::ArgumentException&) {
        SUCCEED();
    } catch (const SocketException&) {
        ADD_FAILURE() << "the family check ran first; the wildcard check must come before it";
    }
}

TEST(DnsWildcardPinTests, Fix2043_EveryOtherLiteralStillResolvesToItself) {
    // The invariance row: only the two unspecified addresses moved. A loopback literal is not
    // one of them, and neither is any ordinary address.
    const std::vector<IPAddress> loopback = Dns::GetHostAddresses("127.0.0.1");
    ASSERT_EQ(loopback.size(), 1u);
    EXPECT_EQ(loopback[0], IPAddress::Loopback);

    const std::vector<IPAddress> v6Loopback = Dns::GetHostAddresses("::1");
    ASSERT_EQ(v6Loopback.size(), 1u);
    EXPECT_EQ(v6Loopback[0], IPAddress::IPv6Loopback);

    const std::vector<IPAddress> ordinary = Dns::GetHostAddresses("8.8.8.8");
    ASSERT_EQ(ordinary.size(), 1u);

    // ...and 0.0.0.1 is NOT the wildcard, so the check must be an equality rather than a
    // "starts with zero" test.
    EXPECT_NO_THROW((void)Dns::GetHostAddresses("0.0.0.1"));
}
