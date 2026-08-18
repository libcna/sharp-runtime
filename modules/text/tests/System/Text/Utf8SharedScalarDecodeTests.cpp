// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Ticket #2354 (2026-08-18). Six copies of one UTF-8 scalar decode existed across four modules;
// #2014 factored the rule into System/detail/Utf8Scalar.hpp and moved two, and this ticket moved
// the remaining four. These tests exist because collapsing copies is only safe if the property
// that DISTINGUISHES them is pinned, and it was not.
//
// The distinguishing property is one input class: a STRUCTURALLY VALID encoding of a value that
// is not a Unicode scalar -- a surrogate, or a value above U+10FFFF. Three doors want three
// different things there, and a fourth wants a fifth thing on all ill-formed input:
//
//   Rune::TryGetRuneAt      report failure, consuming the SEQUENCE'S OWN LENGTH, so a caller
//                           can skip the whole thing
//   any Encoding            substitute U+FFFD over ONE BYTE, so it emits one replacement per
//                           byte, which is what .NET's replacement DecoderFallback does
//   IdnMapping              throw, because a domain name comes from untrusted input
//                           (pinned in modules/globalization, which modules/text does not depend on)
//   UTF8Encoding            report a validity length of 0, because well-formed bytes pass
//                           through unchanged and there is no re-encoding step to fold a
//                           substitution into
//
// Measured before this file existed: Rune::TryGetRuneAt had NO test anywhere in the repository,
// and a mutation collapsing its length onto the Encoding's was not caught by any of the 17,296
// tests then present. That is the gap these cases close.
#include <gtest/gtest.h>
#include <string>
#include "System/Text/Encoding.hpp"
#include "System/Text/Rune.hpp"
#include "System/Text/UnicodeEncoding.hpp"
#include "System/Text/UTF8Encoding.hpp"

using System::Text::Rune;

namespace {
// The 3-byte encoding of U+D800. Every continuation byte is well formed and the length is
// exactly what the lead byte announces; only the resulting value is not a scalar.
constexpr const char* kSurrogateEncoding = "\xED\xA0\x80";
// A 2-byte lead byte followed by an ordinary ASCII byte. Structurally broken at byte 2.
constexpr const char* kBadContinuation = "\xC2\x41";
// The overlong 2-byte encoding of U+0000.
constexpr const char* kOverlongNul = "\xC0\x80";
// The overlong 3- and 4-byte encodings of U+0000. Each branch of the decode carries its own
// overlong test, so each needs its own row: a mutation removing only the 3-byte one is invisible
// to the 2-byte case.
constexpr const char* kOverlongNul3 = "\xE0\x80\x80";
constexpr const char* kOverlongNul4 = "\xF0\x80\x80\x80";
} // namespace

TEST(Utf8SharedScalarDecodeTests, RuneConsumesTheWholeSequenceForANonScalarButOneByteForABreak) {
    Rune        rune(static_cast<std::uint32_t>('x'));
    std::size_t consumed = 0;

    // Structurally valid, not a scalar: the caller is told the sequence is three bytes wide so
    // it can step over the whole thing. This is the assertion a mutation collapsing the two
    // contracts breaks, and nothing else in the repository makes it.
    const std::string surrogate(kSurrogateEncoding);
    EXPECT_FALSE(Rune::TryGetRuneAt(surrogate, 0, rune, consumed));
    EXPECT_EQ(consumed, 3u);

    // Structurally broken: one byte, so the caller resumes at the next byte and cannot loop.
    const std::string broken(kBadContinuation);
    EXPECT_FALSE(Rune::TryGetRuneAt(broken, 0, rune, consumed));
    EXPECT_EQ(consumed, 1u);

    for (const char* text : {kOverlongNul, kOverlongNul3, kOverlongNul4}) {
        const std::string overlong(text);
        EXPECT_FALSE(Rune::TryGetRuneAt(overlong, 0, rune, consumed)) << overlong.size();
        EXPECT_EQ(consumed, 1u) << overlong.size();
    }

    // Past the end is its own answer, and it is neither of the above.
    EXPECT_FALSE(Rune::TryGetRuneAt(std::string("a"), 1, rune, consumed));
    EXPECT_EQ(consumed, 0u);

    // A well-formed scalar still decodes, at its own width.
    const std::string smiley("\xF0\x9F\x98\x80");   // U+1F600
    ASSERT_TRUE(Rune::TryGetRuneAt(smiley, 0, rune, consumed));
    EXPECT_EQ(consumed, 4u);
    EXPECT_EQ(rune.getValueProperty(), 0x1F600u);
}

TEST(Utf8SharedScalarDecodeTests, AnEncodingSubstitutesOneReplacementPerByteForBothKinds) {
    // The counterpart of the case above: where Rune skips three bytes, an Encoding emits three
    // replacement characters, because .NET's replacement DecoderFallback is per byte.
    const auto utf16 = System::Text::Encoding::Unicode();

    const std::string surrogate(kSurrogateEncoding);
    const auto        encoded = utf16->GetBytes(surrogate);
    ASSERT_EQ(encoded.size(), 6u) << "three U+FFFD, two bytes each";
    for (std::size_t unit = 0; unit < 3; ++unit) {
        EXPECT_EQ(static_cast<unsigned char>(encoded[unit * 2]), 0xFDu);
        EXPECT_EQ(static_cast<unsigned char>(encoded[unit * 2 + 1]), 0xFFu);
    }

    const std::string broken(kBadContinuation);
    // "\xC2" is ill-formed and becomes one U+FFFD; 'A' is a scalar and survives.
    const auto brokenEncoded = utf16->GetBytes(broken);
    ASSERT_EQ(brokenEncoded.size(), 4u);
    EXPECT_EQ(static_cast<unsigned char>(brokenEncoded[0]), 0xFDu);
    EXPECT_EQ(static_cast<unsigned char>(brokenEncoded[2]), static_cast<unsigned char>('A'));
}

TEST(Utf8SharedScalarDecodeTests, Utf8EncodingReplacesIllFormedBytesRatherThanPassingThemThrough) {
    // UTF8Encoding is the door that wants a validity LENGTH rather than a code point: its
    // well-formed bytes pass through unchanged, so only the ill-formed ones are rewritten.
    const auto utf8 = System::Text::Encoding::UTF8();

    const auto roundTripped = utf8->GetString(utf8->GetBytes(std::string("h\xC3\xA9llo")));
    EXPECT_EQ(roundTripped, std::string("h\xC3\xA9llo")) << "well-formed bytes are untouched";

    for (const char* illFormed :
         {kSurrogateEncoding, kBadContinuation, kOverlongNul, kOverlongNul3, kOverlongNul4}) {
        const std::string text(illFormed);
        const auto        bytes = utf8->GetBytes(text);
        EXPECT_NE(std::string(bytes.begin(), bytes.end()), text) << "ill-formed bytes must not pass through";
    }
}
