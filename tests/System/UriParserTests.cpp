// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/UriParser.hpp"
#include "System/Uri.hpp"
#include "System/NotImplementedException.hpp"

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

TEST(UriParserTest, IsKnownSchemeIsCaseInsensitive) {
    // URI schemes are case-insensitive per RFC 3986; matches this class's own doc comment.
    EXPECT_TRUE(UriParser::IsKnownScheme("HTTP"));
    EXPECT_TRUE(UriParser::IsKnownScheme("Https"));
}

namespace {
class TestParser final : public UriParser {
public:
    bool IsBaseOf(const System::Uri& /*b*/, const System::Uri& /*r*/) override { return true; }
};
}

TEST(UriParserTest, SubclassIsBaseOf) {
    TestParser p;
    System::Uri base("http://example.com");
    System::Uri rel("http://example.com/path");
    EXPECT_TRUE(p.IsBaseOf(base, rel));
}

TEST(UriParserTest, GetComponents_DefaultThrowsNotImplementedException) {
    TestParser p;
    System::Uri u("http://example.com");
    EXPECT_THROW(p.GetComponents(u, System::UriComponents::Scheme, System::UriFormat::Unescaped),
                 NotImplementedException);
}
