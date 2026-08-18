// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Ticket #2354 (2026-08-18). `IdnMapping::utf8ToCodePoints` held one of six copies of the same
// UTF-8 scalar decode -- a copy the ticket itself did not name. It is now a spelling of
// `System/detail/Utf8Scalar.hpp`'s single rule, and it is the THIRD of the three contracts over
// that rule: `Rune` reports, an `Encoding` substitutes `U+FFFD`, and this door THROWS, because a
// domain name routinely originates from untrusted input.
//
// The companion cases for the other two contracts are in
// modules/text/tests/System/Text/Utf8SharedScalarDecodeTests.cpp. They are in two files rather
// than one because `modules/text` does not depend on `Globalization`, and #2354 is not a reason
// to add a public component edge — the module graph stays at 41/92.
#include <gtest/gtest.h>
#include <string>
#include "System/ArgumentException.hpp"
#include "System/Globalization/IdnMapping.hpp"

using System::Globalization::IdnMapping;

TEST(IdnMappingUtf8DecodeTests, IllFormedUtf8ThrowsWhereTheOtherDoorsReportOrSubstitute) {
    IdnMapping mapping;
    // A 2-byte lead byte followed by an ordinary ASCII byte. Before the conformance repair this
    // produced a garbage Punycode result with no exception at all.
    EXPECT_THROW((void)mapping.GetAscii(std::string("\xC2\x41")), System::ArgumentException);
    // The overlong 2-, 3- and 4-byte encodings of U+0000. Each branch of the decode carries its
    // own overlong test, so each needs its own row.
    EXPECT_THROW((void)mapping.GetAscii(std::string("\xC0\x80")), System::ArgumentException);
    EXPECT_THROW((void)mapping.GetAscii(std::string("\xE0\x80\x80")), System::ArgumentException);
    EXPECT_THROW((void)mapping.GetAscii(std::string("\xF0\x80\x80\x80")), System::ArgumentException);
    // The 3-byte encoding of U+D800: structurally valid, but not a Unicode scalar. This is the
    // input class on which the three contracts differ, so it matters that this door still throws.
    EXPECT_THROW((void)mapping.GetAscii(std::string("\xED\xA0\x80")), System::ArgumentException);
    // The two rejections this door used to spell out by hand are subsumed exactly by the shared
    // rule: a 2-byte lead below 0xC2 is an overlong encoding, and a 4-byte lead above 0xF4
    // encodes a value above U+10FFFF. Neither is spelled out any more, so both are pinned here.
    EXPECT_THROW((void)mapping.GetAscii(std::string("\xC1\xBF")), System::ArgumentException);
    EXPECT_THROW((void)mapping.GetAscii(std::string("\xF5\x80\x80\x80")), System::ArgumentException);
}

TEST(IdnMappingUtf8DecodeTests, WellFormedNamesAreUnaffected) {
    IdnMapping mapping;
    EXPECT_EQ(mapping.GetAscii(std::string("example.com")), "example.com");
    // A multi-byte scalar still round-trips through Punycode.
    const std::string original    = "b\xC3\xBC" "cher.de";   // "bücher.de" as UTF-8 bytes
    const std::string asciiName   = mapping.GetAscii(original);
    EXPECT_NE(asciiName, original);
    EXPECT_EQ(mapping.GetUnicode(asciiName), original);
}
