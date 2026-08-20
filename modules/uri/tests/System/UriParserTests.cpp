// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/UriParser.hpp"
#include "System/Uri.hpp"
#include "System/NotImplementedException.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/InvalidOperationException.hpp"
#include <memory>


using System::UriParser;
using System::NotImplementedException;

TEST(UriParserTest, IsKnownSchemeHttp) {
    EXPECT_TRUE(UriParser::IsKnownScheme("http"));
}

TEST(UriParserTest, IsKnownSchemeHttps) {
    EXPECT_TRUE(UriParser::IsKnownScheme("https"));
}

TEST(UriParserTest, IsKnownSchemeFtp) {
    EXPECT_TRUE(UriParser::IsKnownScheme("ftp"));
}

TEST(UriParserTest, IsKnownSchemeFile) {
    EXPECT_TRUE(UriParser::IsKnownScheme("file"));
}

TEST(UriParserTest, IsKnownSchemeUnknown) {
    EXPECT_FALSE(UriParser::IsKnownScheme("custom-scheme"));
}

// ---------------------------------------------------------------------------
// Regression: the known-scheme table previously listed "wais" (not one of
// .NET's actual registered UriParser schemes) and was missing "ws"/"wss"
// (WebSocket), "uuid", and "vsmacros" -- fixed to match UriSyntax.cs exactly.
// ---------------------------------------------------------------------------

TEST(UriParserTest, IsKnownSchemeWs) {
    EXPECT_TRUE(UriParser::IsKnownScheme("ws"));
}

TEST(UriParserTest, IsKnownSchemeWss) {
    EXPECT_TRUE(UriParser::IsKnownScheme("wss"));
}

TEST(UriParserTest, IsKnownSchemeUuid) {
    EXPECT_TRUE(UriParser::IsKnownScheme("uuid"));
}

TEST(UriParserTest, IsKnownSchemeVsmacros) {
    EXPECT_TRUE(UriParser::IsKnownScheme("vsmacros"));
}

TEST(UriParserTest, IsKnownSchemeWaisIsNotActuallyRegistered) {
    // "wais" is a historical RFC 1738 scheme, NOT one of .NET's registered UriParser schemes.
    EXPECT_FALSE(UriParser::IsKnownScheme("wais"));
}

TEST(UriParserTest, IsKnownSchemeMailtoGopherNewsNntpTelnetLdapNetTcpNetPipe) {
    EXPECT_TRUE(UriParser::IsKnownScheme("mailto"));
    EXPECT_TRUE(UriParser::IsKnownScheme("gopher"));
    EXPECT_TRUE(UriParser::IsKnownScheme("news"));
    EXPECT_TRUE(UriParser::IsKnownScheme("nntp"));
    EXPECT_TRUE(UriParser::IsKnownScheme("telnet"));
    EXPECT_TRUE(UriParser::IsKnownScheme("ldap"));
    EXPECT_TRUE(UriParser::IsKnownScheme("net.tcp"));
    EXPECT_TRUE(UriParser::IsKnownScheme("net.pipe"));
}

TEST(UriParserTest, IsKnownSchemeIsCaseInsensitive) {
    // URI schemes are case-insensitive per RFC 3986; matches this class's own doc comment.
    EXPECT_TRUE(UriParser::IsKnownScheme("HTTP"));
    EXPECT_TRUE(UriParser::IsKnownScheme("Https"));
}

namespace {
// #1997 A-4 migration. The hooks became `protected`, matching .NET, so a test can no longer call
// them on an object it holds. THE MIGRATION IS THE REFERENCE'S OWN SHAPE rather than a workaround:
// .NET publishes `internal` forwarders (`InternalGetComponents`, `InternalIsBaseOf`, ...) for
// exactly this reason, and its comment says so -- "simple internal wrappers that will call
// protected virtual methods (to avoid `protected internal` signatures in the public docs)"
// (`UriSyntax.cs:245-246`). A subclass that wants its own hook reachable publishes a forwarder.
class TestParser final : public UriParser {
public:
    bool IsBaseOf(const System::Uri& /*b*/, const System::Uri& /*r*/) override { return true; }

    bool CallIsBaseOf(const System::Uri& b, const System::Uri& r) { return IsBaseOf(b, r); }
    std::string CallGetComponents(const System::Uri& u, System::UriComponents c,
                                  System::UriFormat f) {
        return GetComponents(u, c, f);
    }
};
}

TEST(UriParserTest, SubclassIsBaseOf) {
    TestParser p;
    System::Uri base("http://example.com");
    System::Uri rel("http://example.com/path");
    EXPECT_TRUE(p.CallIsBaseOf(base, rel));
}

TEST(UriParserTest, GetComponents_DefaultThrowsNotImplementedException) {
    TestParser p;
    System::Uri u("http://example.com");
    EXPECT_THROW(p.CallGetComponents(u, System::UriComponents::Scheme,
                                     System::UriFormat::Unescaped),
                 NotImplementedException);
}

// ===========================================================================
// #1997 group A-4 (SR-AUD-146) -- Register, the protected hooks, and the layout
// ===========================================================================

namespace {

// A parser that records what its OnRegister callback was handed, and what it could see of
// ITSELF at the time -- which is the detail .NET's ordering makes observable.
class RecordingParser final : public System::UriParser {
public:
    bool        registered = false;
    std::string sawScheme;
    SharpRuntime::intcs sawPort = 0;
    std::string sawOwnSchemeDuringCallback = "<not called>";

protected:
    void OnRegister(const std::string& schemeName, SharpRuntime::intcs defaultPort) override {
        registered = true;
        sawScheme = schemeName;
        sawPort = defaultPort;
        sawOwnSchemeDuringCallback = getSchemeNameProperty();
    }
};

class PlainParser final : public System::UriParser {};

} // namespace

// SA-15.3's layout condition. The two members are what let a parser remember its own
// registration, and they are private, so this asserts the SHAPE a consumer must rebuild for.
TEST(UriParserA4Tests, LayoutIsPinnedBecauseTheRegistrationStateIsNew) {
    EXPECT_EQ(sizeof(System::UriParser), 48u);
    EXPECT_EQ(alignof(System::UriParser), 8u);
    // A vptr plus a std::string plus an intcs, with the relationship stated rather than only the
    // literal -- so a member added later cannot hide behind a hand-updated number.
    struct Shadow { void* vptr; std::string s; SharpRuntime::intcs p; };
    EXPECT_EQ(sizeof(System::UriParser), sizeof(Shadow));
}

TEST(UriParserA4Tests, RegisterMakesTheSchemeKnownWhichIsTheWholePoint) {
    // Before this, `Register` did not exist at all. A version that validated its arguments and
    // stored into a table nothing reads would be accepted-and-ignored -- the SR-AUD-168 defect --
    // so the property worth asserting is the LINKAGE, not the call succeeding.
    EXPECT_FALSE(System::UriParser::IsKnownScheme("a4custom"));
    auto parser = std::make_shared<RecordingParser>();
    System::UriParser::Register(parser, "a4custom", 1234);
    EXPECT_TRUE(System::UriParser::IsKnownScheme("a4custom"));
    // and case-insensitively, because the scheme is lower-cased on the way in.
    EXPECT_TRUE(System::UriParser::IsKnownScheme("A4CUSTOM"));
}

TEST(UriParserA4Tests, OnRegisterRunsBeforeTheSchemeIsStored) {
    auto parser = std::make_shared<RecordingParser>();
    System::UriParser::Register(parser, "A4Callback", 99);

    ASSERT_TRUE(parser->registered);
    // The scheme reaches the callback ALREADY LOWER-CASED, which is .NET's `lwrCaseSchemeName`.
    EXPECT_EQ(parser->sawScheme, "a4callback");
    EXPECT_EQ(parser->sawPort, 99);
    // THE ORDERING ROW. .NET assigns `syntax._scheme` on the line AFTER the OnRegister call
    // (UriSyntax.cs:175-176), so during the callback the parser does not yet know its own scheme
    // -- which is exactly why the scheme is a parameter rather than a property read. A body that
    // stored first would pass every other assertion here.
    EXPECT_EQ(parser->sawOwnSchemeDuringCallback, "");
}

TEST(UriParserA4Tests, RegisterRejectsInDotNetsOrderAndForDotNetsReasons) {
    auto parser = std::make_shared<PlainParser>();

    EXPECT_THROW(System::UriParser::Register(nullptr, "a4null", 80),
                 System::ArgumentNullException);

    // A ONE-CHARACTER SCHEME IS REFUSED THOUGH IT IS A VALID SCHEME NAME -- .NET checks the
    // length separately from `Uri.CheckSchemeName`, and the two rules disagree on exactly this
    // input. Both halves are asserted together so the asymmetry is visible rather than looking
    // like an accident of the grammar.
    EXPECT_TRUE(System::Uri::CheckSchemeName("a"));
    EXPECT_THROW(System::UriParser::Register(parser, "a", 80),
                 System::ArgumentOutOfRangeException);

    EXPECT_THROW(System::UriParser::Register(parser, "1bad", 80),
                 System::ArgumentOutOfRangeException);

    // The port range is 0..65535 plus the single sentinel -1. .NET writes the test as a cast to
    // uint, under which every OTHER negative value is a very large number -- so -2 must be
    // refused, which a naive `port > 0xFFFF` would accept.
    EXPECT_THROW(System::UriParser::Register(parser, "a4port", 65536),
                 System::ArgumentOutOfRangeException);
    EXPECT_THROW(System::UriParser::Register(parser, "a4port", -2),
                 System::ArgumentOutOfRangeException);

    // ...and -1 is accepted, which is what shows the sentinel is carved out rather than the
    // range simply starting at -1.
    EXPECT_NO_THROW(System::UriParser::Register(parser, "a4noport", -1));
}

TEST(UriParserA4Tests, TheTwoInvalidOperationCasesAreTwoDifferentQuestions) {
    auto first  = std::make_shared<PlainParser>();
    auto second = std::make_shared<PlainParser>();
    System::UriParser::Register(first, "a4dup", 80);

    // (1) THIS PARSER is already registered -- a fresh one is needed.
    EXPECT_THROW(System::UriParser::Register(first, "a4other", 80),
                 System::InvalidOperationException);

    // (2) THIS SCHEME is already taken -- the parser is fine, the name is not.
    EXPECT_THROW(System::UriParser::Register(second, "a4dup", 80),
                 System::InvalidOperationException);

    // The messages differ, which is what makes them two answers rather than one. Collapsing them
    // would leave a caller unable to tell which of the two mistakes they made.
    std::string m1, m2;
    try { System::UriParser::Register(first, "a4other", 80); }
    catch (const System::InvalidOperationException& e) { m1 = e.what(); }
    try { System::UriParser::Register(second, "a4dup", 80); }
    catch (const System::InvalidOperationException& e) { m2 = e.what(); }
    EXPECT_NE(m1, m2);
    EXPECT_NE(m1.find("already registered"), std::string::npos);
    EXPECT_NE(m2.find("already has a registered custom parser"), std::string::npos);

    // And the failed second registration left the SCHEME's owner alone -- the guard runs before
    // anything is written, so a rejected call is not a partial one.
    EXPECT_TRUE(System::UriParser::IsKnownScheme("a4dup"));
    EXPECT_FALSE(System::UriParser::IsKnownScheme("a4other"));
}

// A BUILT-IN SCHEME CANNOT BE TAKEN OVER. This row exists because splitting .NET's single table
// into a built-in LIST and a custom MAP -- which this port must do, having no parser objects for
// the built-ins -- makes it possible to check only one of them. A FIRST CUT OF THIS TICKET DID
// EXACTLY THAT and would have let a caller claim `gopher`; the mistake was mine and is recorded
// rather than quietly fixed, because the split is a permanent feature of this port's shape and
// the same mistake is available to every later change.
TEST(UriParserA4Tests, RegisteringOverABuiltInSchemeIsRefused) {
    auto parser = std::make_shared<PlainParser>();
    EXPECT_TRUE(System::UriParser::IsKnownScheme("gopher"));
    EXPECT_THROW(System::UriParser::Register(parser, "gopher", 70),
                 System::InvalidOperationException);
    // The refusal must not consume the parser either: it is still fresh and can be registered
    // somewhere else. A guard that stamped the scheme before throwing would fail this.
    EXPECT_NO_THROW(System::UriParser::Register(parser, "a4afterrefusal", 70));
}
