// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Ticket #2397 -- System::Text::RegularExpressions parity, derived from
// /rv/tmp/runtime/src/libraries/System.Text.RegularExpressions/src/System/Text/RegularExpressions/.
//
// Four divergences were measured against the reference and closed:
//   D1  Regex::Escape used its own metacharacter set and its own spelling for whitespace.
//   D2  Regex::Split discarded every matched capture group's value -- silent data loss.
//   D3  Regex::Split dropped a trailing empty segment -- silent data loss.
//   D4  Match::getIndexProperty() answered -1 for an unsuccessful match, a value .NET never
//       produces.
//
// What these cases are for, beyond the four rows: each repair has a plausible half-repair that
// passes the obvious test, and the pins below are chosen so that each half-repair fails.
#include <gtest/gtest.h>
#include <string>
#include <vector>
#include "System/Text/RegularExpressions/Match.hpp"
#include "System/Text/RegularExpressions/Regex.hpp"

using System::Text::RegularExpressions::Match;
using System::Text::RegularExpressions::Regex;

// ===========================================================================
// D1 -- Regex::Escape
// ===========================================================================

// RegexParser.cs:2135-2136 -- SearchValues.Create("\t\n\f\r #$()*+.?[\\^{|").
//
// The set is asserted as a SET, by sweeping every ASCII byte, rather than by spot-checking the
// characters that happen to be interesting. A spot check cannot see an *extra* escape, and the
// two characters this port used to escape beyond .NET's set (']' and '}') are exactly that shape.
TEST(Regex2397EscapeTests, TheEscapedSetIsExactlyDotNets) {
    const std::string dotnetMetachars = "\t\n\f\r #$()*+.?[\\^{|";
    std::string escaped;
    for (int i = 1; i < 128; ++i) {
        const std::string one(1, static_cast<char>(i));
        if (Regex::Escape(one) != one) escaped += static_cast<char>(i);
    }
    ASSERT_EQ(escaped.size(), dotnetMetachars.size());
    for (char c : dotnetMetachars)
        EXPECT_NE(escaped.find(c), std::string::npos)
            << "byte 0x" << std::hex << static_cast<int>(static_cast<unsigned char>(c))
            << " is a .NET metacharacter and is not escaped here";
    for (char c : escaped)
        EXPECT_NE(dotnetMetachars.find(c), std::string::npos)
            << "byte 0x" << std::hex << static_cast<int>(static_cast<unsigned char>(c))
            << " is escaped here and is not a .NET metacharacter";
}

// RegexParser.cs:182-196 -- '\n' is emitted as the LETTER 'n', not as a backslash followed by the
// raw control byte. A repair that added the four whitespace characters to the set without also
// transcribing this switch would satisfy the set test above and still emit different text.
TEST(Regex2397EscapeTests, WhitespaceIsSpelledWithALetterNotTheRawByte) {
    EXPECT_EQ(Regex::Escape("a\nb"), "a\\nb");
    EXPECT_EQ(Regex::Escape("a\rb"), "a\\rb");
    EXPECT_EQ(Regex::Escape("a\tb"), "a\\tb");
    EXPECT_EQ(Regex::Escape("a\fb"), "a\\fb");
    // Each is two characters -- a backslash and a letter -- never one escaped control byte.
    EXPECT_EQ(Regex::Escape("\n").size(), 2u);
    EXPECT_EQ(Regex::Escape("\n")[1], 'n');
}

// A space and '#' are escaped because .NET's IgnorePatternWhitespace mode gives both a meaning.
// A vertical tab is NOT in .NET's set, so it stays raw -- the near-miss that a "escape all
// whitespace" reading of the doc comment ("spaces", RegexParser.cs:150) would get wrong.
TEST(Regex2397EscapeTests, SpaceAndHashAreEscapedAndVerticalTabIsNot) {
    EXPECT_EQ(Regex::Escape("a b"), "a\\ b");
    EXPECT_EQ(Regex::Escape("a#b"), "a\\#b");
    EXPECT_EQ(Regex::Escape("a\vb"), "a\vb");
}

// ']' and '}' are left bare, because neither can open a construct. This is the direction that
// could break something, so it is asserted with the composites where a bare closer follows an
// escaped opener.
TEST(Regex2397EscapeTests, ClosingBracketAndBraceAreLeftBare) {
    EXPECT_EQ(Regex::Escape("a]b"), "a]b");
    EXPECT_EQ(Regex::Escape("a}b"), "a}b");
    EXPECT_EQ(Regex::Escape("a{2}"), "a\\{2}");
    EXPECT_EQ(Regex::Escape("[a]"), "\\[a]");
}

// The property Escape exists for: its output, used as a pattern, matches the original literal
// and nothing else. Swept over every ASCII byte, because a set change is exactly the change that
// could make some byte's escaped form stop compiling or stop meaning itself.
TEST(Regex2397EscapeTests, EveryEscapedByteRoundTripsAsALiteralPattern) {
    for (int i = 1; i < 128; ++i) {
        const std::string literal(1, static_cast<char>(i));
        const std::string pattern = Regex::Escape(literal);
        ASSERT_NO_THROW({
            Regex re(pattern);
            EXPECT_TRUE(re.IsMatch(literal))
                << "byte 0x" << std::hex << i << " does not match its own escaped pattern";
        }) << "byte 0x" << std::hex << i << " produced a pattern this engine rejects";
    }
}

// ===========================================================================
// D2/D3 -- Regex::Split
// ===========================================================================

// Regex.Split.cs:304-311 -- every MATCHED capture group is appended after the segment that
// preceded its match. This is the row the old sregex_token_iterator(-1) implementation could not
// produce at all: it yields only the non-matching segments.
TEST(Regex2397SplitTests, MatchedCaptureGroupsAreIncluded) {
    const std::vector<std::string> parts = Regex::Split("a1b2c", "(\\d)");
    ASSERT_EQ(parts.size(), 5u);
    EXPECT_EQ(parts[0], "a");
    EXPECT_EQ(parts[1], "1");
    EXPECT_EQ(parts[2], "b");
    EXPECT_EQ(parts[3], "2");
    EXPECT_EQ(parts[4], "c");
}

// The same pattern WITHOUT a capturing group must not gain elements -- which is what separates
// "append the matched groups" from "append the whole match".
TEST(Regex2397SplitTests, ANonCapturingPatternGainsNothing) {
    const std::vector<std::string> parts = Regex::Split("a1b2c", "\\d");
    ASSERT_EQ(parts.size(), 3u);
    EXPECT_EQ(parts[0], "a");
    EXPECT_EQ(parts[1], "b");
    EXPECT_EQ(parts[2], "c");
}

// Regex.Split.cs:306 -- `if (match.IsMatched(i))`. A group that did not participate is SKIPPED,
// not appended as an empty string. Without this the alternation below would yield an extra
// empty element per match, and every ordinary single-group case would still pass.
TEST(Regex2397SplitTests, AGroupThatDidNotParticipateIsSkippedNotEmpty) {
    const std::vector<std::string> parts = Regex::Split("a,b", "(,)|(;)");
    ASSERT_EQ(parts.size(), 3u);
    EXPECT_EQ(parts[0], "a");
    EXPECT_EQ(parts[1], ",");
    EXPECT_EQ(parts[2], "b");

    const std::vector<std::string> optional = Regex::Split("aXbXc", "(X)(?:(Y))?");
    ASSERT_EQ(optional.size(), 5u);
    EXPECT_EQ(optional[1], "X");
    EXPECT_EQ(optional[3], "X");
}

// Regex.Split.cs:321 -- the trailing segment is appended unconditionally once any match was seen.
// std::sregex_token_iterator suppresses a trailing empty token, so this element used to vanish.
TEST(Regex2397SplitTests, ATrailingEmptySegmentIsKept) {
    const std::vector<std::string> parts = Regex::Split("abc", "c");
    ASSERT_EQ(parts.size(), 2u);
    EXPECT_EQ(parts[0], "ab");
    EXPECT_EQ(parts[1], "");
}

// The leading half of the same rule, which the old implementation already got right -- asserted
// so that a repair cannot buy the trailing element by losing the leading one.
TEST(Regex2397SplitTests, ALeadingEmptySegmentIsKept) {
    const std::vector<std::string> parts = Regex::Split("abc", "a");
    ASSERT_EQ(parts.size(), 2u);
    EXPECT_EQ(parts[0], "");
    EXPECT_EQ(parts[1], "bc");

    const std::vector<std::string> both = Regex::Split("aa", "a");
    ASSERT_EQ(both.size(), 3u);
    EXPECT_EQ(both[0], "");
    EXPECT_EQ(both[1], "");
    EXPECT_EQ(both[2], "");
}

// Regex.Split.cs:316-319 -- no match at all returns the whole input as ONE element, which is not
// the same statement as "append the trailing segment". A repair that only ever appended the tail
// would return {""} rather than {input} for a non-matching pattern over an empty string.
TEST(Regex2397SplitTests, NoMatchReturnsTheWholeInputAsOneElement) {
    const std::vector<std::string> parts = Regex::Split("abc", "z");
    ASSERT_EQ(parts.size(), 1u);
    EXPECT_EQ(parts[0], "abc");

    const std::vector<std::string> empty = Regex::Split("", "z");
    ASSERT_EQ(empty.size(), 1u);
    EXPECT_EQ(empty[0], "");
}

// A zero-length match still advances and still contributes a segment, so the element count is
// driven by the match positions rather than by the matched text.
TEST(Regex2397SplitTests, ZeroLengthMatchesSplitBetweenEveryCharacter) {
    const std::vector<std::string> parts = Regex::Split("abc", "");
    ASSERT_EQ(parts.size(), 5u);
    EXPECT_EQ(parts[0], "");
    EXPECT_EQ(parts[1], "a");
    EXPECT_EQ(parts[2], "b");
    EXPECT_EQ(parts[3], "c");
    EXPECT_EQ(parts[4], "");
}

// ===========================================================================
// D4 -- Match::Index for an unsuccessful match
// ===========================================================================

// Match.cs:75 -> Group.cs:27-28 (capcount == 0) -> Capture.cs:27-32. Both routes to an
// unsuccessful Match are asserted, because Match::Empty() and a genuinely failed search are two
// different constructions and only one of them was pinned before.
TEST(Regex2397MatchTests, AnUnsuccessfulMatchReportsIndexZeroNotMinusOne) {
    Regex re("\\d+");
    const Match failed = re.Match("no digits here");
    ASSERT_FALSE(failed.getSuccessProperty());
    EXPECT_EQ(failed.getIndexProperty(), 0);
    EXPECT_EQ(failed.getLengthProperty(), 0);
    EXPECT_EQ(failed.getValueProperty(), "");

    const Match& empty = Match::Empty();
    ASSERT_FALSE(empty.getSuccessProperty());
    EXPECT_EQ(empty.getIndexProperty(), 0);
}

// Match.cs:72-74 says in terms that Index must not be used to decide success. With the sentinel
// gone that is no longer merely advice, so the property that replaces it is pinned: a successful
// match at position 0 and a failed match now report the SAME index, and only Success separates
// them.
TEST(Regex2397MatchTests, IndexNoLongerDiscriminatesSuccess) {
    Regex re("\\d+");
    const Match atZero = re.Match("42abc");
    const Match failed = re.Match("abc");
    ASSERT_TRUE(atZero.getSuccessProperty());
    ASSERT_FALSE(failed.getSuccessProperty());
    EXPECT_EQ(atZero.getIndexProperty(), failed.getIndexProperty());
    EXPECT_NE(atZero.getSuccessProperty(), failed.getSuccessProperty());
}

// The end of a NextMatch() chain is the third route to an unsuccessful Match, and it is the one a
// caller is most likely to read an Index from by accident.
TEST(Regex2397MatchTests, TheEndOfANextMatchChainReportsIndexZero) {
    Regex re("\\d+");
    Match m = re.Match("a1b2");
    int seen = 0;
    while (m.getSuccessProperty()) {
        ++seen;
        m = m.NextMatch();
    }
    EXPECT_EQ(seen, 2);
    EXPECT_FALSE(m.getSuccessProperty());
    EXPECT_EQ(m.getIndexProperty(), 0);
}
