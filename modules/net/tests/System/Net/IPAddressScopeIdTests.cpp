// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Ticket #2036 -- SR-AUD-301, cause N-B of docs/SystemNetNamespaceReviewPlan.md.
//
// The IPv6 scope ID lives in a uint32_t, but both public doors that set it take a longcs and
// used to write static_cast<uint32_t>(value). Measured before the repair
// (build-probe/2036_probe1_before_after.log), that wrapped modulo 2^32 rather than clamping, so
// it produced a PLAUSIBLE scope ID a caller cannot distinguish from one it asked for:
//
//   -1 -> 4294967295   -5 -> 4294967291   -4294967295 -> 1
//   4294967296 -> 0    4294967297 -> 1    LONGCS_MIN -> 0    LONGCS_MAX -> 4294967295
//
// and ToString() rendered the wrapped value, e.g. "fe80::1%4294967291" for scope ID -5.
#include <gtest/gtest.h>
#include <array>
#include <string>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Net/IPAddress.hpp"
#include "System/Net/Sockets/SocketException.hpp"

using System::ArgumentOutOfRangeException;
using System::Net::IPAddress;
using System::Net::Sockets::SocketException;
using SharpRuntime::bytecs;
using SharpRuntime::longcs;

namespace {

    const std::array<bytecs, 16> kFe80_1{0xFE, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};

    // Every value outside [0, UInt32.MaxValue] that the probe measured, including the three
    // whose wrap produced a small, entirely plausible scope ID.
    const longcs kOutOfDomain[] = {
        SharpRuntime::LONGCS_MIN, -4294967297LL, -4294967296LL, -4294967295LL,
        -5, -1, 4294967296LL, 4294967297LL, 8589934591LL, SharpRuntime::LONGCS_MAX,
    };

    const longcs kInDomain[] = {
        0, 1, 7, 255, 65535, 16777215, 2147483647LL, 2147483648LL, 4294967294LL, 4294967295LL,
    };

} // namespace

// ---------------------------------------------------------------------------
// The constructor.
// ---------------------------------------------------------------------------

TEST(IPAddressScopeIdTests, Constructor_OutOfDomain_Throws_WithParamNameScopeId) {
    for (longcs scopeId : kOutOfDomain) {
        try {
            IPAddress address(kFe80_1, scopeId);
            ADD_FAILURE() << "scopeId " << scopeId << " was accepted and read back as "
                          << address.getScopeIdProperty();
        } catch (const ArgumentOutOfRangeException& ex) {
            EXPECT_EQ(ex.getParamNameProperty(), "scopeId") << "scopeId " << scopeId;
        }
    }
}

TEST(IPAddressScopeIdTests, Constructor_InDomain_RoundTripsExactly) {
    for (longcs scopeId : kInDomain) {
        IPAddress address(kFe80_1, scopeId);
        EXPECT_EQ(address.getScopeIdProperty(), scopeId);
        EXPECT_TRUE(address.getIsIPv6Property());
    }
}

TEST(IPAddressScopeIdTests, Constructor_BoundaryValues) {
    EXPECT_NO_THROW((void)IPAddress(kFe80_1, 0));
    EXPECT_NO_THROW((void)IPAddress(kFe80_1, 4294967295LL));
    EXPECT_THROW((void)IPAddress(kFe80_1, -1), ArgumentOutOfRangeException);
    EXPECT_THROW((void)IPAddress(kFe80_1, 4294967296LL), ArgumentOutOfRangeException);
}

// ---------------------------------------------------------------------------
// The setter. A rejected set must leave the previous value in place.
// ---------------------------------------------------------------------------

TEST(IPAddressScopeIdTests, Setter_OutOfDomain_Throws_WithParamNameValue_AndDoesNotMutate) {
    for (longcs value : kOutOfDomain) {
        IPAddress address(kFe80_1, 7);
        try {
            address.setScopeIdProperty(value);
            ADD_FAILURE() << "value " << value << " was accepted and read back as "
                          << address.getScopeIdProperty();
        } catch (const ArgumentOutOfRangeException& ex) {
            EXPECT_EQ(ex.getParamNameProperty(), "value") << "value " << value;
            EXPECT_EQ(address.getScopeIdProperty(), 7) << "value " << value;
            EXPECT_EQ(address.ToString(), "fe80::1%7") << "value " << value;
        }
    }
}

TEST(IPAddressScopeIdTests, Setter_InDomain_RoundTripsExactly) {
    for (longcs value : kInDomain) {
        IPAddress address(kFe80_1, 7);
        address.setScopeIdProperty(value);
        EXPECT_EQ(address.getScopeIdProperty(), value);
    }
}

// ---------------------------------------------------------------------------
// The family guard is unchanged and still wins.
// ---------------------------------------------------------------------------

TEST(IPAddressScopeIdTests, IPv4_FamilyGuardStillFiresFirst) {
    IPAddress v4 = IPAddress::Loopback;
    // Wrong family AND out of domain: the family answer is the more specific one and it is the
    // pre-existing contract, so it must win.
    EXPECT_THROW(v4.setScopeIdProperty(-1), SocketException);
    EXPECT_THROW(v4.setScopeIdProperty(4294967296LL), SocketException);
    EXPECT_THROW((void)v4.getScopeIdProperty(), SocketException);
}

// ---------------------------------------------------------------------------
// ToString and the parser must be unaffected -- the parser's own uint32 from_chars
// already rejected out-of-range scope text, which is why the finding is about the
// raw doors only.
// ---------------------------------------------------------------------------

TEST(IPAddressScopeIdTests, ToString_RendersInDomainScopeIds) {
    EXPECT_EQ(IPAddress(kFe80_1, 0).ToString(), "fe80::1");
    EXPECT_EQ(IPAddress(kFe80_1, 7).ToString(), "fe80::1%7");
    EXPECT_EQ(IPAddress(kFe80_1, 4294967295LL).ToString(), "fe80::1%4294967295");
}

TEST(IPAddressScopeIdTests, Parser_ScopeDomain_Unchanged) {
    IPAddress parsed;
    EXPECT_TRUE(IPAddress::TryParse("fe80::1%0", parsed));
    EXPECT_EQ(parsed.getScopeIdProperty(), 0);
    EXPECT_TRUE(IPAddress::TryParse("fe80::1%4294967295", parsed));
    EXPECT_EQ(parsed.getScopeIdProperty(), 4294967295LL);
    EXPECT_FALSE(IPAddress::TryParse("fe80::1%4294967296", parsed));
    EXPECT_FALSE(IPAddress::TryParse("fe80::1%-1", parsed));
}

// ---------------------------------------------------------------------------
// The scope ID survives the encode/decode paths that carry it.
// ---------------------------------------------------------------------------

TEST(IPAddressScopeIdTests, EqualityStillDistinguishesScopeIds) {
    EXPECT_NE(IPAddress(kFe80_1, 7), IPAddress(kFe80_1, 8));
    EXPECT_EQ(IPAddress(kFe80_1, 7), IPAddress(kFe80_1, 7));
    // Before the repair these two compared EQUAL, because -1 wrapped onto 4294967295.
    EXPECT_THROW((void)IPAddress(kFe80_1, -1), ArgumentOutOfRangeException);
    EXPECT_NO_THROW((void)IPAddress(kFe80_1, 4294967295LL));
}
