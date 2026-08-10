// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Ticket #2035 -- SR-AUD-300, cause N-A of docs/SystemNetNamespaceReviewPlan.md.
//
// GetIPEndPoint read fixed offsets 2..7 (IPv4) or 2..27 (IPv6) out of a buffer whose family and
// size the caller chose, with no check of either. Measured before the repair
// (build-probe/2035_probe1_before_after.log):
//
//   * ASan reported heap-buffer-overflow READs at this function's own lines --
//     SocketAddress.cpp:106 for a 2-byte InterNetwork buffer and :110 for an 8-byte
//     InterNetworkV6 one -- and the decoded values were whatever the heap held
//     ("238.85.0.0:52122" in one run, "[::%553648128]:52122" in another);
//   * every non-IP family (Unix, Unspecified, Unknown, Max) fell into the IPv4 branch;
//   * SocketAddress(IPAddress, port) truncated the whole intcs port domain into 16 bits:
//     70000 -> 4464, -1 -> 65535, INTCS_MAX -> 65535, INTCS_MIN -> 0;
//   * a buffer whose DECLARED size had been shrunk below the layout minimum still decoded,
//     reading bytes the object reports as unused -- a defect the finding does not name, which
//     no sanitizer can see because the allocation is still there.
#include <gtest/gtest.h>
#include <string>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/IndexOutOfRangeException.hpp"
#include "System/Net/IPAddress.hpp"
#include "System/Net/IPEndPoint.hpp"
#include "System/Net/SocketAddress.hpp"
#include "System/Net/Sockets/AddressFamily.hpp"

using System::ArgumentException;
using System::ArgumentOutOfRangeException;
using System::Net::IPAddress;
using System::Net::IPEndPoint;
using System::Net::SocketAddress;
using System::Net::Sockets::AddressFamily;
using SharpRuntime::bytecs;
using SharpRuntime::intcs;

namespace {

    std::vector<bytecs> rawBytes(const SocketAddress& address) {
        std::vector<bytecs> bytes;
        for (intcs i = 0; i < address.getSizeProperty(); ++i)
            bytes.push_back(address[i]);
        return bytes;
    }

} // namespace

// ---------------------------------------------------------------------------
// Family: only the two IP families decode. Every other family used to be
// silently treated as IPv4.
// ---------------------------------------------------------------------------

TEST(SocketAddressDecodeTests, NonIPFamily_Throws_WithFamilyInTheMessage) {
    const AddressFamily families[] = {AddressFamily::Unknown, AddressFamily::Unspecified,
                                      AddressFamily::Unix, AddressFamily::Max};
    const char* names[] = {"Unknown", "Unspecified", "Unix", "Max"};
    for (size_t i = 0; i < 4; ++i) {
        // Generously sized -- the rejection is about the family, not the size.
        SocketAddress address(families[i], SocketAddress::MaxAddressSize);
        try {
            (void)address.GetIPEndPoint();
            ADD_FAILURE() << "family " << names[i] << " decoded instead of throwing";
        } catch (const ArgumentException& ex) {
            const std::string what(ex.what());
            EXPECT_NE(what.find(names[i]), std::string::npos) << what;
            EXPECT_NE(what.find("cannot be decoded as an IPEndPoint"), std::string::npos) << what;
        }
    }
}

TEST(SocketAddressDecodeTests, UnnamedFamilyValue_AlsoRejected) {
    // A value outside the enum's named set is the case that decoded as IPv4 most quietly.
    SocketAddress address(static_cast<AddressFamily>(4242), SocketAddress::MaxAddressSize);
    EXPECT_THROW((void)address.GetIPEndPoint(), ArgumentException);
}

// ---------------------------------------------------------------------------
// Size: below the minimum / exactly the minimum / above it, for both IP families.
// ---------------------------------------------------------------------------

TEST(SocketAddressDecodeTests, IPv4_BelowMinimumSize_Throws) {
    for (intcs size : {static_cast<intcs>(2), static_cast<intcs>(3), static_cast<intcs>(8),
                       static_cast<intcs>(SocketAddress::IPv4AddressSize - 1)}) {
        SocketAddress address(AddressFamily::InterNetwork, size);
        try {
            (void)address.GetIPEndPoint();
            ADD_FAILURE() << "IPv4 size " << size << " decoded instead of throwing";
        } catch (const ArgumentException& ex) {
            const std::string what(ex.what());
            EXPECT_NE(what.find("too small"), std::string::npos) << what;
            EXPECT_NE(what.find("16"), std::string::npos) << what;
        }
    }
}

TEST(SocketAddressDecodeTests, IPv4_MinimumSizeAndAbove_Decode) {
    for (intcs size : {SocketAddress::IPv4AddressSize,
                       static_cast<intcs>(SocketAddress::IPv4AddressSize + 1),
                       SocketAddress::MaxAddressSize}) {
        SocketAddress address(AddressFamily::InterNetwork, size);
        const IPEndPoint endPoint = address.GetIPEndPoint();
        EXPECT_EQ(endPoint.ToString(), "0.0.0.0:0") << "size " << size;
    }
}

TEST(SocketAddressDecodeTests, IPv6_BelowMinimumSize_Throws) {
    for (intcs size : {static_cast<intcs>(2), static_cast<intcs>(8), static_cast<intcs>(16),
                       static_cast<intcs>(SocketAddress::IPv6AddressSize - 1)}) {
        SocketAddress address(AddressFamily::InterNetworkV6, size);
        try {
            (void)address.GetIPEndPoint();
            ADD_FAILURE() << "IPv6 size " << size << " decoded instead of throwing";
        } catch (const ArgumentException& ex) {
            const std::string what(ex.what());
            EXPECT_NE(what.find("too small"), std::string::npos) << what;
            EXPECT_NE(what.find("28"), std::string::npos) << what;
        }
    }
}

TEST(SocketAddressDecodeTests, IPv6_MinimumSizeAndAbove_Decode) {
    for (intcs size : {SocketAddress::IPv6AddressSize,
                       static_cast<intcs>(SocketAddress::IPv6AddressSize + 1),
                       SocketAddress::MaxAddressSize}) {
        SocketAddress address(AddressFamily::InterNetworkV6, size);
        const IPEndPoint endPoint = address.GetIPEndPoint();
        EXPECT_EQ(endPoint.ToString(), "[::]:0") << "size " << size;
    }
}

// ---------------------------------------------------------------------------
// The declared size, not just the allocation. setSizeProperty can shrink an
// otherwise well-formed buffer below its layout minimum; GetIPEndPoint used to
// keep decoding, reading offsets that operator[] itself refuses.
// ---------------------------------------------------------------------------

TEST(SocketAddressDecodeTests, DeclaredSizeShrunkBelowMinimum_Throws) {
    SocketAddress address(IPAddress::Loopback, 80);
    ASSERT_EQ(address.getSizeProperty(), SocketAddress::IPv4AddressSize);
    ASSERT_EQ(address.GetIPEndPoint().ToString(), "127.0.0.1:80");

    address.setSizeProperty(4);
    // The type already refuses to read offset 4 through its own indexer...
    EXPECT_THROW((void)address[4], System::IndexOutOfRangeException);
    // ...so GetIPEndPoint, which needs offsets 4..7, must refuse too.
    EXPECT_THROW((void)address.GetIPEndPoint(), ArgumentException);

    address.setSizeProperty(SocketAddress::IPv4AddressSize);
    EXPECT_EQ(address.GetIPEndPoint().ToString(), "127.0.0.1:80");
}

// ---------------------------------------------------------------------------
// The port domain of SocketAddress(const IPAddress&, intcs).
// ---------------------------------------------------------------------------

TEST(SocketAddressDecodeTests, Port_OutOfRange_Throws_WithExactParamName) {
    for (intcs port : {SharpRuntime::INTCS_MIN, static_cast<intcs>(-70000),
                       static_cast<intcs>(-1), static_cast<intcs>(65536),
                       static_cast<intcs>(70000), SharpRuntime::INTCS_MAX}) {
        try {
            SocketAddress address(IPAddress::Loopback, port);
            ADD_FAILURE() << "port " << port << " was accepted and encoded as "
                          << address.GetIPEndPoint().ToString();
        } catch (const ArgumentOutOfRangeException& ex) {
            EXPECT_EQ(ex.getParamNameProperty(), "port") << "port " << port;
        }
    }
}

TEST(SocketAddressDecodeTests, Port_OutOfRange_Throws_ForIPv6Too) {
    for (intcs port : {static_cast<intcs>(-1), static_cast<intcs>(65536),
                       SharpRuntime::INTCS_MIN, SharpRuntime::INTCS_MAX}) {
        try {
            SocketAddress address(IPAddress::IPv6Loopback, port);
            ADD_FAILURE() << "IPv6 port " << port << " was accepted";
        } catch (const ArgumentOutOfRangeException& ex) {
            EXPECT_EQ(ex.getParamNameProperty(), "port");
        }
    }
}

TEST(SocketAddressDecodeTests, Port_BoundaryValues_StillAccepted) {
    for (intcs port : {IPEndPoint::MinPort, static_cast<intcs>(1), static_cast<intcs>(80),
                       static_cast<intcs>(32768), IPEndPoint::MaxPort}) {
        SocketAddress v4(IPAddress::Loopback, port);
        EXPECT_EQ(v4.GetIPEndPoint().getPortProperty(), port);
        SocketAddress v6(IPAddress::IPv6Loopback, port);
        EXPECT_EQ(v6.GetIPEndPoint().getPortProperty(), port);
    }
}

// ---------------------------------------------------------------------------
// Well-formed round trips must be byte-identical to the pre-repair encoding.
// The literals below are transcribed from build-probe/2035_probe1_before_after.log.
// ---------------------------------------------------------------------------

TEST(SocketAddressDecodeTests, IPv4RoundTrip_ByteIdenticalToPreRepairEncoding) {
    SocketAddress address(IPAddress::Loopback, 80);
    const std::vector<bytecs> expected{2, 0, 0, 80, 127, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0};
    EXPECT_EQ(rawBytes(address), expected);
    EXPECT_EQ(address.GetIPEndPoint().ToString(), "127.0.0.1:80");
}

TEST(SocketAddressDecodeTests, IPv4MaxPortRoundTrip_ByteIdenticalToPreRepairEncoding) {
    SocketAddress address(IPAddress::Broadcast, 65535);
    const std::vector<bytecs> expected{2, 0, 255, 255, 255, 255, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0};
    EXPECT_EQ(rawBytes(address), expected);
    EXPECT_EQ(address.GetIPEndPoint().ToString(), "255.255.255.255:65535");
}

TEST(SocketAddressDecodeTests, IPv6RoundTrip_ByteIdenticalToPreRepairEncoding) {
    SocketAddress address(IPAddress::IPv6Loopback, 443);
    const std::vector<bytecs> expected{23, 0, 1, 187, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                       0,  0, 0, 0,   0, 0, 0, 1, 0, 0, 0, 0};
    EXPECT_EQ(rawBytes(address), expected);
    EXPECT_EQ(address.GetIPEndPoint().ToString(), "[::1]:443");
}

TEST(SocketAddressDecodeTests, IPv6ScopedRoundTrip_KeepsScopeId) {
    SocketAddress address(IPAddress::Parse("fe80::1%7"), 9);
    const std::vector<bytecs> expected{23,  0, 0, 9, 0, 0, 0, 0, 254, 128, 0, 0, 0, 0, 0, 0,
                                       0,   0, 0, 0, 0, 0, 0, 1, 0,   0,   0, 7};
    EXPECT_EQ(rawBytes(address), expected);
    EXPECT_EQ(address.GetIPEndPoint().ToString(), "[fe80::1%7]:9");
}

// ---------------------------------------------------------------------------
// The safe path and the shortcut must not disagree.
// ---------------------------------------------------------------------------

TEST(SocketAddressDecodeTests, IPEndPointCreate_AgreesWithGetIPEndPoint) {
    SocketAddress address(IPAddress::Loopback, 8080);
    IPEndPoint prototype;
    auto created = prototype.Create(address);
    const auto* asEndPoint = dynamic_cast<const IPEndPoint*>(created.get());
    ASSERT_NE(asEndPoint, nullptr);
    EXPECT_EQ(asEndPoint->ToString(), address.GetIPEndPoint().ToString());
}

TEST(SocketAddressDecodeTests, IPEndPointCreate_AndShortcut_BothRejectAnUndersizedBuffer) {
    SocketAddress tiny(AddressFamily::InterNetwork, 2);
    IPEndPoint prototype;
    EXPECT_THROW((void)prototype.Create(tiny), ArgumentException);
    EXPECT_THROW((void)tiny.GetIPEndPoint(), ArgumentException);
}

// ---------------------------------------------------------------------------
// Everything else about SocketAddress is untouched.
// ---------------------------------------------------------------------------

TEST(SocketAddressDecodeTests, RawBufferConstructionStaysPermissive) {
    // The audit's own instruction: "Keep raw-buffer construction permissive." Only the DECODE
    // is narrowed, so a two-byte buffer of any family is still constructible and inspectable.
    for (AddressFamily family : {AddressFamily::Unix, AddressFamily::InterNetwork,
                                 AddressFamily::InterNetworkV6, AddressFamily::Unspecified}) {
        SocketAddress address(family, 2);
        EXPECT_EQ(address.getSizeProperty(), 2);
        EXPECT_EQ(address.getFamilyProperty(), family);
        EXPECT_NO_THROW((void)address.ToString());
    }
}
