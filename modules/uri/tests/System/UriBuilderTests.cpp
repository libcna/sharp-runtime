// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/UriBuilder.hpp"
#include "System/ArgumentException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/UriFormatException.hpp"

using System::UriBuilder;
using System::ArgumentOutOfRangeException;
using System::UriFormatException;

TEST(UriBuilderTest, DefaultCtor) {
    UriBuilder b;
    EXPECT_EQ(b.getSchemeProperty(), "http");
    EXPECT_EQ(b.getPathProperty(), "/");
}

TEST(UriBuilderTest, DefaultCtor_HostIsLocalhost) {
    // .NET's UriBuilder() default host is "localhost", not empty.
    UriBuilder b;
    EXPECT_EQ(b.getHostProperty(), "localhost");
    EXPECT_EQ(b.ToString(), "http://localhost/");
}

TEST(UriBuilderTest, CtorFromString) {
    UriBuilder b("http://example.com/path");
    EXPECT_EQ(b.getSchemeProperty(), "http");
    EXPECT_EQ(b.getHostProperty(), "example.com");
}

TEST(UriBuilderTest, SetScheme) {
    UriBuilder b;
    b.setSchemeProperty("https");
    EXPECT_EQ(b.getSchemeProperty(), "https");
}

TEST(UriBuilderTest, SetHost) {
    UriBuilder b;
    b.setHostProperty("example.org");
    EXPECT_EQ(b.getHostProperty(), "example.org");
}

TEST(UriBuilderTest, SetPath) {
    UriBuilder b;
    b.setPathProperty("/api/v1");
    EXPECT_EQ(b.getPathProperty(), "/api/v1");
}

TEST(UriBuilderTest, SetQuery) {
    UriBuilder b;
    b.setQueryProperty("?key=value");
    EXPECT_FALSE(b.getQueryProperty().empty());
}

TEST(UriBuilderTest, ToUri) {
    UriBuilder b;
    b.setSchemeProperty("https");
    b.setHostProperty("example.com");
    b.setPathProperty("/test");
    auto uri = b.getUriProperty();
    EXPECT_NE(uri.getAbsoluteUriProperty().find("example.com"), std::string::npos);
}

TEST(UriBuilderTest, ToStringNotEmpty) {
    UriBuilder b;
    b.setHostProperty("localhost");
    EXPECT_FALSE(b.ToString().empty());
}

TEST(UriBuilderTest, SchemeHostCtor) {
    UriBuilder b("https", "example.com");
    EXPECT_EQ(b.getSchemeProperty(), "https");
    EXPECT_EQ(b.getHostProperty(), "example.com");
}

TEST(UriBuilderTest, SchemeHostPortCtor) {
    UriBuilder b("https", "example.com", 8443);
    EXPECT_EQ(b.getPortProperty(), 8443);
}

TEST(UriBuilderTest, SchemeHostPortPathCtor) {
    UriBuilder b("https", "example.com", 8443, "api/v1");
    EXPECT_EQ(b.getPathProperty(), "api/v1");
}

TEST(UriBuilderTest, SchemeHostPortPathQueryExtraCtor) {
    UriBuilder b("https", "example.com", -1, "/x", "?a=1");
    EXPECT_EQ(b.getQueryProperty(), "?a=1");
    EXPECT_TRUE(b.getFragmentProperty().empty());
}

TEST(UriBuilderTest, SchemeHostPortPathFragmentExtraCtor) {
    UriBuilder b("https", "example.com", -1, "/x", "#frag");
    EXPECT_EQ(b.getFragmentProperty(), "#frag");
    EXPECT_TRUE(b.getQueryProperty().empty());
}

TEST(UriBuilderTest, ExtraValue_InvalidPrefix_Throws) {
    EXPECT_THROW(UriBuilder("https", "example.com", -1, "/x", "bad"), System::ArgumentException);
}

TEST(UriBuilderTest, Port_OutOfRange_Throws) {
    UriBuilder b;
    EXPECT_THROW(b.setPortProperty(-2), ArgumentOutOfRangeException);
    EXPECT_THROW(b.setPortProperty(70000), ArgumentOutOfRangeException);
}

TEST(UriBuilderTest, Port_MinusOne_Allowed) {
    UriBuilder b;
    EXPECT_NO_THROW(b.setPortProperty(-1));
}

TEST(UriBuilderTest, SetPath_Empty_NormalizesToSlash) {
    UriBuilder b;
    b.setPathProperty("");
    EXPECT_EQ(b.getPathProperty(), "/");
}

TEST(UriBuilderTest, ToString_PasswordWithoutUsername_Throws) {
    UriBuilder b;
    b.setPasswordProperty("secret");
    EXPECT_THROW(b.ToString(), UriFormatException);
}

TEST(UriBuilderTest, Equals_SameComponents) {
    UriBuilder a("https", "example.com", -1, "/x");
    UriBuilder b("https", "example.com", -1, "/x");
    EXPECT_TRUE(a.Equals(b));
}

TEST(UriBuilderTest, GetHashCode_Consistent) {
    UriBuilder a("https", "example.com", -1, "/x");
    UriBuilder b("https", "example.com", -1, "/x");
    EXPECT_EQ(a.GetHashCode(), b.GetHashCode());
}

// ---------------------------------------------------------------------------
// Query/Fragment setter normalization (regression: real .NET's UriBuilder.Query/
// Fragment setters prepend '?'/'#' when missing, so the getter always returns the
// prefixed form -- this port previously stored the raw value verbatim.
// ---------------------------------------------------------------------------

TEST(UriBuilderTest, SetQuery_WithoutLeadingQuestionMark_Normalizes) {
    UriBuilder b;
    b.setQueryProperty("foo=bar");
    EXPECT_EQ(b.getQueryProperty(), "?foo=bar");
}

TEST(UriBuilderTest, SetQuery_WithLeadingQuestionMark_Unchanged) {
    UriBuilder b;
    b.setQueryProperty("?foo=bar");
    EXPECT_EQ(b.getQueryProperty(), "?foo=bar");
}

TEST(UriBuilderTest, SetQuery_Empty_StaysEmpty) {
    UriBuilder b;
    b.setQueryProperty("");
    EXPECT_TRUE(b.getQueryProperty().empty());
}

TEST(UriBuilderTest, SetFragment_WithoutLeadingHash_Normalizes) {
    UriBuilder b;
    b.setFragmentProperty("section1");
    EXPECT_EQ(b.getFragmentProperty(), "#section1");
}

TEST(UriBuilderTest, SetFragment_WithLeadingHash_Unchanged) {
    UriBuilder b;
    b.setFragmentProperty("#section1");
    EXPECT_EQ(b.getFragmentProperty(), "#section1");
}

TEST(UriBuilderTest, SetFragment_Empty_StaysEmpty) {
    UriBuilder b;
    b.setFragmentProperty("");
    EXPECT_TRUE(b.getFragmentProperty().empty());
}

// ---------------------------------------------------------------------------
// User-info split on copy — ticket #1993 (cause U-F, SR-AUD-138). setFieldsFromUri put the
// whole user-info in userName_ and never populated password_, although the type publishes
// UserName and Password separately, so replacing the Password left the original
// credentials in the serialized URI. See docs/SystemUriNamespaceReviewPlan.md §25.
// ---------------------------------------------------------------------------

TEST(UriBuilderTest, CopiedUserInfo_SplitsUserAndPassword) {
    UriBuilder b("http://user:pass@example.com/path");
    EXPECT_EQ(b.getUserNameProperty(), "user");
    EXPECT_EQ(b.getPasswordProperty(), "pass");
}

TEST(UriBuilderTest, CopiedUserInfo_RoundTripsUnchanged) {
    UriBuilder b("http://user:pass@example.com/path");
    EXPECT_EQ(b.ToString(), "http://user:pass@example.com:80/path");
}

TEST(UriBuilderTest, CopiedUserInfo_ReplacingPasswordDoesNotCorrupt) {
    // The shape SR-AUD-138 names: before the repair this serialized
    // "http://user:pass:replacement@example.com:80/path".
    UriBuilder b("http://user:pass@example.com/path");
    b.setPasswordProperty("replacement");
    EXPECT_EQ(b.ToString(), "http://user:replacement@example.com:80/path");
}

TEST(UriBuilderTest, CopiedUserInfo_UserOnly) {
    UriBuilder b("http://user@example.com/path");
    EXPECT_EQ(b.getUserNameProperty(), "user");
    EXPECT_TRUE(b.getPasswordProperty().empty());
    EXPECT_EQ(b.ToString(), "http://user@example.com:80/path");
}

TEST(UriBuilderTest, CopiedUserInfo_None) {
    UriBuilder b("http://example.com/path");
    EXPECT_TRUE(b.getUserNameProperty().empty());
    EXPECT_TRUE(b.getPasswordProperty().empty());
    EXPECT_EQ(b.ToString(), "http://example.com:80/path");
}

TEST(UriBuilderTest, CopiedUserInfo_PasswordContainingColon_SplitsAtTheFirst) {
    UriBuilder b("http://u:p:q@example.com/path");
    EXPECT_EQ(b.getUserNameProperty(), "u");
    EXPECT_EQ(b.getPasswordProperty(), "p:q");
    EXPECT_EQ(b.ToString(), "http://u:p:q@example.com:80/path");
}

TEST(UriBuilderTest, CopiedUserInfo_EmptyPassword_DropsTheTrailingColon) {
    // The ONE input whose rendering changes: "user:" used to be stored whole and emitted
    // as "user:@", and is now UserName "user" with an empty Password, emitted as "user@".
    // That matches .NET's UriBuilder, which splits at the first colon on copy and appends
    // ":" + password only when the password is non-empty.
    UriBuilder b("http://user:@example.com/p");
    EXPECT_EQ(b.getUserNameProperty(), "user");
    EXPECT_TRUE(b.getPasswordProperty().empty());
    EXPECT_EQ(b.ToString(), "http://user@example.com:80/p");
}

TEST(UriBuilderTest, CopiedUserInfo_FromUriOverload_SplitsToo) {
    // The Uri-taking constructor shares setFieldsFromUri and must behave identically.
    UriBuilder b(System::Uri("ftp://user:pass@ftp.example.com/f.txt"));
    EXPECT_EQ(b.getUserNameProperty(), "user");
    EXPECT_EQ(b.getPasswordProperty(), "pass");
}

// ---------------------------------------------------------------------------
// Equals / GetHashCode consistency — ticket #2004 (post-audit defect, no SR-AUD identifier,
// recorded in docs/SystemUriNamespaceReviewPlan.md §4.5 and §27).
//
// Equals() compares the rendered strings and never parses; GetHashCode() used to build a
// whole Uri from the same string purely to hash it. For any builder whose rendering is not a
// parseable URI the two disagreed at the worst possible place — b.Equals(b) returned true
// while b.GetHashCode() threw, so an object that compared equal to itself had no obtainable
// hash. The repair hashes the rendered string directly. It is value-identical wherever the
// old code returned at all, because Uri::parse assigns absoluteUri_ = uriString on every
// accepted branch and Uri::GetHashCode hashes exactly that.
//
// This closes ONLY the internal asymmetry. Identity is still raw rendered text: a
// case-differing or default-port-differing pair is still unequal, which is SR-AUD-142 /
// SR-AUD-140 and stays approval-gated as ticket #1995. The deliberately-unequal pairs below
// are pinned so that landing #1995 is a visible change rather than a silent one.
// ---------------------------------------------------------------------------

namespace {
    System::intcs hashOf(const UriBuilder& b) { return b.GetHashCode(); }

    // a.Equals(b) => hash(a) == hash(b), asserted for every shape the type can hold.
    void expectEqualsImpliesEqualHash(const UriBuilder& a, const UriBuilder& b,
                                      const char* what) {
        if (a.Equals(b)) {
            EXPECT_EQ(hashOf(a), hashOf(b)) << "equal builders must hash equally: " << what;
        }
    }
}

TEST(UriBuilderTest, HashIsObtainableWhereverEqualsSucceeds_MalformedPort) {
    UriBuilder b;
    b.setHostProperty("h:abc");
    ASSERT_EQ(b.ToString(), "http://h:abc/");
    EXPECT_THROW(b.getUriProperty(), UriFormatException); // the string is still unparseable
    EXPECT_TRUE(b.Equals(b));
    EXPECT_NO_THROW((void)b.GetHashCode());
    expectEqualsImpliesEqualHash(b, b, "malformed port");
}

TEST(UriBuilderTest, HashIsObtainableWhereverEqualsSucceeds_OtherUnparseableRenderings) {
    // The three remaining measured routes: an out-of-range port, an unterminated IP literal
    // (#1991) and an empty host (#2000). Each renders a string Uri rejects.
    const char* hosts[] = {"h:99999", "[::1", ""};
    for (const char* host : hosts) {
        UriBuilder b;
        b.setHostProperty(host);
        EXPECT_THROW(b.getUriProperty(), UriFormatException) << host;
        EXPECT_TRUE(b.Equals(b)) << host;
        EXPECT_NO_THROW((void)b.GetHashCode()) << host;
        UriBuilder same;
        same.setHostProperty(host);
        EXPECT_TRUE(b.Equals(same)) << host;
        EXPECT_EQ(b.GetHashCode(), same.GetHashCode()) << host;
    }
}

TEST(UriBuilderTest, HashIsValueIdenticalWhereTheOldRouteSucceeded) {
    // The compatibility claim, asserted rather than argued: for every builder whose
    // rendering parses, hashing the string and hashing the built Uri give the same number.
    UriBuilder shapes[6];
    shapes[0].setHostProperty("example.com");
    shapes[1].setHostProperty("example.com"); shapes[1].setPortProperty(8080);
    shapes[2].setHostProperty("example.com"); shapes[2].setUserNameProperty("u");
                                              shapes[2].setPasswordProperty("p");
    shapes[3].setHostProperty("example.com"); shapes[3].setQueryProperty("a=1");
    shapes[4].setHostProperty("example.com"); shapes[4].setFragmentProperty("f");
    shapes[5].setHostProperty("[::1]");       shapes[5].setPortProperty(443);
    for (const UriBuilder& b : shapes)
        EXPECT_EQ(b.GetHashCode(), b.getUriProperty().GetHashCode()) << b.ToString();
}

TEST(UriBuilderTest, EqualsAndGetHashCodeAreTotalOnTheSameSet) {
    // The single remaining condition under which either operation fails is ToString()
    // itself failing, and then BOTH fail with the same exception — no asymmetry survives.
    UriBuilder b;
    b.setPasswordProperty("secret"); // user name empty, password not
    EXPECT_THROW(b.ToString(), UriFormatException);
    EXPECT_THROW((void)b.Equals(b), UriFormatException);
    EXPECT_THROW((void)b.GetHashCode(), UriFormatException);
}

TEST(UriBuilderTest, EqualsImpliesEqualHashAcrossEveryEqualityClass) {
    UriBuilder plain;      plain.setHostProperty("example.com");
    UriBuilder withPort;   withPort.setHostProperty("example.com");  withPort.setPortProperty(8080);
    UriBuilder withCreds;  withCreds.setHostProperty("example.com"); withCreds.setUserNameProperty("u");
                           withCreds.setPasswordProperty("p");
    UriBuilder withQuery;  withQuery.setHostProperty("example.com"); withQuery.setQueryProperty("a=1");
    UriBuilder withFrag;   withFrag.setHostProperty("example.com");  withFrag.setFragmentProperty("f");
    UriBuilder emptyQuery; emptyQuery.setHostProperty("example.com");emptyQuery.setQueryProperty("");
    UriBuilder badPort;    badPort.setHostProperty("h:abc");
    UriBuilder emptyHost;  emptyHost.setHostProperty("");
    UriBuilder fileShape("file", "");  fileShape.setPathProperty("/tmp/x");

    const UriBuilder* shapes[] = {&plain, &withPort, &withCreds, &withQuery, &withFrag,
                                  &emptyQuery, &badPort, &emptyHost, &fileShape};
    for (const UriBuilder* a : shapes) {
        for (const UriBuilder* b : shapes)
            expectEqualsImpliesEqualHash(*a, *b, "cross product");
        // and against an independently built copy of the same shape
        UriBuilder copy = *a;
        EXPECT_TRUE(a->Equals(copy));
        EXPECT_EQ(a->GetHashCode(), copy.GetHashCode());
    }
}

TEST(UriBuilderTest, DeliberatelyUnequalPairsStayUnequal_PinsTheGatedIdentityChange) {
    // These are the pairs #1995 would make EQUAL. They must stay unequal until that approval
    // lands, and this test must be updated with it — the same role
    // UriTests.DocumentedContract_CaseDifferingUrisAreNotEqualYet plays for Uri.
    UriBuilder lower;  lower.setHostProperty("example.com");
    UriBuilder upperHost; upperHost.setHostProperty("EXAMPLE.COM");
    UriBuilder upperScheme; upperScheme.setSchemeProperty("HTTP"); upperScheme.setHostProperty("example.com");
    UriBuilder defaultPort; defaultPort.setHostProperty("example.com"); defaultPort.setPortProperty(80);

    EXPECT_FALSE(lower.Equals(upperHost));
    EXPECT_FALSE(lower.Equals(upperScheme));
    EXPECT_FALSE(lower.Equals(defaultPort));
    // The three matching hash inequalities were removed by #2284. They restated the gate on a
    // channel that does not carry it: unequal values are permitted to hash equally
    // (docs/HashAssertionContractRule.md R2), so only the Equals assertions above are the pin,
    // and only they have to be updated when #1995 lands.
}

TEST(UriBuilderTest, UriIdentityItselfIsUnchangedByThisTicket) {
    // Uri's own operator== and GetHashCode were already paired (both read absoluteUri_) and
    // this ticket does not touch them. Pinned so a later reader does not mistake #2004 for
    // an identity change.
    System::Uri a("http://example.com/p");
    System::Uri b("http://example.com/p");
    System::Uri caseDiff("HTTP://EXAMPLE.COM/p");
    System::Uri explicitDefaultPort("http://example.com:80/p");
    // Equal Uris hash equally -- the contract direction, kept. The two matching inequalities for
    // the unequal pairs were removed by #2284: a collision between unequal values is legal
    // (docs/HashAssertionContractRule.md R2), so the operator== assertions carry the pin.
    EXPECT_TRUE(a == b);
    EXPECT_EQ(a.GetHashCode(), b.GetHashCode());
    EXPECT_FALSE(a == caseDiff);
    EXPECT_FALSE(a == explicitDefaultPort);
}

TEST(UriBuilderTest, Layout_SizeOfUriBuilderIsPinned) {
    // docs/SystemUriNamespaceReviewPlan.md §9.2 claimed sizeof(System::UriBuilder) == 232 was
    // "re-asserted by permanent tests"; measured on 2026-08-03 no such test existed. Added
    // here so the claim becomes true, alongside UriTests.Layout_SizeOfUriIsPinned.
    EXPECT_EQ(sizeof(System::UriBuilder), 232u);
}
