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
#include "System/Net/CookieException.hpp"
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

// #2040 LANDED 2026-08-17. All four pins below are INVERTED, which is what the ticket said
// would happen if the rule were ever settled. It was, against the reference tree:
// Cookie.VerifyAndSetDefaults (Cookie.cs:358-424) validates an explicitly supplied Domain
// against the request URI's host and throws CookieException when it does not domain-match, and
// .NET's constructors assign through the properties (Cookie.cs:108-118), so a caller-supplied
// path or domain is never marked implicit.

TEST(NetGatedBehaviourPinTests, Pin2040_CrossOriginExplicitDomainIsREJECTED) {
    // THE DEFECT, now repaired. Measured before (build-probe/2034_probe1_net_before.log): a
    // cookie handed to the container from origin.invalid carrying Domain=.unrelated.invalid was
    // stored and handed back out for unrelated.invalid.
    CookieContainer container;
    Cookie cookie("session", "isolated");
    cookie.setDomainProperty(".unrelated.invalid");

    EXPECT_THROW(container.Add(Uri("http://origin.invalid/"), cookie),
                 System::Net::CookieException);
    EXPECT_EQ(container.getCountProperty(), 0) << "a rejected cookie must not be stored";
    EXPECT_EQ(container.GetCookieHeader(Uri("http://unrelated.invalid/")), "");
}

TEST(NetGatedBehaviourPinTests, Pin2040_TheRejectionCarriesDotNetsMessage) {
    CookieContainer container;
    Cookie cookie("session", "isolated");
    cookie.setDomainProperty(".unrelated.invalid");
    try {
        container.Add(Uri("http://origin.invalid/"), cookie);
        FAIL() << "expected CookieException";
    } catch (const System::Net::CookieException& e) {
        // SR.net_cookie_attribute is "The '{0}'='{1}' part of the cookie is invalid."
        // (System.Net.Primitives/src/Resources/Strings.resx:93).
        EXPECT_EQ(std::string(e.what()),
                  "The 'Domain'='.unrelated.invalid' part of the cookie is invalid.");
    }
}

TEST(NetGatedBehaviourPinTests, Pin2040_AnExplicitDomainTHATDoesMatchIsStillAccepted) {
    // The repair must not reject the legitimate case it exists to permit: a cookie scoped to a
    // parent domain of its own origin. This is the assertion that would catch a guard that
    // simply rejected every explicit domain.
    CookieContainer container;
    Cookie cookie("session", "shared");
    cookie.setDomainProperty(".example.invalid");
    container.Add(Uri("http://www.example.invalid/"), cookie);

    EXPECT_EQ(container.getCountProperty(), 1);
    EXPECT_EQ(container.GetCookieHeader(Uri("http://api.example.invalid/")), "session=shared");
    EXPECT_EQ(container.GetCookieHeader(Uri("http://www.example.invalid/")), "session=shared");
}

TEST(NetGatedBehaviourPinTests, Pin2040_SingleLabelDomainsAndIpHostsNeedAnExactMatch) {
    // The two compatibility conditions .NET adds on top of RFC 6265 (Cookie.cs:347-349):
    // the domain must itself contain a dot, and the host must not be an IP literal. Without
    // them, Domain=invalid would attach to every *.invalid host and Domain=1.2.3 to 1.2.3.4.
    {
        CookieContainer container;
        Cookie tooBroad("n", "v");
        tooBroad.setDomainProperty("invalid");
        EXPECT_THROW(container.Add(Uri("http://origin.invalid/"), tooBroad),
                     System::Net::CookieException);
    }
    {
        CookieContainer container;
        Cookie ipSuffix("n", "v");
        ipSuffix.setDomainProperty("2.3.4");
        EXPECT_THROW(container.Add(Uri("http://1.2.3.4/"), ipSuffix),
                     System::Net::CookieException);
    }
    {
        // ...and the exact match those two conditions still permit.
        CookieContainer container;
        Cookie exact("n", "v");
        exact.setDomainProperty("1.2.3.4");
        EXPECT_NO_THROW(container.Add(Uri("http://1.2.3.4/"), exact));
        EXPECT_EQ(container.GetCookieHeader(Uri("http://1.2.3.4/")), "n=v");
    }
}

TEST(NetGatedBehaviourPinTests, Pin2040_ConstructorSuppliedPathIsEXPLICIT) {
    // The constructor and the setter now agree about the same value. That flag is the INPUT to
    // the domain rule above, which is why SR-AUD-305 and SR-AUD-306 were one decision.
    const Cookie viaConstructor("n", "v", "/explicit");
    EXPECT_EQ(viaConstructor.getPathProperty(), "/explicit");
    EXPECT_FALSE(viaConstructor.getPathImplicitProperty());

    Cookie viaSetter("n", "v");
    viaSetter.setPathProperty("/explicit");
    EXPECT_EQ(viaSetter.getPathProperty(), "/explicit");
    EXPECT_FALSE(viaSetter.getPathImplicitProperty());
}

TEST(NetGatedBehaviourPinTests, Pin2040_ConstructorSuppliedDomainIsEXPLICIT) {
    const Cookie viaConstructor("n", "v", "/p", ".d.invalid");
    EXPECT_EQ(viaConstructor.getDomainProperty(), ".d.invalid");
    EXPECT_FALSE(viaConstructor.getDomainImplicitProperty());

    Cookie viaSetter("n", "v");
    viaSetter.setDomainProperty(".d.invalid");
    EXPECT_EQ(viaSetter.getDomainProperty(), ".d.invalid");
    EXPECT_FALSE(viaSetter.getDomainImplicitProperty());
}

TEST(NetGatedBehaviourPinTests, Pin2040_ContainerKEEPSConstructorSuppliedPathAndDomain) {
    // The consequence of the two cases above, and the user-visible half of the repair: because
    // the flags are now cleared, Add() no longer replaces the caller's own values with the
    // request URI's. The domain is chosen to domain-match the origin so the new validation
    // accepts it -- ".d.invalid" from the original pin would now be rejected, which is the point.
    CookieContainer container;
    container.Add(Uri("http://sub.origin.invalid/some/where"),
                  Cookie("n", "v", "/explicit", ".origin.invalid"));

    const CookieCollection stored = container.GetCookies(Uri("http://sub.origin.invalid/explicit"));
    ASSERT_EQ(stored.getCountProperty(), 1);
    EXPECT_EQ(stored[0].getPathProperty(), "/explicit");
    EXPECT_EQ(stored[0].getDomainProperty(), ".origin.invalid");
}

TEST(NetGatedBehaviourPinTests, Pin2040_AnImplicitDomainStaysImplicitAfterBeingDefaulted) {
    // .NET's SetDomainAndKey writes the domain and leaves m_domain_implicit alone
    // (Cookie.cs:310-314): the flag records where the value CAME FROM, so defaulting it from the
    // request URI must not make the cookie claim the caller chose it.
    CookieContainer container;
    container.Add(Uri("http://origin.invalid/a/b"), Cookie("n", "v"));

    const CookieCollection stored = container.GetCookies(Uri("http://origin.invalid/a/b"));
    ASSERT_EQ(stored.getCountProperty(), 1);
    EXPECT_EQ(stored[0].getDomainProperty(), "origin.invalid");
    EXPECT_TRUE(stored[0].getDomainImplicitProperty());
    EXPECT_TRUE(stored[0].getPathImplicitProperty());
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
