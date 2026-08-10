// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Ticket #2047 -- the mandatory disclosure-and-pins ticket of
// docs/SystemNetNamespaceReviewPlan.md §13 item 7 and §15 completion criterion 2.
//
// This file pins the CURRENT behaviour of the four approval-gated System::Net decisions so none
// of them can land silently, and so a reviewer reading the approval package can see exactly what
// changes. NOTHING here endorses the current behaviour -- three of the four are defects. The
// lesson these pins come from is #2022/#2028: two gated findings in System::Text had no test at
// all, so their repairs could have shipped without any suite noticing.
//
//   #2040 (SR-AUD-305 + SR-AUD-306, cause N-E) -- a cookie supplied by one origin with an
//         unrelated explicit Domain is stored and later emitted for that unrelated domain; and
//         Cookie's path/domain-accepting constructors leave the implicit flags SET, so container
//         insertion overwrites what the caller passed.
//   #2042 (SR-AUD-308, cause N-F) -- storage is unbounded: no capacity, per-domain capacity,
//         maximum size, expiry cleanup or eviction.
//   #2043 (SR-AUD-304's wildcard half, cause N-C) -- pinned separately in
//         DnsLiteralAndDuplicateTests.cpp by #2039.
//   #2044 (SR-AUD-309, cause N-G) -- HtmlEncode escapes exactly five ASCII characters and passes
//         every non-ASCII byte through, while HtmlDecode already understands numeric character
//         references and named entities the encoder never produces.
//
// Every pin below was mutation-checked: the proposed repair was applied temporarily and the pin
// was observed to FAIL, then reverted. The evidence is recorded in the plan's §17.7.
#include <gtest/gtest.h>
#include <string>
#include "System/Net/Cookie.hpp"
#include "System/Net/CookieCollection.hpp"
#include "System/Net/CookieContainer.hpp"
#include "System/Net/WebUtility.hpp"
#include "System/Uri.hpp"

using System::Net::Cookie;
using System::Net::CookieCollection;
using System::Net::CookieContainer;
using System::Net::WebUtility;
using System::Uri;

// ===========================================================================
// #2040 -- the cookie origin policy. Approval sentence: plan §14.1.
// ===========================================================================

TEST(NetGatedBehaviourPinTests, Pin2040_CrossOriginExplicitDomainIsStoredAndEmitted) {
    // Measured 2026-08-04 (build-probe/2034_probe1_net_before.log). A cookie handed to the
    // container from origin.invalid, carrying Domain=.unrelated.invalid, comes back out for
    // unrelated.invalid. THIS IS THE DEFECT, pinned so #2040 cannot land unnoticed.
    CookieContainer container;
    Cookie cookie("session", "isolated");
    cookie.setDomainProperty(".unrelated.invalid");
    container.Add(Uri("http://origin.invalid/"), cookie);

    EXPECT_EQ(container.GetCookieHeader(Uri("http://unrelated.invalid/")), "session=isolated");
    EXPECT_EQ(container.getCountProperty(), 1);
}

TEST(NetGatedBehaviourPinTests, Pin2040_ConstructorSuppliedPathIsMarkedImplicit) {
    // The constructor and the setter disagree about the same value: only the setter clears the
    // flag. That flag is the INPUT to the domain-matching rule #2040 must define, which is why
    // SR-AUD-305 and SR-AUD-306 are one decision.
    const Cookie viaConstructor("n", "v", "/explicit");
    EXPECT_EQ(viaConstructor.getPathProperty(), "/explicit");
    EXPECT_TRUE(viaConstructor.getPathImplicitProperty());

    Cookie viaSetter("n", "v");
    viaSetter.setPathProperty("/explicit");
    EXPECT_EQ(viaSetter.getPathProperty(), "/explicit");
    EXPECT_FALSE(viaSetter.getPathImplicitProperty());
}

TEST(NetGatedBehaviourPinTests, Pin2040_ConstructorSuppliedDomainIsMarkedImplicit) {
    const Cookie viaConstructor("n", "v", "/p", ".d.invalid");
    EXPECT_EQ(viaConstructor.getDomainProperty(), ".d.invalid");
    EXPECT_TRUE(viaConstructor.getDomainImplicitProperty());

    Cookie viaSetter("n", "v");
    viaSetter.setDomainProperty(".d.invalid");
    EXPECT_EQ(viaSetter.getDomainProperty(), ".d.invalid");
    EXPECT_FALSE(viaSetter.getDomainImplicitProperty());
}

TEST(NetGatedBehaviourPinTests, Pin2040_ContainerOverwritesConstructorSuppliedPathAndDomain) {
    // The consequence of the two pins above: because the flags stay set, Add() replaces the
    // caller's own explicit values with the request URI's.
    CookieContainer container;
    container.Add(Uri("http://origin.invalid/some/where"), Cookie("n", "v", "/explicit", ".d.invalid"));

    const CookieCollection stored = container.GetCookies(Uri("http://origin.invalid/some/where"));
    ASSERT_EQ(stored.getCountProperty(), 1);
    EXPECT_EQ(stored[0].getPathProperty(), "/some/where");
    EXPECT_EQ(stored[0].getDomainProperty(), "origin.invalid");
}

// ===========================================================================
// #2042 -- the storage bound. Approval sentence: plan §14.2.
// ===========================================================================

TEST(NetGatedBehaviourPinTests, Pin2042_StorageIsUnbounded) {
    // 2,000 rather than the audit's 10,000 purely for runtime -- the container scans linearly on
    // every Add, so the cost is quadratic. 2,000 already exceeds any plausible default capacity
    // by an order of magnitude, so it pins the same fact: there is no bound at all.
    CookieContainer container;
    const Uri origin("http://origin.invalid/");
    for (int i = 0; i < 2000; ++i) {
        container.Add(origin, Cookie("n" + std::to_string(i), "v" + std::to_string(i)));
    }
    EXPECT_EQ(container.getCountProperty(), 2000);
}

TEST(NetGatedBehaviourPinTests, Pin2042_ExpiredCookiesAreRetainedNotPurged) {
    // No expiry cleanup either: an expired cookie stays in storage and only the EMISSION filter
    // hides it. Count is the observable that #2042's "clean expired cookies on insertion" option
    // would change.
    CookieContainer container;
    const Uri origin("http://origin.invalid/");
    Cookie expired("gone", "v");
    expired.setExpiresProperty(System::DateTime(2000, 1, 1));
    container.Add(origin, expired);
    container.Add(origin, Cookie("kept", "v"));

    EXPECT_EQ(container.getCountProperty(), 2);
    EXPECT_EQ(container.GetCookieHeader(origin), "kept=v");
}

// ===========================================================================
// #2044 -- the HTML escaping policy. Approval sentence: plan §14.4, and it is
// coupled to System::Text's #2019: two HTML encoders in one repository must not
// diverge. DEFERRED, not merely blocked.
// ===========================================================================

TEST(NetGatedBehaviourPinTests, Pin2044_HtmlEncodeEscapesExactlyFiveAsciiCharacters) {
    EXPECT_EQ(WebUtility::HtmlEncode("<a href=\"x\">&'"), "&lt;a href=&quot;x&quot;&gt;&amp;&#39;");
    // Every other ASCII character passes through untouched, including ones .NET's encoder
    // leaves alone too -- pinned so the "exactly five" claim is a measurement, not a summary.
    EXPECT_EQ(WebUtility::HtmlEncode("azAZ09 -_.!*()~`@#$%^+=[]{}|\\:;,/?"),
              "azAZ09 -_.!*()~`@#$%^+=[]{}|\\:;,/?");
}

TEST(NetGatedBehaviourPinTests, Pin2044_HtmlEncodePassesEveryNonAsciiBytethrough) {
    // The gated half: .NET numeric-entity-encodes U+00A0-U+00FF and non-BMP characters. This
    // port passes the raw UTF-8 bytes through unchanged.
    const std::string latin1Supplement = "caf\xC3\xA9";        // café, U+00E9
    const std::string euro = "\xE2\x82\xAC";                    // U+20AC
    const std::string nonBmp = "\xF0\x9F\x98\x80";              // U+1F600
    const std::string nbsp = "\xC2\xA0";                        // U+00A0
    EXPECT_EQ(WebUtility::HtmlEncode(latin1Supplement), latin1Supplement);
    EXPECT_EQ(WebUtility::HtmlEncode(euro), euro);
    EXPECT_EQ(WebUtility::HtmlEncode(nonBmp), nonBmp);
    EXPECT_EQ(WebUtility::HtmlEncode(nbsp), nbsp);
}

TEST(NetGatedBehaviourPinTests, Pin2044_HtmlDecodeUnderstandsMoreThanTheEncoderProduces) {
    // Plan §4.4's premise correction, pinned: SR-AUD-309 describes the ENCODE direction only.
    // The decoder already handles decimal and hexadecimal character references and named
    // entities the encoder never emits -- so the finding is an ASYMMETRY, and a repair that
    // widens only the encoder still has to say what the decoder accepts.
    const std::string copyright = "\xC2\xA9";                   // U+00A9
    EXPECT_EQ(WebUtility::HtmlDecode("&copy;"), copyright);
    EXPECT_EQ(WebUtility::HtmlDecode("&#169;"), copyright);
    EXPECT_EQ(WebUtility::HtmlDecode("&#xA9;"), copyright);
    EXPECT_EQ(WebUtility::HtmlDecode("&nbsp;"), std::string("\xC2\xA0"));
    EXPECT_EQ(WebUtility::HtmlDecode("&reg;"), std::string("\xC2\xAE"));
    EXPECT_EQ(WebUtility::HtmlDecode("&trade;"), std::string("\xE2\x84\xA2"));
    // ...and an entity it does NOT know is left literal rather than dropped.
    EXPECT_EQ(WebUtility::HtmlDecode("&hearts;"), "&hearts;");
    EXPECT_EQ(WebUtility::HtmlDecode("&#zz;"), "&#zz;");
}

TEST(NetGatedBehaviourPinTests, Pin2044_EncodeThenDecodeRoundTripsTheFiveEscapedCharacters) {
    const std::string original = "<a href=\"x\">&'";
    EXPECT_EQ(WebUtility::HtmlDecode(WebUtility::HtmlEncode(original)), original);
}
