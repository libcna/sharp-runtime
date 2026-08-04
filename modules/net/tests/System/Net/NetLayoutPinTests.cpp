// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// docs/SystemNetNamespaceReviewPlan.md section 10, "layout pins", and section 8's central ABI
// fact: Cookie, CookieCollection and WebUtility are HEADER-INLINE and this namespace has real
// cross-module consumers (net-sockets, net-http, net-http-json, net-websockets,
// net-network-information and the integration suite). A data member added to one of those
// header-only types is an OBJECT-LAYOUT change that no link step would report -- consumers
// simply disagree with the library about where the fields are.
//
// Every compatible ticket in this namespace (#2035-#2039, #2041) is required to leave every
// layout below untouched, and the gated #2040 explicitly warns that its option C would add a
// member to Cookie. These static_asserts are the guard: measured 2026-08-04 on the Linux/GCC
// baseline (build-tmp/2041_layout.cpp), they fail at COMPILE time the moment a layout moves.
//
// They are deliberately baseline-specific. A different ABI is free to disagree -- the point is
// that a change made HERE cannot pass unnoticed.
#include <gtest/gtest.h>
#include <cstddef>
#include "System/Net/Cookie.hpp"
#include "System/Net/CookieCollection.hpp"
#include "System/Net/IPAddress.hpp"
#include "System/Net/IPEndPoint.hpp"
#include "System/Net/SocketAddress.hpp"

using System::Net::Cookie;
using System::Net::CookieCollection;
using System::Net::IPAddress;
using System::Net::IPEndPoint;
using System::Net::SocketAddress;

#if defined(__x86_64__) && defined(__linux__) && !defined(__EMSCRIPTEN__)

static_assert(sizeof(Cookie) == 152,
              "Cookie is header-inline; changing its size is an object-layout change for every "
              "cross-module consumer. See SystemNetNamespaceReviewPlan.md section 8.1.");
static_assert(alignof(Cookie) == 8, "Cookie alignment changed");

static_assert(sizeof(CookieCollection) == 24,
              "CookieCollection is header-inline; #2041 added only a private member FUNCTION, "
              "which must not have changed the layout.");
static_assert(alignof(CookieCollection) == 8, "CookieCollection alignment changed");

static_assert(sizeof(IPAddress) == 24, "IPAddress layout changed");
static_assert(alignof(IPAddress) == 4, "IPAddress alignment changed");

static_assert(sizeof(IPEndPoint) == 40, "IPEndPoint layout changed");
static_assert(alignof(IPEndPoint) == 8, "IPEndPoint alignment changed");

static_assert(sizeof(SocketAddress) == 32, "SocketAddress layout changed");
static_assert(alignof(SocketAddress) == 8, "SocketAddress alignment changed");

TEST(NetLayoutPinTests, HeaderInlineTypeLayoutsAreUnchanged) {
    EXPECT_EQ(sizeof(Cookie), 152u);
    EXPECT_EQ(sizeof(CookieCollection), 24u);
    EXPECT_EQ(sizeof(IPAddress), 24u);
    EXPECT_EQ(sizeof(IPEndPoint), 40u);
    EXPECT_EQ(sizeof(SocketAddress), 32u);
}

#else

TEST(NetLayoutPinTests, HeaderInlineTypeLayoutsAreUnchanged) {
    GTEST_SKIP() << "layout pins are measured on the x86-64 Linux/GCC baseline only";
}

#endif
