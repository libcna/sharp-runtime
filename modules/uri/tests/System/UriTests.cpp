// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include "System/ArgumentException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Uri.hpp"
#include "System/UriParser.hpp"
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
    EXPECT_EQ(u.getSchemeProperty(), "");
    // The whole reference is preserved by OriginalString/AbsoluteUri. AbsolutePath is the
    // path component only: ticket #2002 made the relative branch split its query and
    // fragment like the other two branches always have. Before #2002 this assertion read
    // getAbsolutePathProperty() == "/path?redirect=http://evil.com".
    EXPECT_EQ(u.getOriginalStringProperty(), "/path?redirect=http://evil.com");
    EXPECT_EQ(u.getAbsolutePathProperty(), "/path");
    EXPECT_EQ(u.getQueryProperty(), "?redirect=http://evil.com");
}

TEST(UriTests, SchemeDetection_PathlessRelativeWithAbsoluteUrlInQuery_IsRelative) {
    Uri u("search?url=https://example.com");
    EXPECT_FALSE(u.getIsAbsoluteUriProperty());
    EXPECT_EQ(u.getOriginalStringProperty(), "search?url=https://example.com");
}

TEST(UriTests, SchemeDetection_RelativeWithAbsoluteUrlInFragment_IsRelative) {
    Uri u("page#see=http://example.com");
    EXPECT_FALSE(u.getIsAbsoluteUriProperty());
    // Same #2002 update as the query case above; before #2002 this assertion read
    // getAbsolutePathProperty() == "page#see=http://example.com".
    EXPECT_EQ(u.getOriginalStringProperty(), "page#see=http://example.com");
    EXPECT_EQ(u.getAbsolutePathProperty(), "page");
    EXPECT_EQ(u.getFragmentProperty(), "#see=http://example.com");
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

// ---------------------------------------------------------------------------
// IP-literal authorities — ticket #1991 (cause U-D, SR-AUD-145a). The authority was split
// on rfind(':') guarded only by rfind(']'), so a malformed bracketed host was silently
// reinterpreted. "http://[::1/path" produced host "[:" AND port 1 — the finding names only
// the host, and the fabricated port is the more dangerous half. Bracket STRUCTURE only is
// validated; literal content and zone identifiers stay out of scope
// (docs/SystemUriNamespaceReviewPlan.md §4.1 and §15.4).
// ---------------------------------------------------------------------------

TEST(UriTests, IPv6_Unterminated_Throws) {
    EXPECT_THROW(Uri("http://[::1/path"), System::UriFormatException);
}

TEST(UriTests, IPv6_UnterminatedWithNoPath_Throws) {
    EXPECT_THROW(Uri("http://[::1"), System::UriFormatException);
}

TEST(UriTests, IPv6_TrailingJunkAfterBracket_Throws) {
    EXPECT_THROW(Uri("http://[::1]junk/path"), System::UriFormatException);
}

TEST(UriTests, IPv6_EmptyLiteral_Throws) {
    EXPECT_THROW(Uri("http://[]/path"), System::UriFormatException);
}

TEST(UriTests, IPv6_WithUserInfoAndUnterminatedLiteral_Throws) {
    // The check must run after the user-info split, or credentials hide the defect.
    EXPECT_THROW(Uri("http://user@[::1/path"), System::UriFormatException);
}

TEST(UriTests, IPv6_MalformedPortAfterBracket_StillThrows) {
    EXPECT_THROW(Uri("http://[::1]:abc/path"), System::UriFormatException);
}

TEST(UriTests, IPv6_WellFormed_NoPort_Unchanged) {
    Uri u("http://[::1]/path");
    EXPECT_EQ(u.getHostProperty(), "[::1]");
    EXPECT_EQ(u.getPortProperty(), 80);
    EXPECT_EQ(u.getAbsolutePathProperty(), "/path");
}

TEST(UriTests, IPv6_WellFormed_WithPort_Unchanged) {
    Uri u("http://[::1]:8080/path");
    EXPECT_EQ(u.getHostProperty(), "[::1]");
    EXPECT_EQ(u.getPortProperty(), 8080);
}

TEST(UriTests, IPv6_WellFormed_FullAddress_Unchanged) {
    Uri u("https://[2001:db8::8a2e:370:7334]:443/a?q=1");
    EXPECT_EQ(u.getHostProperty(), "[2001:db8::8a2e:370:7334]");
    EXPECT_EQ(u.getPortProperty(), 443);
    EXPECT_EQ(u.getQueryProperty(), "?q=1");
}

TEST(UriTests, IPv6_WellFormed_EmptyPortComponent_UsesDefault) {
    // #1989's empty-port repair and #1991's bracket check must compose.
    Uri u("http://[::1]:/path");
    EXPECT_EQ(u.getHostProperty(), "[::1]");
    EXPECT_EQ(u.getPortProperty(), 80);
}

TEST(UriTests, IPv6_Loopback_StillDetected) {
    Uri u("http://[::1]:8080/");
    EXPECT_TRUE(u.getIsLoopbackProperty());
}

TEST(UriTests, IPv6_LiteralContentIsNotValidated_DocumentedExclusion) {
    // Explicit exclusion: bracket STRUCTURE is checked, content is not. This pins the
    // boundary so a later "improvement" that starts validating content is a deliberate
    // decision rather than an accident.
    Uri u("http://[not-an-address]/p");
    EXPECT_EQ(u.getHostProperty(), "[not-an-address]");
}

// ---------------------------------------------------------------------------
// UriKind domain — ticket #1992 (cause U-E, SR-AUD-145b). The two guards matched only
// Absolute and Relative, so any other value fell through both and silently meant
// RelativeOrAbsolute. Same cause and same policy as System::Runtime's GCSettings setters
// (#1976) and System::Threading's boundary checks (#1954).
// ---------------------------------------------------------------------------

TEST(UriTests, UriKind_OutOfDomainPositive_ThrowsArgumentException) {
    EXPECT_THROW(Uri("http://example.com/", static_cast<UriKind>(99)),
                 System::ArgumentException);
}

TEST(UriTests, UriKind_OutOfDomainNegative_ThrowsArgumentException) {
    EXPECT_THROW(Uri("http://example.com/", static_cast<UriKind>(-1)),
                 System::ArgumentException);
}

TEST(UriTests, UriKind_OutOfDomainAdjacent_ThrowsArgumentException) {
    // 3 is one past Relative, the value a careless cast from a count produces.
    EXPECT_THROW(Uri("http://example.com/", static_cast<UriKind>(3)),
                 System::ArgumentException);
}

TEST(UriTests, UriKind_OutOfDomain_ParamNameIsUriKind) {
    try {
        Uri u("http://example.com/", static_cast<UriKind>(99));
        FAIL() << "expected ArgumentException";
    } catch (const System::ArgumentException& e) {
        EXPECT_EQ(e.getParamNameProperty(), "uriKind");
        EXPECT_NE(std::string(e.what()).find("UriKind"), std::string::npos);
    }
}

TEST(UriTests, UriKind_OutOfDomain_RejectedBeforeParsing) {
    // The domain check must precede parse(), so a malformed string still reports the
    // argument error rather than a UriFormatException about the text.
    EXPECT_THROW(Uri("http://h:abc/", static_cast<UriKind>(99)), System::ArgumentException);
}

TEST(UriTests, UriKind_EveryDeclaredMember_StillAccepted) {
    for (auto kind : {UriKind::RelativeOrAbsolute, UriKind::Absolute}) {
        EXPECT_NO_THROW(Uri("http://example.com/", kind));
    }
    for (auto kind : {UriKind::RelativeOrAbsolute, UriKind::Relative}) {
        EXPECT_NO_THROW(Uri("/relative", kind));
    }
}

TEST(UriTests, UriKind_OutOfDomain_TryCreateReturnsFalse) {
    // Deferred verification (docs/SystemUriNamespaceReviewPlan.md §17): TryCreate catches
    // every exception, so the constructor's ArgumentException becomes false + nullptr.
    // Whether .NET propagates instead could not be measured here, so the current answer is
    // PINNED rather than changed on recollection.
    std::shared_ptr<Uri> result;
    EXPECT_FALSE(Uri::TryCreate("http://example.com/", static_cast<UriKind>(99), result));
    EXPECT_EQ(result, nullptr);
}

// ---------------------------------------------------------------------------
// Reference resolution — ticket #1990 (cause U-C, SR-AUD-144). The two-Uri constructor
// implemented only the path case of RFC 3986 §5.2.2/§5.3: it split a "?"/"#" tail off the
// reference, merged the remainder as a path under the base authority, and re-appended the
// tail. Three of the four cases were therefore wrong — a query-only reference truncated
// the base path, a fragment-only reference truncated it AND dropped the base query, and a
// network-path reference was merged under the WRONG authority.
// ---------------------------------------------------------------------------

TEST(UriTests, Resolve_QueryOnlyReference_KeepsFullBasePath) {
    Uri base("http://example.com/a/b?old#old");
    Uri combined(base, "?new");
    EXPECT_EQ(combined.getAbsolutePathProperty(), "/a/b");
    EXPECT_EQ(combined.getQueryProperty(), "?new");
    EXPECT_TRUE(combined.getFragmentProperty().empty());
    EXPECT_EQ(combined.getAbsoluteUriProperty(), "http://example.com/a/b?new");
}

TEST(UriTests, Resolve_FragmentOnlyReference_KeepsBasePathAndBaseQuery) {
    Uri base("http://example.com/a/b?old#old");
    Uri combined(base, "#new");
    EXPECT_EQ(combined.getAbsolutePathProperty(), "/a/b");
    EXPECT_EQ(combined.getQueryProperty(), "?old");
    EXPECT_EQ(combined.getFragmentProperty(), "#new");
    EXPECT_EQ(combined.getAbsoluteUriProperty(), "http://example.com/a/b?old#new");
}

TEST(UriTests, Resolve_QueryOnlyReferenceWithFragment_ReplacesBothQueryAndFragment) {
    Uri base("http://example.com/a/b?old#old");
    Uri combined(base, "?new#frag");
    EXPECT_EQ(combined.getAbsolutePathProperty(), "/a/b");
    EXPECT_EQ(combined.getQueryProperty(), "?new");
    EXPECT_EQ(combined.getFragmentProperty(), "#frag");
}

TEST(UriTests, Resolve_QueryOnlyReferenceOverBaseWithNoQuery) {
    Uri base("http://example.com/a/b");
    Uri combined(base, "?q=1");
    EXPECT_EQ(combined.getAbsoluteUriProperty(), "http://example.com/a/b?q=1");
}

TEST(UriTests, Resolve_FragmentOnlyReferenceOverBaseWithNoQuery) {
    Uri base("http://example.com/a/b");
    Uri combined(base, "#top");
    EXPECT_EQ(combined.getAbsoluteUriProperty(), "http://example.com/a/b#top");
}

TEST(UriTests, Resolve_NetworkPathReference_ReplacesAuthority) {
    Uri base("http://example.com/a/b");
    Uri combined(base, "//other.example/c");
    EXPECT_EQ(combined.getSchemeProperty(), "http");
    EXPECT_EQ(combined.getHostProperty(), "other.example");
    EXPECT_EQ(combined.getAbsolutePathProperty(), "/c");
    EXPECT_EQ(combined.getAbsoluteUriProperty(), "http://other.example/c");
}

TEST(UriTests, Resolve_NetworkPathReference_KeepsBaseSchemeOnly) {
    Uri base("https://example.com:8443/a/b");
    Uri combined(base, "//other.example:9000/c?q#f");
    EXPECT_EQ(combined.getSchemeProperty(), "https");
    EXPECT_EQ(combined.getHostProperty(), "other.example");
    EXPECT_EQ(combined.getPortProperty(), 9000);
    EXPECT_EQ(combined.getQueryProperty(), "?q");
    EXPECT_EQ(combined.getFragmentProperty(), "#f");
}

TEST(UriTests, Resolve_NetworkPathReference_CarriesItsOwnUserInfo) {
    Uri base("http://olduser@example.com/a/b");
    Uri combined(base, "//newuser@other.example/c");
    EXPECT_EQ(combined.getUserInfoProperty(), "newuser");
    EXPECT_EQ(combined.getHostProperty(), "other.example");
}

TEST(UriTests, Resolve_PathReference_StillDropsBaseQueryAndFragment) {
    // Control for the case RFC 3986 already had right: a path-bearing reference replaces
    // the base's query and fragment rather than inheriting them.
    Uri base("http://example.com/a/b?old#old");
    Uri combined(base, "c");
    EXPECT_EQ(combined.getAbsoluteUriProperty(), "http://example.com/a/c");
}

TEST(UriTests, Resolve_QueryOnlyReference_PreservesBaseUserInfoAndPort) {
    Uri base("http://user:pw@example.com:8080/a/b");
    Uri combined(base, "?z=9");
    EXPECT_EQ(combined.getAbsoluteUriProperty(), "http://user:pw@example.com:8080/a/b?z=9");
}

TEST(UriTests, Resolve_EmptyReference_StillReturnsTheBase) {
    // Unchanged by #1990 and deliberately so: RFC 3986 drops the base fragment for an
    // empty reference, but this port has always returned the base verbatim and no
    // reference evidence survives here to decide which .NET does.
    Uri base("http://example.com/a/b?q#f");
    Uri combined(base, "");
    EXPECT_EQ(combined.getAbsoluteUriProperty(), "http://example.com/a/b?q#f");
}

// ---------------------------------------------------------------------------
// Documented contract — ticket #1994 (cause U-K). Uri.hpp's getSchemeProperty doc-comment
// promised a scheme returned "lower-case as parsed"; the parser has never lower-cased
// anything. The claim was corrected rather than the behaviour, because changing the
// behaviour changes equality and hash semantics (SR-AUD-142, approval-gated as #1995).
// These tests make the corrected documentation testable instead of merely asserted.
// ---------------------------------------------------------------------------

TEST(UriTests, DocumentedContract_SchemeCaseIsPreserved) {
    Uri u("HTTP://EXAMPLE.COM/Path");
    EXPECT_EQ(u.getSchemeProperty(), "HTTP");
}

TEST(UriTests, DocumentedContract_HostCaseIsPreserved) {
    Uri u("HTTP://EXAMPLE.COM/Path");
    EXPECT_EQ(u.getHostProperty(), "EXAMPLE.COM");
}

TEST(UriTests, DocumentedContract_CaseDifferingUrisAreNotEqualYet) {
    // Pins the open SR-AUD-142 divergence so #1995's repair is a deliberate, visible
    // change rather than a silent one: this test must be updated when identity changes.
    Uri a("HTTP://EXAMPLE.COM:80/Path");
    Uri b("http://example.com/Path");
    EXPECT_FALSE(a == b);
    // The matching hash inequality was removed by #2284: unequal values are permitted to hash
    // equally (docs/HashAssertionContractRule.md R2), so identity is pinned on operator== alone
    // and that is the single line #1995 has to update.
}

TEST(UriTests, DocumentedContract_MixedCaseSchemeHasNoDefaultPort) {
    // A direct consequence of the case-preserving contract: defaultPortForScheme matches
    // lower-case names only, so "HTTP://" gets no default port.
    Uri u("HTTP://example.com/");
    EXPECT_EQ(u.getPortProperty(), -1);
}

TEST(UriTests, DocumentedContract_EmptyStringThrows) {
    EXPECT_THROW(Uri(""), System::UriFormatException);
}

// ---------------------------------------------------------------------------
// Empty authority — ticket #2000 (post-audit defect, no SR-AUD identifier).
//
// Before this ticket every authority-bearing shape with nothing where the host belongs was
// accepted and given the SCHEME'S DEFAULT PORT: "http://", "http:///path", "http://:80/path"
// and "http://user@/path" all reported host == "" with Port == 80 and Authority == "" (or
// ":8080"). A caller that connected to Host:Port reached a real port number on no host at
// all, and this repository's OTHER url parser already disagreed —
// HttpClientUrlParseTests.EmptyHostThrowsUriFormatException pins "http://:8080/path" as a
// UriFormatException in HttpClient::parseUrl.
//
// The selected contract is NOT "an authority must be non-empty". A host-less authority stays
// legal whenever no port is associated with the URI at all, so "file:///path" (RFC 8089) and
// every unknown hierarchical scheme are untouched; only text that produced a self-
// contradictory object — a port for a host that does not exist — is rejected.
// See docs/SystemUriNamespaceReviewPlan.md §26.
// ---------------------------------------------------------------------------

TEST(UriTests, EmptyAuthority_FileSchemeIsStillAccepted) {
    // The case the contract exists to protect. Two tracked downstream suites construct this
    // exact shape (XmlUrlResolverTests.GetEntity_MissingFile_Throws and IOTests'
    // FileFormatException case); a blanket "authority must be non-empty" rule would break
    // both and would be wrong about RFC 8089 besides.
    Uri u("file:///path/to/file.txt");
    EXPECT_TRUE(u.getIsAbsoluteUriProperty());
    EXPECT_EQ(u.getSchemeProperty(), "file");
    EXPECT_EQ(u.getHostProperty(), "");
    EXPECT_EQ(u.getAuthorityProperty(), "");
    EXPECT_EQ(u.getPortProperty(), -1);
    EXPECT_EQ(u.getAbsolutePathProperty(), "/path/to/file.txt");
    EXPECT_EQ(u.getQueryProperty(), "");
    EXPECT_EQ(u.getFragmentProperty(), "");
    EXPECT_EQ(u.getOriginalStringProperty(), "file:///path/to/file.txt");
    EXPECT_EQ(u.ToString(), "file:///path/to/file.txt");
    EXPECT_EQ(u.getAbsoluteUriProperty(), "file:///path/to/file.txt");
}

TEST(UriTests, EmptyAuthority_FileSchemeBareAndRootAreAccepted) {
    EXPECT_EQ(Uri("file://").getAbsolutePathProperty(), "/");
    EXPECT_EQ(Uri("file:///").getAbsolutePathProperty(), "/");
    EXPECT_EQ(Uri("file://").getPortProperty(), -1);
    EXPECT_EQ(Uri("file:///").getHostProperty(), "");
}

TEST(UriTests, EmptyAuthority_FileSchemeWithHostIsUnaffected) {
    Uri u("file://host/share");
    EXPECT_EQ(u.getHostProperty(), "host");
    EXPECT_EQ(u.getAbsolutePathProperty(), "/share");
    EXPECT_EQ(u.getPortProperty(), -1);
}

TEST(UriTests, EmptyAuthority_FileSchemeWithAnEmptyPortIsAccepted) {
    // "file://:/path" carries a colon but no port digits, so #1989's empty-port branch
    // applies the scheme default, which for "file" is -1 — no port, so no contradiction.
    Uri u("file://:/path");
    EXPECT_EQ(u.getHostProperty(), "");
    EXPECT_EQ(u.getPortProperty(), -1);
    EXPECT_EQ(u.getAbsolutePathProperty(), "/path");
}

TEST(UriTests, EmptyAuthority_FileSchemeWithAnExplicitPortIsRejected) {
    // The one "file" shape that IS rejected, and the reason the rule is stated in terms of
    // the port rather than the scheme: an explicit port with no host is the same fabricated
    // authority as "http://:8080/".
    EXPECT_THROW(Uri("file://:8080/path"), System::UriFormatException);
}

TEST(UriTests, EmptyAuthority_HostRequiredSchemesAreRejected) {
    // Every scheme in this file's own default-port table, in every spelling that reaches the
    // authority branch with an empty host.
    const char* rejected[] = {
        "http://", "http:///", "http:///path", "http://:80/path", "http://:/path", "http://:",
        "http://?query", "http://#fragment", "http://?q#f",
        "https://", "https:///secure", "ws:///socket", "wss:///socket", "ftp:///pub",
        "gopher:///g", "nntp:///n", "telnet:///", "ldap:///dc=example", "net.tcp:///svc",
    };
    for (const char* text : rejected) {
        EXPECT_THROW(Uri(std::string(text)), System::UriFormatException)
            << "expected rejection for " << text;
    }
}

TEST(UriTests, EmptyAuthority_UserInfoWithoutAHostIsRejected) {
    EXPECT_THROW(Uri("http://user@/path"), System::UriFormatException);
    EXPECT_THROW(Uri("http://user:pass@/path"), System::UriFormatException);
    EXPECT_THROW(Uri("http://@/path"), System::UriFormatException);
    EXPECT_THROW(Uri("http://user@:8080/path"), System::UriFormatException);
}

TEST(UriTests, EmptyAuthority_EmptyUserInfoWithAHostIsStillAccepted) {
    // The control: it is the HOST that must exist, not the user-info.
    Uri u("http://@example.com/");
    EXPECT_EQ(u.getUserInfoProperty(), "");
    EXPECT_EQ(u.getHostProperty(), "example.com");
    EXPECT_EQ(u.getPortProperty(), 80);
}

TEST(UriTests, EmptyAuthority_UnknownSchemesAreUnaffected) {
    // No default-port entry means no fabricated port, so nothing narrows here. Pinned so a
    // later ticket that starts rejecting unknown schemes is a deliberate decision.
    for (const char* text : {"unknown://", "unknown:///path", "git+ssh:///repo", "ssh:///host"}) {
        Uri u{std::string(text)};
        EXPECT_TRUE(u.getIsAbsoluteUriProperty()) << text;
        EXPECT_EQ(u.getHostProperty(), "") << text;
        EXPECT_EQ(u.getPortProperty(), -1) << text;
    }
}

TEST(UriTests, EmptyAuthority_OpaqueSchemesKeepTheirGrammar) {
    // The opaque branch has no authority at all and returns before the check, so #1989's
    // default port for an opaque URI survives untouched.
    Uri mail("mailto:a@b.com");
    EXPECT_EQ(mail.getAbsolutePathProperty(), "a@b.com");
    EXPECT_EQ(mail.getPortProperty(), 25);
    EXPECT_EQ(mail.getHostProperty(), "");

    Uri urn("urn:isbn:0-395-36341-1");
    EXPECT_EQ(urn.getAbsolutePathProperty(), "isbn:0-395-36341-1");
    EXPECT_EQ(urn.getPortProperty(), -1);

    Uri tel("telnet:host.example.com");
    EXPECT_EQ(tel.getAbsolutePathProperty(), "host.example.com");
    EXPECT_EQ(tel.getPortProperty(), 23);

    EXPECT_EQ(Uri("mailto:").getAbsolutePathProperty(), "");
}

TEST(UriTests, EmptyAuthority_AuthorityBearingSpellingOfAnOpaqueSchemeIsRejected) {
    // "mailto://" is not the opaque grammar — "//" follows the colon, so it takes the
    // authority branch and used to report host "" with port 25. "mailto:///c" is the same
    // shape and is what ticket #2001's opaque-base resolution produced.
    EXPECT_THROW(Uri("mailto://"), System::UriFormatException);
    EXPECT_THROW(Uri("mailto:///c"), System::UriFormatException);
}

TEST(UriTests, EmptyAuthority_RelativeReferencesAreUnaffected) {
    // "unaffected" means BY #2000: none of these becomes an absolute URI and none is
    // rejected. Their component split is ticket #2002's subject, so the assertion is on
    // OriginalString, which no ticket in this batch moves.
    for (const char* text : {"//", "//other.example/c", "/path", "path", "?q", "#f"}) {
        Uri u{std::string(text)};
        EXPECT_FALSE(u.getIsAbsoluteUriProperty()) << text;
        EXPECT_EQ(u.getOriginalStringProperty(), text) << text;
    }
}

TEST(UriTests, EmptyAuthority_MixedCaseSchemeIsStillAcceptedAndWhy) {
    // An honest consequence of the still-open SR-AUD-142 (scheme case is not normalised):
    // defaultPortForScheme matches lower-case names only, so "HTTP:///path" has no default
    // port and therefore no contradiction to reject. This test must be revisited when #1995
    // lands, exactly like DocumentedContract_CaseDifferingUrisAreNotEqualYet.
    Uri u("HTTP:///path");
    EXPECT_TRUE(u.getIsAbsoluteUriProperty());
    EXPECT_EQ(u.getHostProperty(), "");
    EXPECT_EQ(u.getPortProperty(), -1);
}

TEST(UriTests, EmptyAuthority_ExceptionTypeAndMessageAreExact) {
    try {
        Uri u("http:///path");
        FAIL() << "expected UriFormatException";
    } catch (const System::UriFormatException& e) {
        EXPECT_STREQ(e.what(), "Invalid URI: The hostname could not be parsed.");
    }
}

TEST(UriTests, EmptyAuthority_PortAndBracketErrorsStillWin) {
    // Validation ORDER: a malformed port and a malformed IP literal are both diagnosed
    // before the host is examined, so the caller keeps the more specific message.
    try {
        Uri u("http://:notaport/p");
        FAIL() << "expected UriFormatException";
    } catch (const System::UriFormatException& e) {
        EXPECT_STREQ(e.what(), "Invalid URI: Invalid port specified.");
    }
    try {
        Uri u("http://[]/path");
        FAIL() << "expected UriFormatException";
    } catch (const System::UriFormatException& e) {
        EXPECT_STREQ(e.what(), "Invalid URI: An invalid IPv6 address was specified.");
    }
}

TEST(UriTests, EmptyAuthority_UriKindDomainCheckStillRunsFirst) {
    // #1992's argument check runs before parse(), so an out-of-domain kind is still an
    // ArgumentException rather than this ticket's UriFormatException.
    try {
        Uri u("http:///path", static_cast<UriKind>(99));
        FAIL() << "expected ArgumentException";
    } catch (const System::ArgumentException& e) {
        EXPECT_EQ(e.getParamNameProperty(), "uriKind");
    }
}

TEST(UriTests, EmptyAuthority_RejectedByEveryUriKindOverload) {
    EXPECT_THROW(Uri("http:///path", UriKind::Absolute), System::UriFormatException);
    EXPECT_THROW(Uri("http:///path", UriKind::Relative), System::UriFormatException);
    EXPECT_THROW(Uri("http:///path", UriKind::RelativeOrAbsolute), System::UriFormatException);
}

TEST(UriTests, EmptyAuthority_TryCreateReportsFailureAndLeavesNoObject) {
    // No partially initialised Uri escapes: TryCreate's catch-all turns the throw into
    // false + nullptr, and the file scheme still succeeds through the same entry point.
    std::shared_ptr<Uri> result = std::make_shared<Uri>("http://example.com/");
    EXPECT_FALSE(Uri::TryCreate("http:///path", UriKind::Absolute, result));
    EXPECT_EQ(result, nullptr);
    EXPECT_FALSE(Uri::TryCreate("http://:8080/path", UriKind::RelativeOrAbsolute, result));
    EXPECT_EQ(result, nullptr);
    EXPECT_TRUE(Uri::TryCreate("file:///path", UriKind::Absolute, result));
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->getHostProperty(), "");
}

TEST(UriTests, EmptyAuthority_NetworkPathReferenceToAnEmptyAuthorityIsRejected) {
    // Reached through relative resolution rather than through a constructor string:
    // #1990 turns a "//" reference into scheme + ":" + reference, i.e. "http://".
    Uri base("http://example.com/a/b");
    EXPECT_THROW(Uri(base, "//"), System::UriFormatException);
}

TEST(UriTests, EmptyAuthority_OrdinaryRelativeResolutionIsUnaffected) {
    Uri base("http://example.com/a/b");
    EXPECT_EQ(Uri(base, "c").getAbsoluteUriProperty(), "http://example.com/a/c");
    EXPECT_EQ(Uri(base, "/c").getAbsoluteUriProperty(), "http://example.com/c");
    EXPECT_EQ(Uri(base, "?new").getAbsoluteUriProperty(), "http://example.com/a/b?new");
    EXPECT_EQ(Uri(base, "#new").getAbsoluteUriProperty(), "http://example.com/a/b#new");
    EXPECT_EQ(Uri(base, "//other.example/c").getAbsoluteUriProperty(), "http://other.example/c");

    Uri fileBase("file:///a/b");
    EXPECT_EQ(Uri(fileBase, "c").getAbsoluteUriProperty(), "file:///a/c");
}

TEST(UriTests, EmptyAuthority_CopyAndAssignOfAHostLessUriStillWork) {
    Uri original("file:///p");
    Uri copied(original);
    Uri assigned("http://example.com/");
    assigned = original;
    EXPECT_EQ(copied.ToString(), "file:///p");
    EXPECT_EQ(assigned.ToString(), "file:///p");
    EXPECT_TRUE(copied == assigned);
    EXPECT_EQ(copied.GetHashCode(), assigned.GetHashCode());
}

// ---------------------------------------------------------------------------
// Layout pins. docs/SystemUriNamespaceReviewPlan.md §9.2 said sizeof(System::Uri) == 240 was
// "re-asserted by permanent tests"; measured on 2026-08-03 it was not — no test in the
// repository referenced sizeof at all. The pin is added here so the claim becomes true.
// ---------------------------------------------------------------------------

TEST(UriTests, Layout_SizeOfUriIsPinned) {
    EXPECT_EQ(sizeof(System::Uri), 240u);
}

// ---------------------------------------------------------------------------
// Resolution against an OPAQUE base — ticket #2001 (post-audit defect, no SR-AUD identifier).
//
// The reconstruction in Uri(base, relative) emitted "scheme://" unconditionally, so the
// base's OPAQUE PATH was fed through parse()'s authority splitter. The named shape was
// Uri(Uri("mailto:a@b.com"), "c") -> "mailto:///c"; measurement found two worse ones:
//   ("mailto:a@b.com", "?q") -> "mailto://a@b.com?q" with Host == "b.com" — the mailbox
//                               domain silently became a host name;
//   ("urn:isbn:0-395-36341-1", "?q") -> threw "Invalid port specified." because
//                               "isbn:0-395-36341-1" was read as "host:port".
// parse() classifies these bases as opaque with Host == "", so the resolver was contradicting
// the parser about the same base. The repair derives opacity from the base's own original
// string with findSchemeColon — the single scheme-recognition point since #1988 — so there is
// no new member and no layout change. See docs/SystemUriNamespaceReviewPlan.md §28.
// ---------------------------------------------------------------------------

TEST(UriTests, OpaqueBase_PathReferenceReplacesTheLastSegment) {
    // RFC 3986 §5.3 merge with an undefined base authority: the reference is appended to all
    // but the last segment of the base path, which for a segment-free opaque path is nothing.
    Uri base("mailto:a@b.com");
    Uri combined(base, "c");
    EXPECT_EQ(combined.getAbsoluteUriProperty(), "mailto:c");
    EXPECT_EQ(combined.getSchemeProperty(), "mailto");
    EXPECT_EQ(combined.getHostProperty(), "");
    EXPECT_EQ(combined.getAuthorityProperty(), "");
    EXPECT_EQ(combined.getAbsolutePathProperty(), "c");
}

TEST(UriTests, OpaqueBase_NoAuthorityIsEverFabricated) {
    // The exact defect the ticket names: "mailto:///c" contained an authority the base never
    // had. It is now unreachable — and would be rejected by #2000 even if it were produced.
    const char* bases[] = {"mailto:a@b.com", "urn:isbn:0-395-36341-1", "news:comp.lang.c++",
                           "tag:example.com,2026:x", "about:blank", "javascript:void(0)",
                           "telnet:host.example.com"};
    for (const char* text : bases) {
        Uri base{std::string(text)};
        Uri combined(base, "c");
        EXPECT_EQ(combined.getAbsoluteUriProperty(),
                  base.getSchemeProperty() + ":c") << text;
        EXPECT_EQ(combined.getHostProperty(), "") << text;
        EXPECT_EQ(combined.getAbsoluteUriProperty().find("//"), std::string::npos) << text;
    }
}

TEST(UriTests, OpaqueBase_QueryOnlyAndFragmentOnlyKeepTheBasePath) {
    // The two shapes whose old answer invented a HOST out of the mailbox domain.
    Uri base("mailto:a@b.com");
    Uri withQuery(base, "?q");
    EXPECT_EQ(withQuery.getAbsoluteUriProperty(), "mailto:a@b.com?q");
    EXPECT_EQ(withQuery.getHostProperty(), "");
    EXPECT_EQ(withQuery.getAbsolutePathProperty(), "a@b.com");
    EXPECT_EQ(withQuery.getQueryProperty(), "?q");

    Uri withFragment(base, "#f");
    EXPECT_EQ(withFragment.getAbsoluteUriProperty(), "mailto:a@b.com#f");
    EXPECT_EQ(withFragment.getHostProperty(), "");
    EXPECT_EQ(withFragment.getFragmentProperty(), "#f");
}

TEST(UriTests, OpaqueBase_QueryReferenceNoLongerFalselyRejected) {
    // "urn:isbn:0-395-36341-1" + "?q" used to throw because "isbn:0-395-36341-1" was split as
    // host:port. A false rejection of well-formed input, now resolved.
    Uri base("urn:isbn:0-395-36341-1");
    EXPECT_EQ(Uri(base, "?q").getAbsoluteUriProperty(), "urn:isbn:0-395-36341-1?q");
    EXPECT_EQ(Uri(base, "#f").getAbsoluteUriProperty(), "urn:isbn:0-395-36341-1#f");
}

TEST(UriTests, OpaqueBase_AbsolutePathReferenceKeepsOnlyTheScheme) {
    Uri base("mailto:a@b.com");
    Uri combined(base, "/c");
    EXPECT_EQ(combined.getAbsoluteUriProperty(), "mailto:/c");
    EXPECT_EQ(combined.getHostProperty(), "");
    EXPECT_EQ(combined.getAbsolutePathProperty(), "/c");
}

TEST(UriTests, OpaqueBase_NetworkPathReferenceStillSuppliesItsOwnAuthority) {
    // Unchanged and deliberately so: RFC 3986 §5.2.2 says a reference beginning "//" carries
    // its own authority whatever the base was, so only the scheme survives.
    Uri base("mailto:a@b.com");
    Uri combined(base, "//other/c");
    EXPECT_EQ(combined.getAbsoluteUriProperty(), "mailto://other/c");
    EXPECT_EQ(combined.getHostProperty(), "other");
}

TEST(UriTests, OpaqueBase_AbsoluteReferenceStillDiscardsTheBase) {
    Uri base("mailto:a@b.com");
    EXPECT_EQ(Uri(base, "mailto:x@y.com").getAbsoluteUriProperty(), "mailto:x@y.com");
    EXPECT_EQ(Uri(base, "http://example.com/p").getAbsoluteUriProperty(), "http://example.com/p");
}

TEST(UriTests, OpaqueBase_EmptyReferenceStillReturnsTheBase) {
    Uri base("mailto:a@b.com");
    EXPECT_EQ(Uri(base, "").getAbsoluteUriProperty(), "mailto:a@b.com");
}

TEST(UriTests, OpaqueBase_DoesNotChangeHierarchicalResolution) {
    // The controls. Every hierarchical shape #1990 fixed must be byte-identical.
    Uri http("http://user:pw@example.com:8080/a/b?old#old");
    EXPECT_EQ(Uri(http, "c").getAbsoluteUriProperty(), "http://user:pw@example.com:8080/a/c");
    EXPECT_EQ(Uri(http, "/c").getAbsoluteUriProperty(), "http://user:pw@example.com:8080/c");
    EXPECT_EQ(Uri(http, "?new").getAbsoluteUriProperty(), "http://user:pw@example.com:8080/a/b?new");
    EXPECT_EQ(Uri(http, "#new").getAbsoluteUriProperty(), "http://user:pw@example.com:8080/a/b?old#new");
    EXPECT_EQ(Uri(http, "//other.example/c").getAbsoluteUriProperty(), "http://other.example/c");
    EXPECT_EQ(Uri(http, "../x").getAbsoluteUriProperty(), "http://user:pw@example.com:8080/x");

    Uri file("file:///a/b");
    EXPECT_EQ(Uri(file, "c").getAbsoluteUriProperty(), "file:///a/c");
    EXPECT_EQ(Uri(file, "/c").getAbsoluteUriProperty(), "file:///c");
    EXPECT_EQ(Uri(file, "?q").getAbsoluteUriProperty(), "file:///a/b?q");
}

TEST(UriTests, OpaqueBase_ResultsRoundTrip) {
    // Every resolved value must re-parse to itself, which is what proves the emitted text is
    // a URI of the same shape rather than something only this constructor can read.
    Uri base("mailto:a@b.com");
    for (const char* rel : {"c", "/c", "?q", "#f", "//other/c"}) {
        Uri combined(base, rel);
        Uri reparsed(combined.getAbsoluteUriProperty());
        EXPECT_TRUE(reparsed == combined) << rel;
        EXPECT_EQ(reparsed.GetHashCode(), combined.GetHashCode()) << rel;
        EXPECT_EQ(reparsed.getSchemeProperty(), combined.getSchemeProperty()) << rel;
        EXPECT_EQ(reparsed.getHostProperty(), combined.getHostProperty()) << rel;
        EXPECT_EQ(reparsed.getAbsolutePathProperty(), combined.getAbsolutePathProperty()) << rel;
    }
}

TEST(UriTests, OpaqueBase_RelativeBaseIsStillRejected) {
    Uri relative("just/a/path");
    EXPECT_THROW(Uri(relative, "c"), System::ArgumentOutOfRangeException);
}

// ---------------------------------------------------------------------------
// Relative-reference components — ticket #2002 (post-audit defect, no SR-AUD identifier).
//
// The relative branch of parse() stored the whole reference as the path, so Uri("a?b#c")
// reported AbsolutePath == "a?b#c" with Query and Fragment empty, and Uri("?b") reported
// AbsolutePath == "?b". RFC 3986 §4.2 spells a relative reference
// `relative-ref = relative-part [ "?" query ] [ "#" fragment ]`, and BOTH other branches of
// the same function already split those components — only this one did not. The evidence is
// therefore the same kind #1988 and #1989 landed on: a contradiction inside one file,
// resolved in favour of the grammar the file itself cites. Impact was measured, not assumed:
// no consumer in this repository reads AbsolutePath, Query or Fragment off a relative Uri
// (XmlUrlResolver::GetEntity explicitly branches on IsAbsoluteUri and uses AbsoluteUri
// instead), and relative resolution reads the raw reference string, not these components.
// See docs/SystemUriNamespaceReviewPlan.md §29.
// ---------------------------------------------------------------------------

TEST(UriTests, RelativeComponents_QueryIsSplitOutOfThePath) {
    Uri u("a?b");
    EXPECT_FALSE(u.getIsAbsoluteUriProperty());
    EXPECT_EQ(u.getAbsolutePathProperty(), "a");
    EXPECT_EQ(u.getQueryProperty(), "?b");
    EXPECT_EQ(u.getFragmentProperty(), "");
}

TEST(UriTests, RelativeComponents_FragmentIsSplitOutOfThePath) {
    Uri u("a#c");
    EXPECT_EQ(u.getAbsolutePathProperty(), "a");
    EXPECT_EQ(u.getQueryProperty(), "");
    EXPECT_EQ(u.getFragmentProperty(), "#c");
}

TEST(UriTests, RelativeComponents_QueryAndFragmentTogether) {
    Uri u("a?b#c");
    EXPECT_EQ(u.getAbsolutePathProperty(), "a");
    EXPECT_EQ(u.getQueryProperty(), "?b");
    EXPECT_EQ(u.getFragmentProperty(), "#c");
    EXPECT_EQ(u.getPathAndQueryProperty(), "a?b");
}

TEST(UriTests, RelativeComponents_QueryOnlyAndFragmentOnlyHaveAnEmptyPath) {
    Uri query("?b");
    EXPECT_EQ(query.getAbsolutePathProperty(), "");
    EXPECT_EQ(query.getQueryProperty(), "?b");

    Uri fragment("#c");
    EXPECT_EQ(fragment.getAbsolutePathProperty(), "");
    EXPECT_EQ(fragment.getFragmentProperty(), "#c");
    EXPECT_EQ(fragment.getQueryProperty(), "");
}

TEST(UriTests, RelativeComponents_EmptyQueryAndEmptyFragmentAreKept) {
    // Exactly what the absolute branch does: a bare '?' is a present-but-empty query.
    EXPECT_EQ(Uri("a?").getQueryProperty(), "?");
    EXPECT_EQ(Uri("a?").getAbsolutePathProperty(), "a");
    EXPECT_EQ(Uri("a#").getFragmentProperty(), "#");
    EXPECT_EQ(Uri("a#").getAbsolutePathProperty(), "a");
    EXPECT_EQ(Uri("a?#").getQueryProperty(), "?");
    EXPECT_EQ(Uri("a?#").getFragmentProperty(), "#");
}

TEST(UriTests, RelativeComponents_FirstDelimiterWins) {
    // The fragment is split first, so a '?' after a '#' belongs to the fragment and a second
    // '?' belongs to the query — identical to the absolute and opaque branches.
    EXPECT_EQ(Uri("a?b?c").getQueryProperty(), "?b?c");
    EXPECT_EQ(Uri("a#c#d").getFragmentProperty(), "#c#d");
    Uri fragFirst("a#c?b");
    EXPECT_EQ(fragFirst.getAbsolutePathProperty(), "a");
    EXPECT_EQ(fragFirst.getFragmentProperty(), "#c?b");
    EXPECT_EQ(fragFirst.getQueryProperty(), "");
}

TEST(UriTests, RelativeComponents_EscapedDelimitersAreNotSplit) {
    // The no-percent-decoding boundary is unchanged: "%3F" and "%23" are ordinary characters.
    EXPECT_EQ(Uri("a%3Fb").getAbsolutePathProperty(), "a%3Fb");
    EXPECT_EQ(Uri("a%3Fb").getQueryProperty(), "");
    EXPECT_EQ(Uri("a%23c").getAbsolutePathProperty(), "a%23c");
    EXPECT_EQ(Uri("a%23c").getFragmentProperty(), "");
}

TEST(UriTests, RelativeComponents_AbsolutePathAndNetworkPathReferences) {
    Uri absolutePath("/p/q?b#c");
    EXPECT_EQ(absolutePath.getAbsolutePathProperty(), "/p/q");
    EXPECT_EQ(absolutePath.getQueryProperty(), "?b");
    EXPECT_EQ(absolutePath.getFragmentProperty(), "#c");

    Uri networkPath("//host/p?b#c");
    EXPECT_FALSE(networkPath.getIsAbsoluteUriProperty());
    EXPECT_EQ(networkPath.getAbsolutePathProperty(), "//host/p");
    EXPECT_EQ(networkPath.getQueryProperty(), "?b");
}

TEST(UriTests, RelativeComponents_OriginalStringAndIdentityAreUnchanged) {
    // The four things this ticket must NOT move: OriginalString, AbsoluteUri, ToString and
    // identity all read the verbatim input.
    Uri u("a?b#c");
    EXPECT_EQ(u.getOriginalStringProperty(), "a?b#c");
    EXPECT_EQ(u.getAbsoluteUriProperty(), "a?b#c");
    EXPECT_EQ(u.ToString(), "a?b#c");
    Uri same("a?b#c");
    EXPECT_TRUE(u == same);
    EXPECT_EQ(u.GetHashCode(), same.GetHashCode());
    Uri other("a?b");
    EXPECT_FALSE(u == other);
}

TEST(UriTests, RelativeComponents_RoundTrip) {
    for (const char* text : {"a?b", "a#c", "a?b#c", "?b", "#c", "/p/q?b#c"}) {
        Uri u{std::string(text)};
        Uri again(u.getAbsoluteUriProperty());
        EXPECT_TRUE(again == u) << text;
        EXPECT_EQ(again.getAbsolutePathProperty(), u.getAbsolutePathProperty()) << text;
        EXPECT_EQ(again.getQueryProperty(), u.getQueryProperty()) << text;
        EXPECT_EQ(again.getFragmentProperty(), u.getFragmentProperty()) << text;
    }
}

TEST(UriTests, RelativeComponents_ResolutionIsUnaffected) {
    // Uri(base, relative) consumes the raw reference string, never these components, so every
    // resolved value is byte-identical to what #1990 landed.
    Uri base("http://example.com/a/b");
    EXPECT_EQ(Uri(base, "c?q#f").getAbsoluteUriProperty(), "http://example.com/a/c?q#f");
    EXPECT_EQ(Uri(base, "?q").getAbsoluteUriProperty(), "http://example.com/a/b?q");
    EXPECT_EQ(Uri(base, "#f").getAbsoluteUriProperty(), "http://example.com/a/b#f");
    EXPECT_EQ(Uri(base, "c?q").getAbsoluteUriProperty(), "http://example.com/a/c?q");
    EXPECT_EQ(Uri(base, "c#f").getAbsoluteUriProperty(), "http://example.com/a/c#f");
}

TEST(UriTests, RelativeComponents_UriKindRelativeSeesTheSameSplit) {
    Uri u("a?b#c", UriKind::Relative);
    EXPECT_EQ(u.getAbsolutePathProperty(), "a");
    EXPECT_EQ(u.getQueryProperty(), "?b");
    EXPECT_EQ(u.getFragmentProperty(), "#c");

    std::shared_ptr<Uri> out;
    ASSERT_TRUE(Uri::TryCreate("a?b#c", UriKind::Relative, out));
    ASSERT_NE(out, nullptr);
    EXPECT_EQ(out->getAbsolutePathProperty(), "a");
}

// ---------------------------------------------------------------------------
// Embedded NUL — ticket #2003, pinned rather than changed.
//
// Measured across every component position: a NUL is carried through as ordinary data. There
// is NO prefix-only parse, NO silent truncation and no length loss — Uri("http://h/a\0b")
// keeps all 12 bytes, round-trips, and does NOT compare equal to Uri("http://h/a"). The port
// position already rejects it, because a NUL is not a digit. Whether .NET rejects a NUL
// outright could not be measured here (/rv/tmp/runtime/src/libraries/ is absent), so choosing
// rejection would be a narrowing on no surviving evidence — the line #1998 and #1963 sit on.
// The current, self-consistent behaviour is pinned so a later change is deliberate.
// See docs/SystemUriNamespaceReviewPlan.md §30.
// ---------------------------------------------------------------------------

TEST(UriTests, EmbeddedNul_IsCarriedThroughEveryComponentWithoutTruncation) {
    // #2359: a NUL in the HOST is now rejected, because NUL is not in DomainNameHelper's
    // s_validChars either -- the same rule that rejects a space. Every other component is
    // untouched, which is what this test is about.
    EXPECT_THROW((void)Uri(std::string("http://h\0st/p", 13)), System::UriFormatException);

    Uri path(std::string("http://h/a\0b", 12));
    EXPECT_EQ(path.getAbsolutePathProperty(), std::string("/a\0b", 4));
    EXPECT_EQ(path.getAbsoluteUriProperty().size(), 12u);
    EXPECT_EQ(path.ToString().size(), 12u);

    Uri query(std::string("http://h/p?a\0b", 14));
    EXPECT_EQ(query.getQueryProperty(), std::string("?a\0b", 4));

    Uri fragment(std::string("http://h/p#a\0b", 14));
    EXPECT_EQ(fragment.getFragmentProperty(), std::string("#a\0b", 4));

    Uri userInfo(std::string("http://u\0r@h/p", 14));
    EXPECT_EQ(userInfo.getUserInfoProperty(), std::string("u\0r", 3));

    Uri relative(std::string("a\0b", 3));
    EXPECT_FALSE(relative.getIsAbsoluteUriProperty());
    EXPECT_EQ(relative.getAbsolutePathProperty(), std::string("a\0b", 3));

    Uri opaque(std::string("mailto:a\0b", 10));
    EXPECT_EQ(opaque.getAbsolutePathProperty(), std::string("a\0b", 3));
}

TEST(UriTests, EmbeddedNul_DoesNotProduceAPrefixOnlyParse) {
    // The dangerous failure this ticket exists to rule out: a URI that silently means only
    // the text before the NUL. It does not — the two are unequal, and a caller keeps them apart
    // by that inequality. (#2284 removed a matching hash inequality here: unequal values are
    // permitted to hash equally, and a hashed container separates colliding keys by Equals
    // anyway — docs/HashAssertionContractRule.md R2.)
    Uri withNul(std::string("http://h/a\0b", 12));
    Uri prefix("http://h/a");
    EXPECT_FALSE(withNul == prefix);
    Uri reparsed(withNul.getAbsoluteUriProperty());
    EXPECT_TRUE(reparsed == withNul);
}

TEST(UriTests, EmbeddedNul_InThePortPositionIsStillRejected) {
    EXPECT_THROW(Uri(std::string("http://h:8\0 0/p", 15)), System::UriFormatException);
}

// ---------------------------------------------------------------------------
// Surrounding whitespace — ticket #2005 RESOLVED.
//
// The deferral was correct: whether .NET trims could not be measured, so nothing was changed
// and the ASYMMETRY was recorded rather than tidied — leading whitespace made the reference
// relative, while trailing whitespace on an otherwise absolute URI was accepted into the path.
//
// With the reference present, .NET trims BOTH ends before parsing: leading in ParseScheme
// (Uri.cs:3513-3518) and trailing in GetCanonicalPath and CreateThis (:3464-3469, :1992-1993),
// in both cases with UriHelper.IsLWS — space, LF, CR and TAB, and nothing else
// (UriHelper.cs:556-559). See docs/SystemUriNamespaceReviewPlan.md §31.
// ---------------------------------------------------------------------------

TEST(UriTests, Fix2005_SurroundingWhitespaceIsTrimmedBeforeParsing) {
    // The headline: this used to be a RELATIVE reference whose whole path was the padded text.
    Uri padded("  http://example.com/  ");
    EXPECT_TRUE(padded.getIsAbsoluteUriProperty());
    EXPECT_EQ(padded.getSchemeProperty(), "http");
    EXPECT_EQ(padded.getHostProperty(), "example.com");
    EXPECT_EQ(padded.getAbsolutePathProperty(), "/");

    // The asymmetry is gone: both ends now behave the same way.
    EXPECT_TRUE(Uri(" http://example.com/").getIsAbsoluteUriProperty());
    EXPECT_EQ(Uri("http://example.com/ ").getAbsolutePathProperty(), "/");
}

TEST(UriTests, Fix2005_TheTrimmedSetIsExactlyDotNetsIsLWS) {
    // UriHelper.cs:556-559 -- space, LF, CR, TAB. Deliberately NOT std::isspace, which also
    // folds vertical tab and form feed and is locale-sensitive.
    for (const char* pad : {" ", "\t", "\r", "\n", " \t\r\n "}) {
        SCOPED_TRACE(pad);
        Uri padded(std::string(pad) + "http://h/p" + pad);
        EXPECT_TRUE(padded.getIsAbsoluteUriProperty());
        EXPECT_EQ(padded.getHostProperty(), "h");
        EXPECT_EQ(padded.getAbsolutePathProperty(), "/p");
    }

    // A vertical tab is NOT linear whitespace, so it is left to fail as an ordinary bad
    // character rather than being trimmed away.
    EXPECT_FALSE(Uri("\vhttp://h/p").getIsAbsoluteUriProperty())
        << "trimming more than IsLWS would accept text .NET rejects";
}

TEST(UriTests, Fix2005_AWhitespaceOnlyStringIsStillEmpty) {
    // Trimming must not turn "   " into a URI. It becomes the empty string, which the
    // constructor already rejects.
    for (const char* blank : {" ", "\t", " \r\n\t "}) {
        SCOPED_TRACE(blank);
        EXPECT_THROW((void)Uri(std::string(blank)), System::UriFormatException);
    }
}

TEST(UriTests, Fix2005_AnUnpaddedUriIsUntouched) {
    // The invariance row.
    Uri plain("http://example.com/a/b?q=1#f");
    EXPECT_EQ(plain.getAbsoluteUriProperty(), "http://example.com/a/b?q=1#f");
    EXPECT_EQ(plain.getAbsolutePathProperty(), "/a/b");
    EXPECT_EQ(plain.getQueryProperty(), "?q=1");
    EXPECT_EQ(plain.getFragmentProperty(), "#f");
}

// FLIPPED by #2359 (2026-08-18), the half #2005 deliberately left open. The HOST half is now a
// rejection; the PATH half is unchanged, and keeping both in one test is what shows the narrowing
// stopped where .NET's does.
TEST(UriTests, Fix2359_WhitespaceInsideTheHostIsRejectedButNotInsideThePath) {
    // DomainNameHelper.IsValid tests `IndexOfAnyExcept(s_validChars)` over exactly
    // "-0123456789A-Z_a-z." (DomainNameHelper.cs:36-37, :100-119). A space is not in it and is
    // not one of the delimiters that truncate the host instead, so the host does not parse -- and
    // no built-in scheme sets AllowAnyOtherHost, so CheckAuthorityHelper reports BadHostName
    // (Uri.cs:3902-3928, UriSyntax.cs, all fourteen flag constants).
    EXPECT_THROW((void)Uri("http://exa mple.com/"), System::UriFormatException);
    EXPECT_THROW((void)Uri("https://exa mple.com/"), System::UriFormatException);

    // The PATH is a different production entirely and is untouched.
    EXPECT_EQ(Uri("http://h/a b").getAbsolutePathProperty(), "/a b");
    EXPECT_EQ(Uri("http://h/p?a b").getQueryProperty(), "?a b");
    EXPECT_EQ(Uri("http://h/p#a b").getFragmentProperty(), "#a b");
}

TEST(UriTests, Fix2359_TheHostCharacterSetIsDotNetsAndNothingWider) {
    // Every character outside "-0-9A-Za-z." plus '_' fails, which is the whole rule rather than a
    // special case for the space the ticket named.
    for (const char* bad : {"http://ex$ample.com/", "http://ex ample.com/", "http://ex	ample.com/",
                            "http://ex(ample.com/", "http://ex,ample.com/",
                            "http://ex|ample.com/"}) {
        SCOPED_TRACE(bad);
        EXPECT_THROW((void)Uri(bad), System::UriFormatException);
    }

    // ...and everything IN the set still parses, including the historically tolerated underscore
    // that .NET's own comment calls "formally invalid but ... especially on corpnets".
    EXPECT_EQ(Uri("http://my_host-1.example.com/").getHostProperty(), "my_host-1.example.com");
    EXPECT_EQ(Uri("http://192.168.1.1/").getHostProperty(), "192.168.1.1");
    EXPECT_EQ(Uri("http://[::1]/").getHostProperty(), "[::1]")
        << "a bracketed IPv6 literal takes IPv6AddressHelper and is exempt from this rule";

    // NON-ASCII IS ACCEPTED, because .NET's IRI path takes anything outside ASCII -- EXCEPT
    // U+0080-U+009F, which s_iriInvalidChars lists (DomainNameHelper.cs:40-45). Decoding the
    // scalar rather than waving the bytes through is what makes that distinction reachable at
    // all, and it costs nothing because #2354 put the decoder in Core.Base.
    EXPECT_EQ(Uri("http://bÃ¼" "cher.example/").getHostProperty(), "bÃ¼" "cher.example");
    EXPECT_THROW((void)Uri("http://exÂ" "ample.com/"), System::UriFormatException)
        << "U+0085 is inside the excluded C1 range";
}

// ---------------------------------------------------------------------------
// Ticket #1998 (SR-AUD-147) -- IsKnownScheme rejects a malformed scheme.
//
// The ticket was blocked because "no evidence for the .NET behaviour survives in this
// environment: the audit's C# probe directory is absent and /rv/tmp/runtime/src/libraries/ is
// absent. This is the same line #1963 sits on and it is respected." That was the correct call
// then. The reference is present now, and it says exactly what SR-AUD-147 recorded.
// ---------------------------------------------------------------------------

TEST(UriParserSchemeValidationTests, Fix1998_AMalformedSchemeIsRejectedNotReportedUnknown) {
    // UriScheme.cs:186-195 -- `if (!Uri.CheckSchemeName(schemeName)) throw new
    // ArgumentOutOfRangeException(nameof(schemeName));` BEFORE the registration lookup. Before
    // this, a caller could not tell "I do not recognise this scheme" from "that is not a
    // scheme at all": both answered false.
    for (const char* bad : {"", " ", "ht tp", "1http", "-http", "http:", "ht\ttp", "http/",
                            "ht\nttp", "http?"}) {
        SCOPED_TRACE(bad);
        EXPECT_THROW((void)System::UriParser::IsKnownScheme(bad),
                     System::ArgumentOutOfRangeException);
    }
}

TEST(UriParserSchemeValidationTests, Fix1998_AWellFormedSchemeStillAnswersTrueOrFalse) {
    // The invariance row: a syntactically valid scheme still gets a yes/no answer, and the
    // registration table is untouched.
    EXPECT_TRUE(System::UriParser::IsKnownScheme("http"));
    EXPECT_TRUE(System::UriParser::IsKnownScheme("HTTP"));
    EXPECT_TRUE(System::UriParser::IsKnownScheme("net.tcp"));   // '.' is a legal scheme char
    EXPECT_TRUE(System::UriParser::IsKnownScheme("net.pipe"));
    EXPECT_FALSE(System::UriParser::IsKnownScheme("wais"));      // valid syntax, not registered
    EXPECT_FALSE(System::UriParser::IsKnownScheme("a+b-c.d1"));  // every legal punctuation
}

TEST(UriParserSchemeValidationTests, Fix1998_CheckSchemeNameIsDotNetsGrammar) {
    // Uri.cs:1485-1488 with s_schemeChars from :1477-1478 -- RFC 3986 3.1's
    // ALPHA *( ALPHA / DIGIT / "+" / "-" / "." ). Public in .NET, so public here.
    EXPECT_TRUE(System::Uri::CheckSchemeName("h"));
    EXPECT_TRUE(System::Uri::CheckSchemeName("a+b-c.d0123456789"));
    EXPECT_TRUE(System::Uri::CheckSchemeName("Z"));
    EXPECT_FALSE(System::Uri::CheckSchemeName(""));
    EXPECT_FALSE(System::Uri::CheckSchemeName("0a")) << "the first character must be a letter";
    EXPECT_FALSE(System::Uri::CheckSchemeName("+a")) << "...and '+' is not one, even though it "
                                                        "is legal later in the scheme";
    EXPECT_FALSE(System::Uri::CheckSchemeName("a b"));
    EXPECT_FALSE(System::Uri::CheckSchemeName("a_b")) << "'_' is not in s_schemeChars";
    EXPECT_FALSE(System::Uri::CheckSchemeName(std::string("ab\0c", 4)))
        << "a NUL is not a scheme character either";
}
