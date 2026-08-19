// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <optional>
#include "System/UriFormatException.hpp"
#include "System/UriTypeConverter.hpp"

using System::UriTypeConverter;
using System::Uri;

TEST(UriTypeConverterTest, CanConvertFrom) {
    UriTypeConverter c;
    EXPECT_TRUE(c.CanConvertFrom());
}

TEST(UriTypeConverterTest, CanConvertTo) {
    UriTypeConverter c;
    EXPECT_TRUE(c.CanConvertTo());
}

TEST(UriTypeConverterTest, ConvertFromString) {
    // MIGRATED by #1999: ConvertFrom returns std::optional<Uri>, because a by-value Uri cannot
    // express .NET's null.
    UriTypeConverter c;
    auto uri = c.ConvertFrom("http://example.com");
    ASSERT_TRUE(uri.has_value());
    EXPECT_EQ(uri->getHostProperty(), "example.com");
}

TEST(UriTypeConverterTest, Fix1999_ConvertFromEmptyReturnsTheEmptyStateInsteadOfThrowing) {
    // INVERTED by #1999 / SR-AUD-148. This asserted the divergence: an empty string was forwarded
    // straight to the Uri constructor and threw. .NET short-circuits it and returns null --
    // "if (string.IsNullOrEmpty(uriString)) { return null; }" (UriTypeConverter.cs:44-47).
    UriTypeConverter c;
    std::optional<Uri> result;
    EXPECT_NO_THROW(result = c.ConvertFrom(""));
    EXPECT_FALSE(result.has_value());
}

TEST(UriTypeConverterTest, Fix1999_ARelativeUriIsAccepted) {
    // Added after mutation M4 -- "use UriKind::Absolute" -- went UNCAUGHT: every other case
    // converts an ABSOLUTE URI, so requiring Absolute never failed. A relative input is the only
    // one the kind discriminates, and .NET passes UriKind.RelativeOrAbsolute precisely so this
    // works (UriTypeConverter.cs:50).
    UriTypeConverter c;
    std::optional<Uri> relative;
    EXPECT_NO_THROW(relative = c.ConvertFrom("/path/to/resource"));
    ASSERT_TRUE(relative.has_value());
    EXPECT_FALSE(relative->getIsAbsoluteUriProperty());
    EXPECT_EQ(c.ConvertTo(*relative), "/path/to/resource")
        << "and it round-trips, which is why ConvertTo uses OriginalString";
}

TEST(UriTypeConverterTest, Decl1999_OnlyTheEmptyInputIsShortCircuited) {
    // The widening is exactly one input wide, and that is .NET's own boundary: its comment says
    // "Let the Uri constructor throw any informative exceptions", so a MALFORMED string must
    // still throw rather than return the empty state.
    UriTypeConverter c;
    EXPECT_THROW((void)c.ConvertFrom("http://exa mple.com/"), System::UriFormatException);
}

TEST(UriTypeConverterTest, ConvertToString) {
    UriTypeConverter c;
    Uri uri("http://example.com/path");
    std::string s = c.ConvertTo(uri);
    EXPECT_NE(s.find("example.com"), std::string::npos);
}

TEST(UriTypeConverterTest, RoundTrip) {
    UriTypeConverter c;
    Uri original("https://test.org/api");
    std::string s = c.ConvertTo(original);
    auto restored = c.ConvertFrom(s);
    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(restored->getHostProperty(), "test.org");
}

// ---------------------------------------------------------------------------
// Regression: real .NET's UriTypeConverter.ConvertTo returns uri.OriginalString,
// not uri.AbsoluteUri (verified against UriTypeConverter.cs) -- this matters
// because AbsoluteUri throws for a relative Uri while OriginalString never does.
// ---------------------------------------------------------------------------

TEST(UriTypeConverterTest, ConvertTo_UsesOriginalString) {
    UriTypeConverter c;
    Uri uri("http://example.com/path");
    EXPECT_EQ(c.ConvertTo(uri), uri.getOriginalStringProperty());
}
