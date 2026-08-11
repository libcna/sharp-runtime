// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Ticket #2038 -- SR-AUD-303, cause N-D of docs/SystemNetNamespaceReviewPlan.md.
//
// clearNonPrefixBits rebuilt the masked address through the byte-vector constructor, which knows
// nothing about scope ids, so an IPv6 base address handed to the constructor WITH a scope id
// silently lost it. Measured before the repair (build-probe/2038_probe1_before_after.log),
// IPNetwork(fe80::1%7, 64).BaseAddress reported ScopeId 0, ToString() rendered "fe80::/64", and
// -- a consequence the finding does not name -- two networks on DIFFERENT links compared equal
// and hashed equal.
//
// The half of this ticket that could have gone wrong silently is Contains: IPAddress::operator==
// compares the scope id, so preserving the scope while still comparing IPAddress objects would
// have made containment scope-SENSITIVE, and fe80::1%7/64 would have stopped containing
// fe80::1%9 -- and even fe80::1%7 itself. Contains therefore compares masked BYTES, and all
// fifteen probed Contains answers are identical before and after.
#include <gtest/gtest.h>
#include <string>
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Net/IPAddress.hpp"
#include "System/Net/IPNetwork.hpp"

using System::Net::IPAddress;
using System::Net::IPNetwork;
using SharpRuntime::intcs;

// ---------------------------------------------------------------------------
// The scope id survives masking, at every prefix length.
// ---------------------------------------------------------------------------

TEST(IPNetworkScopeTests, IPv6BaseAddressKeepsItsScopeId_AcrossPrefixLengths) {
    const IPAddress scoped = IPAddress::Parse("fe80::1%7");
    for (intcs prefix : {0, 1, 63, 64, 127, 128}) {
        const IPNetwork network(scoped, prefix);
        EXPECT_EQ(network.getBaseAddressProperty().getScopeIdProperty(), 7)
            << "prefix " << prefix;
        EXPECT_TRUE(network.getBaseAddressProperty().getIsIPv6Property()) << "prefix " << prefix;
    }
}

TEST(IPNetworkScopeTests, MaskedBytesAreStillMasked) {
    // Preserving the scope id must not stop the host bits from being cleared.
    const IPNetwork network(IPAddress::Parse("fe80::1%7"), 64);
    EXPECT_EQ(network.getBaseAddressProperty().ToString(), "fe80::%7");
    EXPECT_EQ(network.ToString(), "fe80::%7/64");

    const IPNetwork exact(IPAddress::Parse("fe80::1%7"), 128);
    EXPECT_EQ(exact.getBaseAddressProperty().ToString(), "fe80::1%7");
    EXPECT_EQ(exact.ToString(), "fe80::1%7/128");

    const IPNetwork whole(IPAddress::Parse("fe80::1%7"), 0);
    EXPECT_EQ(whole.getBaseAddressProperty().ToString(), "::%7");
}

TEST(IPNetworkScopeTests, ScopeZeroAndIPv4_Unchanged) {
    for (intcs prefix : {0, 1, 63, 64, 127, 128}) {
        const IPNetwork network(IPAddress::Parse("fe80::1"), prefix);
        EXPECT_EQ(network.getBaseAddressProperty().getScopeIdProperty(), 0) << "prefix " << prefix;
    }
    EXPECT_EQ(IPNetwork(IPAddress::Parse("fe80::1"), 64).ToString(), "fe80::/64");
    EXPECT_EQ(IPNetwork(IPAddress::Parse("192.168.1.130"), 24).ToString(), "192.168.1.0/24");
    EXPECT_EQ(IPNetwork(IPAddress::Parse("192.168.1.130"), 0).ToString(), "0.0.0.0/0");
    EXPECT_EQ(IPNetwork(IPAddress::Parse("192.168.1.130"), 32).ToString(), "192.168.1.130/32");
    EXPECT_EQ(IPNetwork(IPAddress::Parse("192.168.1.130"), 1).ToString(), "128.0.0.0/1");
}

// ---------------------------------------------------------------------------
// Contains must be byte-exact with the pre-repair behaviour. Every expectation
// below is transcribed from the BEFORE half of the probe log.
// ---------------------------------------------------------------------------

TEST(IPNetworkScopeTests, Contains_IsUnchanged_AndScopeIndependent) {
    struct Row { const char* network; intcs prefix; const char* candidate; bool expected; };
    const Row rows[] = {
        {"fe80::1%7",     64,  "fe80::1%7",          true},
        {"fe80::1%7",     64,  "fe80::2%7",          true},
        {"fe80::1%7",     64,  "fe80::1%9",          true},   // a DIFFERENT link: still contained
        {"fe80::1%7",     64,  "fe80::1",            true},   // no scope at all: still contained
        {"fe80::1%7",     128, "fe80::1%7",          true},
        {"fe80::1%7",     128, "fe80::2%7",          false},
        {"fe80::1",       64,  "fe80::1%7",          true},
        {"fe80::1",       64,  "fe80::5",            true},
        {"fe80::1",       64,  "fe81::5",            false},
        {"192.168.1.130", 24,  "192.168.1.5",        true},
        {"192.168.1.130", 24,  "192.168.2.5",        false},
        {"192.168.1.130", 24,  "::ffff:192.168.1.5", true},   // IPv4-mapped, still mapped down
        {"192.168.1.130", 24,  "fe80::1",            false},
        {"::",            0,   "fe80::1%7",          true},
        {"0.0.0.0",       0,   "8.8.8.8",            true},
    };
    for (const Row& row : rows) {
        const IPNetwork network(IPAddress::Parse(row.network), row.prefix);
        EXPECT_EQ(network.Contains(IPAddress::Parse(row.candidate)), row.expected)
            << row.network << "/" << row.prefix << " Contains(" << row.candidate << ")";
    }
}

TEST(IPNetworkScopeTests, Contains_AgreesWithTheMaskedBase) {
    // The base address is inside its own network at every prefix length, which is the property
    // that would have broken had Contains kept comparing whole IPAddress objects.
    const IPAddress scoped = IPAddress::Parse("fe80::1%7");
    for (intcs prefix : {0, 1, 63, 64, 127, 128}) {
        const IPNetwork network(scoped, prefix);
        EXPECT_TRUE(network.Contains(scoped)) << "prefix " << prefix;
        EXPECT_TRUE(network.Contains(network.getBaseAddressProperty())) << "prefix " << prefix;
    }
}

// ---------------------------------------------------------------------------
// Parse / ToString / equality / hashing.
// ---------------------------------------------------------------------------

TEST(IPNetworkScopeTests, ParseKeepsTheScopeId_AndRoundTrips) {
    IPNetwork parsed;
    ASSERT_TRUE(IPNetwork::TryParse("fe80::1%7/64", parsed));
    EXPECT_EQ(parsed.ToString(), "fe80::%7/64");
    EXPECT_EQ(parsed.getBaseAddressProperty().getScopeIdProperty(), 7);

    IPNetwork again;
    ASSERT_TRUE(IPNetwork::TryParse(parsed.ToString(), again));
    EXPECT_EQ(again, parsed);
    EXPECT_EQ(again.GetHashCode(), parsed.GetHashCode());
}

TEST(IPNetworkScopeTests, UnscopedAndIPv4Forms_RoundTripUnchanged) {
    for (const char* text : {"fe80::/64", "192.168.1.0/24", "::/0", "0.0.0.0/0"}) {
        IPNetwork parsed;
        ASSERT_TRUE(IPNetwork::TryParse(text, parsed)) << text;
        EXPECT_EQ(parsed.ToString(), text);
        IPNetwork again;
        ASSERT_TRUE(IPNetwork::TryParse(parsed.ToString(), again)) << text;
        EXPECT_EQ(again, parsed) << text;
        EXPECT_EQ(again.GetHashCode(), parsed.GetHashCode()) << text;
    }
}

TEST(IPNetworkScopeTests, NetworksOnDifferentLinksAreNoLongerEqual) {
    // Not named by the finding, and the sharpest consequence of the dropped scope id: before the
    // repair these two compared EQUAL and hashed EQUAL, so a map keyed on IPNetwork silently
    // merged two different links.
    const IPNetwork onLink7(IPAddress::Parse("fe80::1%7"), 64);
    const IPNetwork onLink9(IPAddress::Parse("fe80::1%9"), 64);
    EXPECT_NE(onLink7, onLink9);
    // The hash is deliberately NOT asserted to differ. Equal-implies-equal is the whole of the
    // contract, unequal keys are permitted to collide, and a hashed container keeps colliding
    // keys apart by Equals -- so the inequality above is what rules the silent merge out.
    // IPNetwork also hashes through the per-process-seeded System::HashCode, which makes any
    // such inequality unreproducible across runs (docs/HashAssertionContractRule.md R2/R4).
    EXPECT_EQ(onLink7.ToString(), "fe80::%7/64");
    EXPECT_EQ(onLink9.ToString(), "fe80::%9/64");

    // ...while a scoped and an unscoped network still differ, as they always did in bytes.
    const IPNetwork unscoped(IPAddress::Parse("fe80::1"), 64);
    EXPECT_NE(onLink7, unscoped);
}

TEST(IPNetworkScopeTests, PrefixLengthValidation_Unchanged) {
    EXPECT_THROW((void)IPNetwork(IPAddress::Parse("fe80::1%7"), -1),
                 System::ArgumentOutOfRangeException);
    EXPECT_THROW((void)IPNetwork(IPAddress::Parse("fe80::1%7"), 129),
                 System::ArgumentOutOfRangeException);
    EXPECT_THROW((void)IPNetwork(IPAddress::Loopback, 33), System::ArgumentOutOfRangeException);
    EXPECT_NO_THROW((void)IPNetwork(IPAddress::Parse("fe80::1%7"), 128));
    EXPECT_NO_THROW((void)IPNetwork(IPAddress::Loopback, 32));
}
