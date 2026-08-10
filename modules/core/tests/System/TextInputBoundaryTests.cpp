// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Permanent regressions for the #2223 Core text input-boundary family.
//
//   #2224 / SR-AUD-016  String::LastIndexOf returned a match extending past the search range.
//   #2225 / SR-AUD-017  Char::Parse accepted overlong, surrogate and invalid-lead UTF-8.
//   #2226 / SR-AUD-048  Trim and ThrowIfNullOrWhiteSpace classified whitespace by C-locale BYTE.
//   #2227 / SR-AUD-028  FromBase64String accepted misplaced padding and rejected legal whitespace.
//
// Measured before the repair (build-probe/2223_probe1_before.log): 46 cases, **19 wrong**.
// After: 46 cases, **0 wrong**, with every positive control unchanged. Each suite below pairs its
// rejection cases with the positive controls that keep the repair from being over-rejection --
// the failure mode a validation fix is most likely to introduce.

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "System/ArgumentException.hpp"
#include "System/Char.hpp"
#include "System/Convert.hpp"
#include "System/FormatException.hpp"
#include "System/MemoryExtensions.hpp"
#include "System/String.hpp"

using SharpRuntime::bytecs;
using SharpRuntime::charcs;
using SharpRuntime::intcs;

// ---------------------------------------------------------------------------
// #2224 / SR-AUD-016
// ---------------------------------------------------------------------------

// The searched section is `count` positions ending at `startIndex`; a match beginning inside it
// but ending past `startIndex` is not inside it. std::string::rfind's second argument is the
// latest permitted match START, which is why the raw call was wrong.
TEST(LastIndexOfBoundedRange_2224, MatchSpillingPastTheRangeEndIsNotFound) {
    EXPECT_EQ(System::String::LastIndexOf("abcde", "de", 3, 4), -1);
}

// The audit named only the four-argument overload; the three-argument one carried the identical
// unchecked rfind and is measured failing in the before-probe.
TEST(LastIndexOfBoundedRange_2224, ThreeArgumentOverloadAlsoRejectsASpillingMatch) {
    EXPECT_EQ(System::String::LastIndexOf("abcde", "de", 3), -1);
}

TEST(LastIndexOfBoundedRange_2224, SubstringLongerThanTheRangeIsNotFound) {
    // begin == 0, so the old lower-bound-only check passed a match that overran startIndex by 3.
    EXPECT_EQ(System::String::LastIndexOf("abcde", "abcde", 2, 3), -1);
    EXPECT_EQ(System::String::LastIndexOf("abcde", "abcdef", 4, 5), -1);
}

TEST(LastIndexOfBoundedRange_2224, AMatchThatFitsExactlyIsStillFound) {
    EXPECT_EQ(System::String::LastIndexOf("abcde", "de", 4, 5), 3);
    EXPECT_EQ(System::String::LastIndexOf("abcde", "de", 4), 3);
    EXPECT_EQ(System::String::LastIndexOf("abcde", "e", 4, 1), 4);
}

TEST(LastIndexOfBoundedRange_2224, InteriorAndRepeatedMatchesAreUnchanged) {
    EXPECT_EQ(System::String::LastIndexOf("abcde", "bc", 3, 4), 1);
    // The LAST fitting match wins, which is what makes this LastIndexOf rather than IndexOf.
    EXPECT_EQ(System::String::LastIndexOf("abab", "ab", 3, 4), 2);
    EXPECT_EQ(System::String::LastIndexOf("abab", "ab", 2, 3), 0);
}

// The documented CompareInfo.LastIndexOf off-by-one fixup and the empty-value return are
// pre-existing compatibility behaviour; pin them so the range repair cannot disturb either.
TEST(LastIndexOfBoundedRange_2224, StartIndexEqualToLengthAndEmptyValueAreUnchanged) {
    EXPECT_EQ(System::String::LastIndexOf("abcde", "de", 5), 3);
    EXPECT_EQ(System::String::LastIndexOf("abcde", "de", 5, 5), 3);
    EXPECT_EQ(System::String::LastIndexOf("abcde", "", 3, 4), 3);
    EXPECT_EQ(System::String::LastIndexOf("", "x", 0), -1);
}

// A single-character match cannot overrun its own start, so these overloads were never affected.
TEST(LastIndexOfBoundedRange_2224, CharOverloadsAreUnaffected) {
    EXPECT_EQ(System::String::LastIndexOf("abcde", 'e', 4, 5), 4);
    EXPECT_EQ(System::String::LastIndexOf("abcde", 'e', 3, 4), -1);
}

// ---------------------------------------------------------------------------
// #2225 / SR-AUD-017
// ---------------------------------------------------------------------------

TEST(CharParseUtf8_2225, OverlongEncodingsAreRejected) {
    EXPECT_THROW(System::Char::Parse("\xC0\x80"), System::FormatException);      // U+0000 as 2
    EXPECT_THROW(System::Char::Parse("\xC1\xBF"), System::FormatException);      // U+007F as 2
    EXPECT_THROW(System::Char::Parse("\xE0\x80\x80"), System::FormatException);  // U+0000 as 3
    EXPECT_THROW(System::Char::Parse("\xE0\x9F\xBF"), System::FormatException);  // U+07FF as 3
}

TEST(CharParseUtf8_2225, SurrogatesAndOutOfRangeLeadBytesAreRejected) {
    EXPECT_THROW(System::Char::Parse("\xED\xA0\x80"), System::FormatException);  // U+D800
    EXPECT_THROW(System::Char::Parse("\xED\xBF\xBF"), System::FormatException);  // U+DFFF
    EXPECT_THROW(System::Char::Parse("\xF8\x80\x80\x80"), System::FormatException);
    EXPECT_THROW(System::Char::Parse("\xFF"), System::FormatException);
}

TEST(CharParseUtf8_2225, StrayContinuationAndTruncatedSequencesAreRejected) {
    EXPECT_THROW(System::Char::Parse("\x80"), System::FormatException);
    EXPECT_THROW(System::Char::Parse("\xBF"), System::FormatException);
    EXPECT_THROW(System::Char::Parse("\xC3"), System::FormatException);
    EXPECT_THROW(System::Char::Parse("\xE2\x82"), System::FormatException);
    EXPECT_THROW(System::Char::Parse("\xC3\x28"), System::FormatException);  // bad continuation
}

TEST(CharParseUtf8_2225, EveryValidSequenceStillDecodesUnchanged) {
    EXPECT_EQ(System::Char::Parse("A"), u'A');
    EXPECT_EQ(static_cast<uint32_t>(System::Char::Parse(std::string(1, '\0'))), 0x0000u);
    EXPECT_EQ(static_cast<uint32_t>(System::Char::Parse("\x7F")), 0x007Fu);
    EXPECT_EQ(static_cast<uint32_t>(System::Char::Parse("\xC2\x80")), 0x0080u);  // 2-byte minimum
    EXPECT_EQ(static_cast<uint32_t>(System::Char::Parse("\xC3\xA9")), 0x00E9u);
    EXPECT_EQ(static_cast<uint32_t>(System::Char::Parse("\xDF\xBF")), 0x07FFu);  // 2-byte maximum
    EXPECT_EQ(static_cast<uint32_t>(System::Char::Parse("\xE0\xA0\x80")), 0x0800u);  // 3-byte min
    EXPECT_EQ(static_cast<uint32_t>(System::Char::Parse("\xE2\x82\xAC")), 0x20ACu);
    EXPECT_EQ(static_cast<uint32_t>(System::Char::Parse("\xEF\xBF\xBF")), 0xFFFFu);  // BMP maximum
}

// A well-formed sequence outside the BMP keeps its own long-standing diagnostic rather than
// being lumped in with malformed input, and extra text after a valid character still reports the
// cardinality message.
TEST(CharParseUtf8_2225, WellFormedButUnrepresentableInputKeepsItsOwnDiagnostic) {
    EXPECT_THROW(System::Char::Parse("\xF0\x9F\x98\x80"), System::FormatException);  // U+1F600
    EXPECT_THROW(System::Char::Parse("ab"), System::FormatException);
    EXPECT_THROW(System::Char::Parse(""), System::FormatException);
}

TEST(CharParseUtf8_2225, TryParseMirrorsParseAndClearsItsOutput) {
    for (const char* bad : {"\xC0\x80", "\xED\xA0\x80", "\x80", "\xC3", "ab", ""}) {
        charcs out = u'X';
        EXPECT_FALSE(System::Char::TryParse(bad, out)) << bad;
        EXPECT_EQ(out, u'\0') << bad;
    }
    charcs out = u'X';
    EXPECT_TRUE(System::Char::TryParse("\xC3\xA9", out));
    EXPECT_EQ(static_cast<uint32_t>(out), 0x00E9u);
}

// ---------------------------------------------------------------------------
// #2226 / SR-AUD-048 / CCF-015
// ---------------------------------------------------------------------------

namespace {
std::string trimmed(const std::string& s) {
    auto sp = System::MemoryExtensions::Trim(
        System::ReadOnlySpan<char>(s.data(), static_cast<intcs>(s.size())));
    return std::string(sp.getPointer(), static_cast<size_t>(sp.getLengthProperty()));
}
}  // namespace

TEST(Utf8WhiteSpacePolicy_2226, TrimRemovesNonAsciiUnicodeWhitespace) {
    EXPECT_EQ(trimmed("\xC2\xA0value\xC2\xA0"), "value");              // U+00A0
    EXPECT_EQ(trimmed("\xE3\x80\x80value\xE3\x80\x80"), "value");      // U+3000
    EXPECT_EQ(trimmed("\xE2\x80\x83value"), "value");                  // U+2003
    EXPECT_EQ(trimmed("\xE2\x80\xA8value\xE2\x80\xA9"), "value");      // U+2028 / U+2029
    EXPECT_EQ(trimmed("\xC2\x85value"), "value");                      // U+0085
    EXPECT_EQ(trimmed("\xE2\x80\xAFvalue\xE2\x81\x9F"), "value");      // U+202F / U+205F
    EXPECT_EQ(trimmed("\xE1\x9A\x80value"), "value");                  // U+1680
}

TEST(Utf8WhiteSpacePolicy_2226, MixedAndRepeatedWhitespaceRunsAreRemovedWhole) {
    EXPECT_EQ(trimmed(" \xC2\xA0\t\xE3\x80\x80value\xC2\xA0 \n"), "value");
    EXPECT_EQ(trimmed("\xC2\xA0\xC2\xA0\xC2\xA0"), "");
    EXPECT_EQ(trimmed(""), "");
}

TEST(Utf8WhiteSpacePolicy_2226, AsciiBehaviourIsByteIdenticalToBefore) {
    EXPECT_EQ(trimmed("  value\t\n"), "value");
    EXPECT_EQ(trimmed("value"), "value");
    EXPECT_EQ(trimmed("   "), "");
    EXPECT_EQ(trimmed("\r\n\v\f value \v\f\r\n"), "value");
}

// The repair must not turn ordinary multibyte text into whitespace, and it must not consume a
// character whose interior bytes happen to look like something else.
TEST(Utf8WhiteSpacePolicy_2226, NonWhitespaceMultibyteTextSurvives) {
    EXPECT_EQ(trimmed("\xC3\xA9value\xC3\xA9"), "\xC3\xA9value\xC3\xA9");
    EXPECT_EQ(trimmed("\xE2\x82\xAC"), "\xE2\x82\xAC");
    EXPECT_EQ(trimmed(" \xC3\xA9 "), "\xC3\xA9");
}

// Malformed UTF-8 is not whitespace, and neither door gains an exception it never had.
TEST(Utf8WhiteSpacePolicy_2226, MalformedUtf8IsNotWhitespaceAndDoesNotThrow) {
    EXPECT_NO_THROW({
        EXPECT_EQ(trimmed("\xFFvalue\xFF"), "\xFFvalue\xFF");
        EXPECT_EQ(trimmed(" \xC0\x80 "), "\xC0\x80");
    });
}

TEST(Utf8WhiteSpacePolicy_2226, ThrowIfNullOrWhiteSpaceRejectsUnicodeWhitespaceArguments) {
    EXPECT_THROW(System::ArgumentException::ThrowIfNullOrWhiteSpace("\xC2\xA0\xC2\xA0", "p"),
                 System::ArgumentException);
    EXPECT_THROW(System::ArgumentException::ThrowIfNullOrWhiteSpace("\xE3\x80\x80", "p"),
                 System::ArgumentException);
    EXPECT_THROW(System::ArgumentException::ThrowIfNullOrWhiteSpace(" \xC2\xA0\t", "p"),
                 System::ArgumentException);
    // Pre-existing behaviour, unchanged.
    EXPECT_THROW(System::ArgumentException::ThrowIfNullOrWhiteSpace("", "p"),
                 System::ArgumentException);
    EXPECT_THROW(System::ArgumentException::ThrowIfNullOrWhiteSpace("  \t", "p"),
                 System::ArgumentException);
}

TEST(Utf8WhiteSpacePolicy_2226, ThrowIfNullOrWhiteSpaceStillAcceptsRealValues) {
    EXPECT_NO_THROW(System::ArgumentException::ThrowIfNullOrWhiteSpace("\xC2\xA0x", "p"));
    EXPECT_NO_THROW(System::ArgumentException::ThrowIfNullOrWhiteSpace("x", "p"));
    EXPECT_NO_THROW(System::ArgumentException::ThrowIfNullOrWhiteSpace("\xC3\xA9", "p"));
    EXPECT_NO_THROW(System::ArgumentException::ThrowIfNullOrWhiteSpace("\xFF", "p"));
}

// ---------------------------------------------------------------------------
// #2227 / SR-AUD-028
// ---------------------------------------------------------------------------

namespace {
std::string decoded(const std::string& s) {
    std::string out;
    for (auto b : System::Convert::FromBase64String(s)) out += static_cast<char>(b);
    return out;
}
}  // namespace

TEST(Base64Grammar_2227, PaddingOutsideTheFinalPositionsIsRejected) {
    EXPECT_THROW(decoded("=AAA"), System::FormatException);
    EXPECT_THROW(decoded("A=AA"), System::FormatException);
    EXPECT_THROW(decoded("AA=A"), System::FormatException);
    EXPECT_THROW(decoded("AA==AAAA"), System::FormatException);
    EXPECT_THROW(decoded("A==A"), System::FormatException);
}

TEST(Base64Grammar_2227, ExcessPaddingIsRejected) {
    EXPECT_THROW(decoded("===="), System::FormatException);
    EXPECT_THROW(decoded("A==="), System::FormatException);
    EXPECT_THROW(decoded("AAAA===="), System::FormatException);
}

TEST(Base64Grammar_2227, PermittedWhitespaceIsIgnoredBeforeTheLengthCheck) {
    EXPECT_EQ(decoded("T Q=="), "M");
    EXPECT_EQ(decoded("TQ\n=="), "M");
    EXPECT_EQ(decoded("T\tQ\r\n=="), "M");
    EXPECT_EQ(decoded("  TWFu  "), "Man");
    EXPECT_EQ(decoded("T W\tF\nu"), "Man");
    EXPECT_EQ(decoded("   "), "");
}

TEST(Base64Grammar_2227, ValidInputIsUnchanged) {
    EXPECT_EQ(decoded("TQ=="), "M");
    EXPECT_EQ(decoded("TWE="), "Ma");
    EXPECT_EQ(decoded("TWFu"), "Man");
    EXPECT_EQ(decoded(""), "");
    EXPECT_EQ(decoded("TWFuTQ=="), "ManM");
}

TEST(Base64Grammar_2227, RoundTripsForEveryRemainderShape) {
    for (size_t n = 0; n <= 16; ++n) {
        std::vector<bytecs> payload;
        for (size_t i = 0; i < n; ++i) payload.push_back(static_cast<bytecs>('a' + (i % 26)));
        const std::string encoded = System::Convert::ToBase64String(payload);
        const auto back = System::Convert::FromBase64String(encoded);
        ASSERT_EQ(back.size(), payload.size()) << "n=" << n;
        EXPECT_EQ(back, payload) << "n=" << n;
    }
}

TEST(Base64Grammar_2227, InvalidCharactersAndLengthsAreStillRejected) {
    EXPECT_THROW(decoded("T*=="), System::FormatException);
    EXPECT_THROW(decoded("TQ="), System::FormatException);
    EXPECT_THROW(decoded("TQ"), System::FormatException);
    EXPECT_THROW(decoded("T Q="), System::FormatException);  // whitespace does not pad the length
}

// The audit also asked for "unused-bit rules". Real .NET is LENIENT there -- it discards the
// unused bits rather than rejecting -- and /rv is absent here to confirm the exact rule, so
// enforcing it would risk rejecting input .NET accepts. The current lenient behaviour is pinned
// instead, and tightening it stays a separate, evidence-gated question. If this test ever starts
// failing, that is the signal the decision was revisited, not a silent drift.
TEST(Base64Grammar_2227, UnusedTrailingBitsRemainLenientAndPinned) {
    EXPECT_NO_THROW({
        const auto a = System::Convert::FromBase64String("AB==");
        ASSERT_EQ(a.size(), 1u);
        EXPECT_EQ(static_cast<int>(a[0]), 0);
        const auto b = System::Convert::FromBase64String("AAB=");
        ASSERT_EQ(b.size(), 2u);
        EXPECT_EQ(static_cast<int>(b[0]), 0);
        EXPECT_EQ(static_cast<int>(b[1]), 0);
    });
}
