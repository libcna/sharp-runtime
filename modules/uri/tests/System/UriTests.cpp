// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <memory>
#include "System/Uri.hpp"
#include "System/UriFormatException.hpp"

using System::Uri;
using System::UriKind;

// ---------------------------------------------------------------------------
// Basic absolute HTTP URI
// ---------------------------------------------------------------------------

TEST(UriTests, Http_IsAbsolute) {
    Uri u("http://example.com/path");
    EXPECT_TRUE(u.getIsAbsoluteUriProperty());
}

TEST(UriTests, Http_Scheme) {
    Uri u("http://example.com/path");
    EXPECT_EQ(u.getSchemeProperty(), "http");
}

TEST(UriTests, Http_Host) {
    Uri u("http://example.com/path");
    EXPECT_EQ(u.getHostProperty(), "example.com");
}

TEST(UriTests, Http_DefaultPort_Is80) {
    Uri u("http://example.com/path");
    EXPECT_EQ(u.getPortProperty(), 80);
}

TEST(UriTests, Http_AbsolutePath) {
    Uri u("http://example.com/path/to/resource");
    EXPECT_EQ(u.getAbsolutePathProperty(), "/path/to/resource");
}

TEST(UriTests, Http_AbsoluteUri_EqualsInput) {
    std::string raw = "http://example.com/path";
    Uri u(raw);
    EXPECT_EQ(u.getAbsoluteUriProperty(), raw);
}

TEST(UriTests, Http_ToString_EqualsAbsoluteUri) {
    Uri u("http://example.com/");
    EXPECT_EQ(u.ToString(), u.getAbsoluteUriProperty());
}

// ---------------------------------------------------------------------------
// HTTPS
// ---------------------------------------------------------------------------

TEST(UriTests, Https_Scheme) {
    Uri u("https://secure.example.com/login");
    EXPECT_EQ(u.getSchemeProperty(), "https");
}

TEST(UriTests, Https_DefaultPort_Is443) {
    Uri u("https://secure.example.com/");
    EXPECT_EQ(u.getPortProperty(), 443);
}

// ---------------------------------------------------------------------------
// Explicit port
// ---------------------------------------------------------------------------

TEST(UriTests, ExplicitPort_Parsed) {
    Uri u("http://example.com:8080/api");
    EXPECT_EQ(u.getPortProperty(), 8080);
}

TEST(UriTests, ExplicitPort_Host_ExcludesPort) {
    Uri u("http://example.com:8080/api");
    EXPECT_EQ(u.getHostProperty(), "example.com");
}

// ---------------------------------------------------------------------------
// Query and fragment
// ---------------------------------------------------------------------------

TEST(UriTests, Query_IncludesQuestionMark) {
    Uri u("http://example.com/search?q=hello&lang=en");
    EXPECT_EQ(u.getQueryProperty(), "?q=hello&lang=en");
}

TEST(UriTests, Query_EmptyWhenAbsent) {
    Uri u("http://example.com/page");
    EXPECT_TRUE(u.getQueryProperty().empty());
}

TEST(UriTests, Fragment_IncludesHash) {
    Uri u("http://example.com/page#section1");
    EXPECT_EQ(u.getFragmentProperty(), "#section1");
}

TEST(UriTests, Fragment_EmptyWhenAbsent) {
    Uri u("http://example.com/page");
    EXPECT_TRUE(u.getFragmentProperty().empty());
}

TEST(UriTests, PathAndQuery_CombinesPathAndQuery) {
    Uri u("http://example.com/search?q=test");
    EXPECT_EQ(u.getPathAndQueryProperty(), "/search?q=test");
}

TEST(UriTests, PathAndQuery_NoQueryIsJustPath) {
    Uri u("http://example.com/about");
    EXPECT_EQ(u.getPathAndQueryProperty(), "/about");
}

// ---------------------------------------------------------------------------
// User info
// ---------------------------------------------------------------------------

TEST(UriTests, UserInfo_Parsed) {
    Uri u("ftp://user:pass@ftp.example.com/file.txt");
    EXPECT_EQ(u.getUserInfoProperty(), "user:pass");
}

TEST(UriTests, UserInfo_EmptyWhenAbsent) {
    Uri u("http://example.com/");
    EXPECT_TRUE(u.getUserInfoProperty().empty());
}

// ---------------------------------------------------------------------------
// Authority
// ---------------------------------------------------------------------------

TEST(UriTests, Authority_DefaultPort_ExcludesPort) {
    Uri u("http://example.com/");
    EXPECT_EQ(u.getAuthorityProperty(), "example.com");
}

TEST(UriTests, Authority_NonDefaultPort_IncludesPort) {
    Uri u("http://example.com:9000/");
    EXPECT_EQ(u.getAuthorityProperty(), "example.com:9000");
}

// ---------------------------------------------------------------------------
// Loopback detection
// ---------------------------------------------------------------------------

TEST(UriTests, Loopback_Localhost_IsLoopback) {
    Uri u("http://localhost/");
    EXPECT_TRUE(u.getIsLoopbackProperty());
}

TEST(UriTests, Loopback_127_IsLoopback) {
    Uri u("http://127.0.0.1/");
    EXPECT_TRUE(u.getIsLoopbackProperty());
}

TEST(UriTests, Loopback_ExternalHost_IsNotLoopback) {
    Uri u("http://example.com/");
    EXPECT_FALSE(u.getIsLoopbackProperty());
}

// Regression for ticket 340: the previous `host_ == "::1"` comparison could never match, since
// a parsed IPv6 literal's host_ retains its surrounding brackets ("[::1]", matching .NET's own
// bracketed Host property for IPv6). Also verifies "localhost" is matched case-insensitively,
// per Uri.cs's DomainNameHelper (StringComparison.OrdinalIgnoreCase).
TEST(UriTests, Loopback_IPv6_IsLoopback) {
    Uri u("http://[::1]:8080/");
    EXPECT_TRUE(u.getIsLoopbackProperty());
}

TEST(UriTests, Loopback_LocalhostUppercase_IsLoopback) {
    Uri u("http://LOCALHOST/");
    EXPECT_TRUE(u.getIsLoopbackProperty());
}

// ---------------------------------------------------------------------------
// Relative URI
// ---------------------------------------------------------------------------

TEST(UriTests, Relative_IsNotAbsolute) {
    Uri u("/relative/path");
    EXPECT_FALSE(u.getIsAbsoluteUriProperty());
}

TEST(UriTests, Relative_UriKind_Accepted) {
    EXPECT_NO_THROW(Uri("/path/to/resource", UriKind::Relative));
}

TEST(UriTests, Absolute_UriKind_RejectsRelative) {
    EXPECT_THROW(Uri("/path/only", UriKind::Absolute), System::UriFormatException);
}

// ---------------------------------------------------------------------------
// TryCreate
// ---------------------------------------------------------------------------

TEST(UriTests, TryCreate_ValidAbsolute_ReturnsTrue) {
    std::shared_ptr<Uri> result;
    bool ok = Uri::TryCreate("http://example.com/", UriKind::Absolute, result);
    EXPECT_TRUE(ok);
    EXPECT_NE(result, nullptr);
}

TEST(UriTests, TryCreate_RelativeAsAbsolute_ReturnsFalse) {
    std::shared_ptr<Uri> result;
    bool ok = Uri::TryCreate("/relative", UriKind::Absolute, result);
    EXPECT_FALSE(ok);
    EXPECT_EQ(result, nullptr);
}

TEST(UriTests, TryCreate_ValidRelative_ReturnsTrue) {
    std::shared_ptr<Uri> result;
    bool ok = Uri::TryCreate("/path", UriKind::Relative, result);
    EXPECT_TRUE(ok);
    EXPECT_NE(result, nullptr);
}

// ---------------------------------------------------------------------------
// Equality
// ---------------------------------------------------------------------------

TEST(UriTests, Equality_SameString_Equal) {
    Uri a("http://example.com/path");
    Uri b("http://example.com/path");
    EXPECT_TRUE(a == b);
}

TEST(UriTests, Equality_DifferentString_NotEqual) {
    Uri a("http://example.com/foo");
    Uri b("http://example.com/bar");
    EXPECT_TRUE(a != b);
}

// ---------------------------------------------------------------------------
// FTP scheme
// ---------------------------------------------------------------------------

TEST(UriTests, Ftp_DefaultPort_Is21) {
    Uri u("ftp://ftp.example.com/pub/file.txt");
    EXPECT_EQ(u.getPortProperty(), 21);
}

TEST(UriTests, Ftp_Scheme) {
    Uri u("ftp://ftp.example.com/pub/file.txt");
    EXPECT_EQ(u.getSchemeProperty(), "ftp");
}

// ---------------------------------------------------------------------------
// Opaque (non-hierarchical) absolute URIs — no "//" authority
// ---------------------------------------------------------------------------

TEST(UriTests, Mailto_IsAbsolute) {
    Uri u("mailto:user@example.com");
    EXPECT_TRUE(u.getIsAbsoluteUriProperty());
}

TEST(UriTests, Mailto_Scheme) {
    Uri u("mailto:user@example.com");
    EXPECT_EQ(u.getSchemeProperty(), "mailto");
}

TEST(UriTests, Mailto_OpaquePartInPath) {
    Uri u("mailto:user@example.com");
    EXPECT_EQ(u.getAbsolutePathProperty(), "user@example.com");
}

TEST(UriTests, Mailto_HostIsEmpty) {
    Uri u("mailto:user@example.com");
    EXPECT_TRUE(u.getHostProperty().empty());
}

TEST(UriTests, Urn_IsAbsolute) {
    Uri u("urn:isbn:0-395-36341-1");
    EXPECT_TRUE(u.getIsAbsoluteUriProperty());
    EXPECT_EQ(u.getSchemeProperty(), "urn");
    EXPECT_EQ(u.getAbsolutePathProperty(), "isbn:0-395-36341-1");
}

TEST(UriTests, Opaque_WithQueryAndFragment) {
    Uri u("tel:+1-555-0100?ext=42#note");
    EXPECT_EQ(u.getSchemeProperty(), "tel");
    EXPECT_EQ(u.getAbsolutePathProperty(), "+1-555-0100");
    EXPECT_EQ(u.getQueryProperty(), "?ext=42");
    EXPECT_EQ(u.getFragmentProperty(), "#note");
}

// ---------------------------------------------------------------------------
// Combine (base, relative) — RFC 3986 §5.3 merge + dot-segment removal
// ---------------------------------------------------------------------------

TEST(UriTests, Combine_TrailingSlashBase_AppendsRelative) {
    Uri base("http://example.com/a/b/");
    Uri combined(base, "c");
    EXPECT_EQ(combined.getAbsoluteUriProperty(), "http://example.com/a/b/c");
}

TEST(UriTests, Combine_NoTrailingSlashBase_ReplacesLastSegment) {
    Uri base("http://example.com/a/b");
    Uri combined(base, "c");
    EXPECT_EQ(combined.getAbsoluteUriProperty(), "http://example.com/a/c");
}

TEST(UriTests, Combine_DotDotSegment_ResolvesUpOneLevel) {
    Uri base("http://example.com/a/b/");
    Uri combined(base, "../c");
    EXPECT_EQ(combined.getAbsoluteUriProperty(), "http://example.com/a/c");
}

TEST(UriTests, Combine_AbsolutePathRelative_ReplacesWholePath) {
    Uri base("http://example.com/a/b/");
    Uri combined(base, "/absolute-path");
    EXPECT_EQ(combined.getAbsoluteUriProperty(), "http://example.com/absolute-path");
}

TEST(UriTests, Combine_DotSegment_IsRemoved) {
    Uri base("http://example.com/a/b/");
    Uri combined(base, "./c");
    EXPECT_EQ(combined.getAbsoluteUriProperty(), "http://example.com/a/b/c");
}

// ---------------------------------------------------------------------------
// Default ports — must match .NET's built-in scheme table exactly
// ---------------------------------------------------------------------------

TEST(UriTests, DefaultPort_UnregisteredScheme_IsMinusOne) {
    // "ssh" is not a .NET built-in scheme and has no default port.
    Uri u("ssh://host/");
    EXPECT_EQ(u.getPortProperty(), -1);
}

// ---------------------------------------------------------------------------
// Malformed port — verified against Uri.cs's port-parsing loop (ParsingError.BadPort for a
// non-digit character in the port position, or a value > 0xFFFF; both surface as
// UriFormatException). The previous code let std::stoi's exception fall through to
// `host_ = authority`, silently mangling host_ into the whole "host:badport" text instead of
// rejecting the URI, and didn't range-check a successfully-parsed but oversized port at all.
// ---------------------------------------------------------------------------

TEST(UriTests, Port_NonNumeric_Throws) {
    EXPECT_THROW(Uri("http://example.com:abc/path"), System::UriFormatException);
}

TEST(UriTests, Port_TooLarge_Throws) {
    EXPECT_THROW(Uri("http://example.com:99999/path"), System::UriFormatException);
}

TEST(UriTests, Port_IntOverflow_Throws) {
    EXPECT_THROW(Uri("http://example.com:99999999999999999999/path"), System::UriFormatException);
}

TEST(UriTests, Port_MaxValid_Parsed) {
    Uri u("http://example.com:65535/path");
    EXPECT_EQ(u.getPortProperty(), 65535);
}

// ---------------------------------------------------------------------------
// Combine with an opaque (non-"//"-form) absolute relativeUri — verified against Uri.cs's
// CreateUri/ResolveHelper: relativeUri is parsed standalone first, and if it is itself absolute
// (hierarchical OR opaque), the base is discarded entirely. The previous check only recognized
// the "://" hierarchical form via a raw substring search.
// ---------------------------------------------------------------------------

TEST(UriTests, Combine_OpaqueAbsoluteRelative_DiscardsBase) {
    Uri base("http://example.com/a/b/");
    Uri combined(base, "mailto:user@example.com");
    EXPECT_EQ(combined.getSchemeProperty(), "mailto");
    EXPECT_EQ(combined.getAbsolutePathProperty(), "user@example.com");
}

TEST(UriTests, Combine_UrnAbsoluteRelative_DiscardsBase) {
    Uri base("http://example.com/a/b/");
    Uri combined(base, "urn:isbn:0-395-36341-1");
    EXPECT_EQ(combined.getSchemeProperty(), "urn");
}

// ---------------------------------------------------------------------------
// Combine preserves the base's userInfo — RFC 3986 §5.3: "if defined, userinfo, host, port
// [are] copied from base" into the merged authority. The previous code omitted
// baseUri.userInfo_ entirely when reconstructing the merged authority string.
// ---------------------------------------------------------------------------

TEST(UriTests, Combine_PreservesBaseUserInfo) {
    Uri base("http://user:pass@example.com/a/b/");
    Uri combined(base, "c");
    EXPECT_EQ(combined.getUserInfoProperty(), "user:pass");
    EXPECT_EQ(combined.getAbsoluteUriProperty(), "http://user:pass@example.com/a/b/c");
}

// ---------------------------------------------------------------------------
// OriginalString -- was entirely missing from this port (C++ counterpart of
// .NET Uri.OriginalString), unlike getAbsoluteUriProperty() which existed but
// is documented (per real .NET semantics) to sometimes differ.
// ---------------------------------------------------------------------------

TEST(UriTests, OriginalString_MatchesConstructorInput) {
    Uri u("http://example.com/path?q=1#frag");
    EXPECT_EQ(u.getOriginalStringProperty(), "http://example.com/path?q=1#frag");
}

TEST(UriTests, OriginalString_WorksForRelativeUri) {
    // Real .NET's AbsoluteUri throws for a relative Uri; OriginalString never does.
    Uri u("relative/path", UriKind::Relative);
    EXPECT_EQ(u.getOriginalStringProperty(), "relative/path");
}

// ---------------------------------------------------------------------------
// Scheme recognition — ticket #1988 (cause U-A). parse() previously located the scheme
// with find("://"), a search for "://" ANYWHERE in the string, and only fell back to the
// grammar-correct findSchemeColon() when that search failed. The file therefore held two
// contradictory notions of where the scheme ends and consulted the wrong one first, so a
// relative reference whose query embeds an absolute URL — the commonest redirect and
// callback shape there is — threw instead of parsing. See
// docs/SystemUriNamespaceReviewPlan.md §4.4 and §9.1 (the strict-widening proof).
// ---------------------------------------------------------------------------

TEST(UriTests, SchemeDetection_RelativeWithAbsoluteUrlInQuery_IsRelative) {
    Uri u("/path?redirect=http://evil.com");
    EXPECT_FALSE(u.getIsAbsoluteUriProperty());
    EXPECT_EQ(u.getAbsolutePathProperty(), "/path?redirect=http://evil.com");
    EXPECT_EQ(u.getSchemeProperty(), "");
}

TEST(UriTests, SchemeDetection_PathlessRelativeWithAbsoluteUrlInQuery_IsRelative) {
    Uri u("search?url=https://example.com");
    EXPECT_FALSE(u.getIsAbsoluteUriProperty());
    EXPECT_EQ(u.getOriginalStringProperty(), "search?url=https://example.com");
}

TEST(UriTests, SchemeDetection_RelativeWithAbsoluteUrlInFragment_IsRelative) {
    Uri u("page#see=http://example.com");
    EXPECT_FALSE(u.getIsAbsoluteUriProperty());
    EXPECT_EQ(u.getAbsolutePathProperty(), "page#see=http://example.com");
}

TEST(UriTests, SchemeDetection_OpaqueWithAbsoluteUrlInQuery_KeepsSchemeAndQuery) {
    Uri u("mailto:a@b.com?body=see http://x");
    EXPECT_TRUE(u.getIsAbsoluteUriProperty());
    EXPECT_EQ(u.getSchemeProperty(), "mailto");
    EXPECT_EQ(u.getAbsolutePathProperty(), "a@b.com");
    EXPECT_EQ(u.getQueryProperty(), "?body=see http://x");
}

TEST(UriTests, SchemeDetection_ColonBeforeSlashSlash_TakesTheFirstColon) {
    // The scheme is the token before the FIRST colon; "bar://baz" is opaque content, not
    // an authority, because "//" does not immediately follow that colon.
    Uri u("foo:bar://baz");
    EXPECT_TRUE(u.getIsAbsoluteUriProperty());
    EXPECT_EQ(u.getSchemeProperty(), "foo");
    EXPECT_EQ(u.getAbsolutePathProperty(), "bar://baz");
    EXPECT_TRUE(u.getHostProperty().empty());
}

TEST(UriTests, SchemeDetection_LeadingColon_IsRelative) {
    Uri u("://foo");
    EXPECT_FALSE(u.getIsAbsoluteUriProperty());
    EXPECT_EQ(u.getAbsolutePathProperty(), "://foo");
}

TEST(UriTests, SchemeDetection_SchemeStartingWithDigit_IsRelative) {
    // RFC 3986 requires ALPHA first, so "1http:" is not a scheme token.
    Uri u("1http://example.com/");
    EXPECT_FALSE(u.getIsAbsoluteUriProperty());
}

TEST(UriTests, SchemeDetection_FullSchemeGrammar_IsAccepted) {
    // ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
    Uri u("a1+-.:opaque-part");
    EXPECT_TRUE(u.getIsAbsoluteUriProperty());
    EXPECT_EQ(u.getSchemeProperty(), "a1+-.");
    EXPECT_EQ(u.getAbsolutePathProperty(), "opaque-part");
}

TEST(UriTests, SchemeDetection_SingleSlashAfterColon_StaysOpaque) {
    // Only "//" introduces an authority; one slash does not.
    Uri u("http:/example.com");
    EXPECT_TRUE(u.getIsAbsoluteUriProperty());
    EXPECT_EQ(u.getSchemeProperty(), "http");
    EXPECT_EQ(u.getAbsolutePathProperty(), "/example.com");
    EXPECT_TRUE(u.getHostProperty().empty());
}

TEST(UriTests, SchemeDetection_OrdinaryHierarchical_Unchanged) {
    // Control: the shape the old substring search got right must be byte-identical.
    Uri u("https://user:pw@example.com:8443/a/b?q=1#f");
    EXPECT_TRUE(u.getIsAbsoluteUriProperty());
    EXPECT_EQ(u.getSchemeProperty(), "https");
    EXPECT_EQ(u.getUserInfoProperty(), "user:pw");
    EXPECT_EQ(u.getHostProperty(), "example.com");
    EXPECT_EQ(u.getPortProperty(), 8443);
    EXPECT_EQ(u.getAbsolutePathProperty(), "/a/b");
    EXPECT_EQ(u.getQueryProperty(), "?q=1");
    EXPECT_EQ(u.getFragmentProperty(), "#f");
}

TEST(UriTests, SchemeDetection_TryCreateRelativeWithEmbeddedAbsoluteUrl_Succeeds) {
    std::shared_ptr<Uri> result;
    EXPECT_TRUE(Uri::TryCreate("/cb?next=http://a.example/b", UriKind::Relative, result));
    ASSERT_NE(result, nullptr);
    EXPECT_FALSE(result->getIsAbsoluteUriProperty());
}

// ---------------------------------------------------------------------------
// Default ports — ticket #1989 (cause U-B, SR-AUD-143 plus one unnamed site).
// defaultPortForScheme() declares mailto=25 and telnet=23, but the opaque branch assigned
// -1 unconditionally and the empty-port branch never consulted the table at all, so the
// file contradicted itself in two places. See docs/SystemUriNamespaceReviewPlan.md §4.2
// and §4.3.
// ---------------------------------------------------------------------------

TEST(UriTests, DefaultPort_OpaqueMailto_Is25) {
    Uri u("mailto:user@example.com");
    EXPECT_EQ(u.getPortProperty(), 25);
}

TEST(UriTests, DefaultPort_OpaqueTelnet_Is23) {
    // SR-AUD-143 names only mailto; every opaque scheme with a table entry was affected.
    Uri u("telnet:host.example.com");
    EXPECT_EQ(u.getPortProperty(), 23);
}

TEST(UriTests, DefaultPort_OpaqueSchemeWithNoTableEntry_IsMinusOne) {
    Uri u("urn:isbn:0-395-36341-1");
    EXPECT_EQ(u.getPortProperty(), -1);
}

TEST(UriTests, DefaultPort_OpaqueMailto_AuthorityStillEmpty) {
    // The port becoming the default must not make Authority start rendering ":25".
    Uri u("mailto:user@example.com");
    EXPECT_EQ(u.getAuthorityProperty(), "");
}

TEST(UriTests, DefaultPort_EmptyPortComponent_UsesSchemeDefault) {
    Uri u("http://example.com:/");
    EXPECT_EQ(u.getPortProperty(), 80);
    EXPECT_EQ(u.getHostProperty(), "example.com");
}

TEST(UriTests, DefaultPort_EmptyPortComponentHttps_UsesSchemeDefault) {
    Uri u("https://example.com:/a");
    EXPECT_EQ(u.getPortProperty(), 443);
    EXPECT_EQ(u.getAbsolutePathProperty(), "/a");
}

TEST(UriTests, DefaultPort_EmptyPortComponent_AuthorityOmitsPort) {
    Uri u("http://example.com:/");
    EXPECT_EQ(u.getAuthorityProperty(), "example.com");
}

TEST(UriTests, DefaultPort_EmptyPortComponentUnregisteredScheme_IsMinusOne) {
    Uri u("ssh://host:/");
    EXPECT_EQ(u.getPortProperty(), -1);
}

TEST(UriTests, DefaultPort_ExplicitPort_Unchanged) {
    // Control: the branch that already worked must not move.
    Uri u("http://example.com:8080/");
    EXPECT_EQ(u.getPortProperty(), 8080);
    EXPECT_EQ(u.getAuthorityProperty(), "example.com:8080");
}

TEST(UriTests, DefaultPort_CombineWithDefaultPortBase_OmitsPort) {
    // Combine renders ":port" only when the port differs from the scheme default; the
    // opaque/empty-port repair must not start injecting one.
    Uri base("http://example.com:/a/b/");
    Uri combined(base, "c");
    EXPECT_EQ(combined.getAbsoluteUriProperty(), "http://example.com/a/b/c");
}
