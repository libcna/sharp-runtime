// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/UriParser.hpp"
#include "System/Uri.hpp"

using System::UriParser;

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

TEST(UriParserTest, IsKnownSchemeCaseSensitive) {
    EXPECT_FALSE(UriParser::IsKnownScheme("HTTP"));
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
