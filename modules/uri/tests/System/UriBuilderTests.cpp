// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <functional>
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
    // #1996 G-1 changed the rendering here, and it is .NET's: a host containing ':' is a
    // "probable ipv6 address" (UriBuilder.cs:176) and is bracketed, so "h:abc" renders
    // "http://[h:abc]/" rather than "http://h:abc/". The point of THIS test is unaffected --
    // the string is still one Uri rejects, so Equals and GetHashCode must still work on it.
    UriBuilder b;
    b.setHostProperty("h:abc");
    ASSERT_EQ(b.ToString(), "http://[h:abc]/");
    // The rendering is now one Uri ACCEPTS, because .NET's bracketing turned a malformed port
    // into a (content-unvalidated) literal. The property this test exists for is unchanged and
    // is asserted on a shape that is still unparseable, below.
    EXPECT_NO_THROW((void)b.getUriProperty());
    EXPECT_TRUE(b.Equals(b));
    EXPECT_NO_THROW((void)b.GetHashCode());
    expectEqualsImpliesEqualHash(b, b, "bracketed host");
}

TEST(UriBuilderTest, Fix2391_AnUnparseableBuilderNowThrowsFromBothMembers) {
    // INVERTED BY #2391. Its predecessor asserted the OPPOSITE -- that Equals and GetHashCode
    // both answer for a builder whose rendering Uri rejects -- which was #2004's non-throwing
    // guarantee. The user withdrew that guarantee on 2026-08-19 to take .NET's delegation
    // (UriBuilder.cs:277-279), knowing this exact cost, so the pin records the new contract.
    //
    // Two measured routes survive: an unterminated IP literal (#1991), which #1996 G-1
    // deliberately does NOT wrap, and an empty host (#2000). ("h:abc" and "h:99999" left this
    // set with G-1: they are bracketed now and parse.)
    const char* hosts[] = {"[::1", ""};
    for (const char* host : hosts) {
        UriBuilder b;
        b.setHostProperty(host);
        EXPECT_THROW(b.getUriProperty(), UriFormatException) << host;

        // b.Equals(b) THROWS. This is the sharp edge of the decision and it is asserted rather
        // than glossed: an object that cannot be compared to itself is exactly what #2004
        // removed, and #2391 accepted its return in exchange for canonical identity.
        EXPECT_THROW((void)b.Equals(b), UriFormatException) << host;
        EXPECT_THROW((void)b.GetHashCode(), UriFormatException) << host;

        // Totality is what #2004 was really protecting, and it is PRESERVED -- just over a
        // smaller set. The two members now throw on exactly the same objects, so there is no
        // builder that compares equal to itself yet has no obtainable hash.
        UriBuilder same;
        same.setHostProperty(host);
        EXPECT_THROW((void)b.Equals(same), UriFormatException) << host;
        EXPECT_THROW((void)same.GetHashCode(), UriFormatException) << host;
    }
}

TEST(UriBuilderTest, Fix2391_BuilderHashIsTheBuiltUrisHash) {
    // INVERTED BY #2391, and it is the second inversion of this pin in one session -- worth
    // stating, because the two moves are not a reversal, they are a convergence.
    //   #2004 made the builder hash its RENDERED STRING, arguing that was value-identical to
    //         delegating, since Uri::GetHashCode then hashed absoluteUri_ verbatim.
    //   #1995 made Uri::GetHashCode hash a CANONICAL IDENTITY KEY, which silently falsified
    //         that argument -- this pin was rewritten to record the resulting disagreement.
    //   #2391 restores .NET's delegation (UriBuilder.cs:279), so they agree again, by
    //         construction rather than by coincidence.
    UriBuilder b;
    b.setHostProperty("EXAMPLE.COM");
    b.setPortProperty(80);
    EXPECT_EQ(b.GetHashCode(), b.getUriProperty().GetHashCode())
        << "the builder now delegates, so this holds by construction";

    // And it is NO LONGER the hash of the rendered string -- the assertion the predecessor made.
    // Written as EXPECT_NE deliberately: unequal values may hash equally
    // (docs/HashAssertionContractRule.md R2), but here the two numbers come from different
    // inputs -- "http://EXAMPLE.COM:80/" against the folded, port-resolved identity key -- so a
    // collision would be a coincidence this test is entitled to notice.
    EXPECT_NE(b.GetHashCode(), static_cast<SharpRuntime::intcs>(
                                    std::hash<std::string>{}(b.ToString())));
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
    // UPDATED BY #2391. The property is unchanged -- equal builders hash equally -- but its
    // DOMAIN shrank: the two unparseable shapes now throw from both members, so they are no
    // longer members of any equality class and are exercised by
    // Fix2391_AnUnparseableBuilderNowThrowsFromBothMembers instead.
    UriBuilder plain;      plain.setHostProperty("example.com");
    UriBuilder withPort;   withPort.setHostProperty("example.com");  withPort.setPortProperty(8080);
    UriBuilder withCreds;  withCreds.setHostProperty("example.com"); withCreds.setUserNameProperty("u");
                           withCreds.setPasswordProperty("p");
    UriBuilder withQuery;  withQuery.setHostProperty("example.com"); withQuery.setQueryProperty("a=1");
    UriBuilder withFrag;   withFrag.setHostProperty("example.com");  withFrag.setFragmentProperty("f");
    UriBuilder emptyQuery; emptyQuery.setHostProperty("example.com");emptyQuery.setQueryProperty("");
    UriBuilder badPort;    badPort.setHostProperty("h:abc");  // parses since #1996 G-1
    UriBuilder fileShape("file", "");  fileShape.setPathProperty("/tmp/x");

    const UriBuilder* shapes[] = {&plain, &withPort, &withCreds, &withQuery, &withFrag,
                                  &emptyQuery, &badPort, &fileShape};
    for (const UriBuilder* a : shapes) {
        for (const UriBuilder* b : shapes)
            expectEqualsImpliesEqualHash(*a, *b, "cross product");
        UriBuilder copy = *a;
        EXPECT_TRUE(a->Equals(copy));
        EXPECT_EQ(a->GetHashCode(), copy.GetHashCode());
    }

    // The shape that used to be in this list and no longer can be. Kept HERE, in the test whose
    // domain it left, so the exclusion is visible at the place it was made rather than only in
    // the ticket.
    UriBuilder emptyHost;  emptyHost.setHostProperty("");
    EXPECT_THROW((void)emptyHost.GetHashCode(), UriFormatException);
}

TEST(UriBuilderTest, Fix2391_TheComparandIsAStringSoAnUnparseableOTHERIsMerelyUnequal) {
    // .NET's asymmetry, transcribed rather than tidied. UriBuilder.Equals(object) is
    // `rparam is not null && Uri.Equals(rparam.ToString())` (UriBuilder.cs:277):
    //   * THIS builder goes through the `Uri` property, so an unparseable SELF throws;
    //   * the OTHER is handed over as a STRING, and Uri.Equals(string) runs
    //     TryCreate(s, UriKind.RelativeOrAbsolute, out _) and returns FALSE when that fails.
    // Making both sides throw would be tidier and would not be .NET. Making neither throw would
    // be #2004, which the user withdrew.
    UriBuilder good;   good.setHostProperty("example.com");
    UriBuilder broken; broken.setHostProperty("[::1");

    EXPECT_FALSE(good.Equals(broken)) << "an unparseable OTHER is unequal, not an error";
    EXPECT_THROW((void)broken.Equals(good), UriFormatException)
        << "an unparseable SELF throws, because the Uri property does";
}

TEST(UriBuilderTest, Decl2391_TheComparandKindIsRelativeOrAbsoluteAndIsNowAnEquivalence) {
    // .NET's comparand branch uses UriKind.RelativeOrAbsolute, NOT Absolute (Uri.cs, the
    // `comparand is string` branch), and this port transcribes that. HONEST RECORD, and it has
    // moved twice in one day:
    //
    //   * The first cut of this case recorded the kind as an unobservable EQUIVALENCE, reasoning
    //     that getUriProperty() is always ABSOLUTE, so a relative comparand can never compare
    //     equal and both spellings return false.
    //   * That reasoning was then measured WRONG, because this port's Uri(std::string)
    //     constructor ACCEPTED "://example.com/" while its own TryCreate(s, Absolute) REJECTED
    //     it -- two grammars in one type. `self` could therefore be a Uri the strict comparand
    //     parse would refuse, and the spellings disagreed. The case was rewritten to pin that.
    //   * #2393 REMOVED THE SECOND GRAMMAR. Uri(std::string) now delegates to
    //     Uri(s, UriKind::Absolute), which is what .NET's does (Uri.cs:424-429), so the
    //     constructor and TryCreate accept exactly the same strings -- and the original
    //     reasoning becomes true. The kind is an equivalence again, this time for a reason that
    //     holds by construction rather than by luck.
    //
    // So: mutating RelativeOrAbsolute to Absolute is a mutation NO TEST CAN CATCH, and the line
    // is kept as RelativeOrAbsolute because that is what .NET writes, not because it is
    // load-bearing. Simplifying it would be simplifying the reference rather than porting it.
    UriBuilder emptyScheme;
    emptyScheme.setSchemeProperty("");   // accepted since #1996 G-3
    emptyScheme.setHostProperty("example.com");
    ASSERT_EQ(emptyScheme.ToString(), "://example.com/");

    // The premise, measured: the two kinds still disagree on this string, so the branch is
    // reachable and the equivalence is not vacuous.
    std::shared_ptr<System::Uri> parsed;
    EXPECT_TRUE(System::Uri::TryCreate(emptyScheme.ToString(),
                                       System::UriKind::RelativeOrAbsolute, parsed));
    EXPECT_FALSE(System::Uri::TryCreate(emptyScheme.ToString(),
                                        System::UriKind::Absolute, parsed));

    // And here is why it no longer discriminates: `self` can no longer BE such a Uri. The builder
    // throws before any comparand is parsed, so neither spelling is ever reached.
    UriBuilder good; good.setHostProperty("example.com");
    EXPECT_THROW((void)emptyScheme.getUriProperty(), UriFormatException);
    EXPECT_THROW((void)emptyScheme.Equals(emptyScheme), UriFormatException);
    EXPECT_THROW((void)emptyScheme.Equals(good), UriFormatException);

    // An unparseable OTHER is still merely unequal, which is the asymmetry #2391 landed and
    // #2393 does not touch.
    EXPECT_FALSE(good.Equals(emptyScheme));
}

TEST(UriBuilderTest, Fix2391_TheseFormerlyUnequalPairsAreNowEqual) {
    // INVERTED BY #2391, exactly as its predecessor said it must be: "these are the pairs #1995
    // would make EQUAL. They must stay unequal until that approval lands, and this test must be
    // updated with it."
    //
    // #1995 gave Uri canonical identity; it did NOT reach UriBuilder, because the builder still
    // compared rendered text. #2391 made the builder delegate, so the canonicalisation finally
    // arrives here -- which is why this inversion belongs to #2391 and not to #1995.
    UriBuilder lower;  lower.setHostProperty("example.com");
    UriBuilder upperHost; upperHost.setHostProperty("EXAMPLE.COM");
    UriBuilder defaultPort; defaultPort.setHostProperty("example.com"); defaultPort.setPortProperty(80);

    EXPECT_TRUE(lower.Equals(upperHost)) << "host case is folded by Uri's identity key";
    EXPECT_TRUE(lower.Equals(defaultPort)) << "an explicit default port resolves to the same key";
    EXPECT_EQ(lower.GetHashCode(), upperHost.GetHashCode());
    EXPECT_EQ(lower.GetHashCode(), defaultPort.GetHashCode());

    // Not everything collapsed: identity is canonical, not blind. A different host is still a
    // different builder, and a NON-default port is still significant.
    UriBuilder otherHost; otherHost.setHostProperty("example.org");
    UriBuilder port8080;  port8080.setHostProperty("example.com"); port8080.setPortProperty(8080);
    EXPECT_FALSE(lower.Equals(otherHost));
    EXPECT_FALSE(lower.Equals(port8080));
}

TEST(UriBuilderTest, Fix1995_UriIdentityIsNowCanonical) {
    // INVERTED by #1995. This was written by #2004 to say "this ticket does not change Uri
    // identity", so a later reader would not mistake #2004 for an identity change. #1995 IS that
    // change, so the pin's subject moves rather than the pin being deleted.
    System::Uri a("http://example.com/p");
    System::Uri b("http://example.com/p");
    System::Uri caseDiff("HTTP://EXAMPLE.COM/p");
    System::Uri explicitDefaultPort("http://example.com:80/p");
    EXPECT_TRUE(a == b);
    EXPECT_EQ(a.GetHashCode(), b.GetHashCode());
    EXPECT_TRUE(a == caseDiff)            << "#1995: scheme and host are folded for comparison";
    EXPECT_TRUE(a == explicitDefaultPort) << "#1995: an explicit default port is the default";
    EXPECT_EQ(a.GetHashCode(), caseDiff.GetHashCode());
    EXPECT_EQ(a.GetHashCode(), explicitDefaultPort.GetHashCode());
}

TEST(UriBuilderTest, Layout_SizeOfUriBuilderIsPinned) {
    // docs/SystemUriNamespaceReviewPlan.md §9.2 claimed sizeof(System::UriBuilder) == 232 was
    // "re-asserted by permanent tests"; measured on 2026-08-03 no such test existed. Added
    // here so the claim becomes true, alongside UriTests.Layout_SizeOfUriIsPinned.
    EXPECT_EQ(sizeof(System::UriBuilder), 232u);
}

// ===========================================================================
// #1996 groups G-1 (IPv6 bracketing) and G-2 (scheme lower-casing) -- the
// ticket's own "recommended minimum". Both are alignments to the reference, so
// SA-5 covers them; G-3 (scheme validation) and G-4 (relative promotion) are
// not taken and #1996 keeps them.
// ===========================================================================

TEST(UriBuilderTest, Fix1996G1_AnIPv6HostIsBracketed) {
    // Measured before: setHostProperty("::1") rendered "http://::1/", which Uri rejects.
    UriBuilder b;
    b.setHostProperty("::1");
    EXPECT_EQ(b.getHostProperty(), "[::1]");
    EXPECT_EQ(b.ToString(), "http://[::1]/");
    EXPECT_NO_THROW((void)b.getUriProperty()) << "the rendering is now one Uri accepts";

    // Already bracketed: left alone, not double-wrapped. UriBuilder.cs:178 tests
    // StartsWith('[') && EndsWith(']') for exactly this.
    UriBuilder already;
    already.setHostProperty("[::1]");
    EXPECT_EQ(already.getHostProperty(), "[::1]");
    EXPECT_EQ(already.ToString(), "http://[::1]/");

    // A full-length literal and a scoped one both go through.
    UriBuilder full;
    full.setHostProperty("2001:db8::1");
    EXPECT_EQ(full.ToString(), "http://[2001:db8::1]/");
}

TEST(UriBuilderTest, Fix1996G1_AHostWithoutAColonIsUntouched) {
    // The trigger is .NET's own: the value must contain ':'. A DNS name, an IPv4 literal and an
    // empty host touch nothing -- the rows that fail if the bracketing is applied unconditionally.
    for (const char* host : {"example.com", "192.0.2.1", "", "localhost"}) {
        UriBuilder b;
        b.setHostProperty(host);
        EXPECT_EQ(b.getHostProperty(), host) << host;
    }
}

TEST(UriBuilderTest, Fix1996G2_TheSchemeIsLowerCasedInTheSetter) {
    // Measured before: setSchemeProperty("HTTP") kept "HTTP" and rendered "HTTP://…".
    UriBuilder b;
    b.setSchemeProperty("HTTP");
    EXPECT_EQ(b.getSchemeProperty(), "http");
    b.setHostProperty("example.com");
    EXPECT_EQ(b.ToString(), "http://example.com/");

    UriBuilder mixed;
    mixed.setSchemeProperty("HtTpS");
    EXPECT_EQ(mixed.getSchemeProperty(), "https");

    // THIS ROW'S SUBJECT MOVED WITH G-3, and the new guarantee is stronger. It used to assert
    // that a NON-ASCII scheme is folded invariantly -- an ASCII-only fold, so a process with a
    // Turkish locale could not change what a scheme means. G-3 makes such a scheme unstorable at
    // all: CheckSchemeName accepts only ASCII letters, digits and "+-.", and "HÜTTP" has no ':'
    // to truncate at, so it is refused before folding is ever reached. The locale hazard is now
    // closed by construction rather than by the fold's implementation.
    UriBuilder nonAscii;
    EXPECT_THROW(nonAscii.setSchemeProperty("H\xC3\x9CTTP"), System::ArgumentException);
    EXPECT_EQ(nonAscii.getSchemeProperty(), "http") << "and the rejected value was not stored";
}

TEST(UriBuilderTest, Fix1996G3_AnInvalidSchemeIsRejected) {
    // INVERTED: this pin used to assert that G-3 was NOT taken. .NET validates the scheme,
    // truncates at a ':' and re-checks, and throws ArgumentException(net_uri_BadScheme,
    // nameof(value)) if it still fails (UriBuilder.cs:108-134).
    UriBuilder bad;
    EXPECT_THROW(bad.setSchemeProperty("bad scheme"), System::ArgumentException);
    EXPECT_EQ(bad.getSchemeProperty(), "http") << "the rejected value was not stored";

    try {
        UriBuilder b;
        b.setSchemeProperty("bad scheme");
        FAIL() << "expected a throw";
    } catch (const System::ArgumentException& e) {
        EXPECT_EQ(std::string(e.getMessageProperty()).find(
                      "Invalid URI: The URI scheme is not valid."), 0u);
        EXPECT_EQ(e.getParamNameProperty(), "value") << ".NET passes nameof(value)";
    }
}

TEST(UriBuilderTest, Fix1996G3_TheTruncateAtColonRetryIsTranscribed) {
    // THE HALF THE TICKET'S SUMMARY OMITTED. §14.2 says only "throw for an invalid one", which
    // would reject "http:" -- .NET ACCEPTS it, because a scheme failing the first check is
    // retried after cutting at the first ':'.
    UriBuilder withColon;
    EXPECT_NO_THROW(withColon.setSchemeProperty("http:"));
    EXPECT_EQ(withColon.getSchemeProperty(), "http");

    UriBuilder withDelimiter;
    EXPECT_NO_THROW(withDelimiter.setSchemeProperty("HTTPS://"));
    EXPECT_EQ(withDelimiter.getSchemeProperty(), "https") << "truncated, then folded";

    // ...but truncation only rescues text whose PREFIX is a scheme.
    UriBuilder stillBad;
    EXPECT_THROW(stillBad.setSchemeProperty("bad scheme:x"), System::ArgumentException);
}

TEST(UriBuilderTest, Decl1996G3_AnEmptySchemeIsAccepted) {
    // The whole validation block is guarded on `value.Length != 0`, so empty is stored as empty
    // rather than rejected -- easy to lose when transcribing a guard that throws.
    UriBuilder empty;
    EXPECT_NO_THROW(empty.setSchemeProperty(""));
    EXPECT_EQ(empty.getSchemeProperty(), "");
}

TEST(UriBuilderTest, Fix1996G4_ARelativeConstructorStringIsPromoted) {
    // INVERTED: this used to assert the measured ":///www.example.com/path". .NET prefixes
    // "http://" and REPARSES (UriBuilder.cs:29-40), and its own comment says why: "setting
    // allowRelative=true for a string like www.acme.org".
    UriBuilder relative("www.example.com/path");
    EXPECT_EQ(relative.getSchemeProperty(), "http");
    EXPECT_EQ(relative.getHostProperty(), "www.example.com")
        << "the host comes from the REPARSE -- §14.2's 'localhost host' is wrong";
    EXPECT_EQ(relative.getPathProperty(), "/path");

    // THE STRONGEST FORM OF THE ASSERTION: the promoted string is indistinguishable from writing
    // the scheme out. Anything else would mean the promotion took a different route.
    UriBuilder explicitScheme("http://www.example.com/path");
    EXPECT_EQ(relative.ToString(), explicitScheme.ToString());

    // And the rendered port is ":80", which is CORRECT rather than a wart -- my first expectation
    // here was "http://www.example.com/path" and it was wrong, not the code. .NET's
    // SetFieldsFromUri does `_port = _uri.Port` (UriBuilder.cs:307), Uri("http://…").Port is 80,
    // and ToString appends the port whenever `_port != -1` (:381), so .NET renders the default
    // port too. Pre-existing behaviour on BOTH routes, and out of G-4's scope either way.
    EXPECT_EQ(relative.ToString(), "http://www.example.com:80/path");
}

TEST(UriBuilderTest, Decl1996G4_AnAbsoluteConstructorStringIsUntouched) {
    // The promotion runs only when the parse is NOT absolute, so nothing that worked moves.
    UriBuilder absolute("https://example.com:8443/p?q=1#f");
    EXPECT_EQ(absolute.getSchemeProperty(), "https");
    EXPECT_EQ(absolute.getHostProperty(), "example.com");
    EXPECT_EQ(absolute.getPortProperty(), 8443);
}

TEST(UriBuilderTest, Decl1996_TheHostRejectionIsStillNotTaken) {
    // What G-1's block still does NOT take, kept so the boundary stays visible: .NET throws
    // ArgumentException(net_uri_BadHostName) for "contoso.com/path"; this port stores it.
    UriBuilder path;
    EXPECT_NO_THROW(path.setHostProperty("contoso.com/path"));
    EXPECT_EQ(path.getHostProperty(), "contoso.com/path");
}
