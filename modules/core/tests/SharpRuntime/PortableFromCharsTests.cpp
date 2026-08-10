// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// SR-AUD-180 / ticket #2221 -- `PortableFromCharsFloat` must never read at or past `last`.
//
// This is the first test coverage this header has ever had. The audit recorded that "no test
// forces the fallback path; Linux normally chooses native floating std::from_chars, masking every
// Apple deployment-target behavior" -- which is true of `FromCharsFloat`, the dispatching wrapper,
// but NOT of `PortableFromCharsFloat`, which is an ordinary public function template that any
// platform can call directly. Every test below calls it directly, so the Apple-only fallback is
// exercised on the Linux gate.
//
// Two properties are asserted separately throughout, because a parser can get either wrong on its
// own: WHAT it returns (value and status) and HOW MUCH it consumed (`ptr`). Where the platform has
// a real `std::from_chars`, the test asserts the fallback agrees with it rather than asserting a
// hand-written expectation -- the helper's whole contract is to be a drop-in.
#include <gtest/gtest.h>

#include <charconv>
#include <cmath>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "SharpRuntime/PortableFromChars.hpp"

namespace {

using SharpRuntime::FromCharsFloat;
using SharpRuntime::PortableFromCharsFloat;

struct Outcome {
    std::errc ec{};
    std::ptrdiff_t consumed = 0;
    double value = 0.0;
    bool wrote = false;
};

/** Runs the fallback over [first, last) and reports all four observable facts separately. */
Outcome runFallback(const char* first, const char* last) {
    constexpr double sentinel = -12345.6789;
    double v = sentinel;
    const std::from_chars_result r = PortableFromCharsFloat(first, last, v);
    return {r.ec, r.ptr - first, v, v != sentinel};
}

/** The same range through the platform's real std::from_chars, for agreement assertions. */
Outcome runNative(const char* first, const char* last) {
    constexpr double sentinel = -12345.6789;
    double v = sentinel;
    const std::from_chars_result r = std::from_chars(first, last, v);
    return {r.ec, r.ptr - first, v, v != sentinel};
}

/**
 * Asserts the fallback and the platform's own std::from_chars agree on every observable, over a
 * range that deliberately does NOT end at a terminator.
 */
void expectAgreesWithNative(std::string_view backing, std::size_t rangeLength,
                            const char* what) {
    const char* first = backing.data();
    const char* last = backing.data() + rangeLength;
    const Outcome fb = runFallback(first, last);
    const Outcome nat = runNative(first, last);
    EXPECT_EQ(static_cast<int>(nat.ec), static_cast<int>(fb.ec)) << what << " (status)";
    EXPECT_EQ(nat.consumed, fb.consumed) << what << " (consumed)";
    EXPECT_EQ(nat.wrote, fb.wrote) << what << " (wrote output?)";
    if (nat.wrote && fb.wrote) {
        EXPECT_DOUBLE_EQ(nat.value, fb.value) << what << " (value)";
    }
}

}  // namespace

// ---------------------------------------------------------------------------
// The finding itself: a range whose backing storage continues past `last`.
// ---------------------------------------------------------------------------

TEST(PortableFromCharsRangeTests, AuditedCase_OneCharacterRangeBackedByTwo) {
    // The audit's own reproduction. Before #2221 this returned value 12 and ptr at offset 2 --
    // two past a ONE-character range.
    const char* text = "12";
    const Outcome fb = runFallback(text, text + 1);
    EXPECT_EQ(std::errc{}, fb.ec);
    EXPECT_EQ(1, fb.consumed);
    EXPECT_DOUBLE_EQ(1.0, fb.value);
    expectAgreesWithNative("12", 1, "\"12\" over [0,1)");
}

TEST(PortableFromCharsRangeTests, ExponentSplitByTheRangeBoundaryChangesTheValue) {
    // Probe case P12, and a sharper shape than the audit's: before the repair this returned
    // 1000 for a range containing only "1".
    const char* text = "1e3";
    const Outcome fb = runFallback(text, text + 1);
    EXPECT_EQ(std::errc{}, fb.ec);
    EXPECT_EQ(1, fb.consumed);
    EXPECT_DOUBLE_EQ(1.0, fb.value);
    expectAgreesWithNative("1e3", 1, "\"1e3\" over [0,1)");
    // And the fraction split at the point.
    expectAgreesWithNative("1.75", 1, "\"1.75\" over [0,1)");
    expectAgreesWithNative("1.75", 2, "\"1.75\" over [0,2)");
    expectAgreesWithNative("1.75", 3, "\"1.75\" over [0,3)");
}

TEST(PortableFromCharsRangeTests, SignSplitByTheRangeBoundary) {
    // Probe case P13: before the repair "-57" restricted to [0,2) returned -57 and ptr at 3.
    expectAgreesWithNative("-57", 2, "\"-57\" over [0,2)");
    // A range holding only the sign is not a number at all.
    const char* text = "-57";
    const Outcome fb = runFallback(text, text + 1);
    EXPECT_EQ(std::errc::invalid_argument, fb.ec);
    EXPECT_EQ(0, fb.consumed);
    EXPECT_FALSE(fb.wrote);
}

TEST(PortableFromCharsRangeTests, EveryPrefixOfALongLiteralAgreesWithNative) {
    // A systematic sweep: every prefix length of one backing string, so no single boundary can
    // be right by accident.
    static const char kBacking[] = "-123.4567e+12";
    const std::size_t n = std::strlen(kBacking);
    for (std::size_t len = 1; len <= n; ++len) {
        const std::string what = std::string("prefix length ") + std::to_string(len);
        expectAgreesWithNative(kBacking, len, what.c_str());
    }
}

TEST(PortableFromCharsRangeTests, NoNulAnywhereInTheBackingStorage) {
    // Probe case P1: the shape AddressSanitizer cannot see, because the over-read happens inside
    // the C library. The heap block holds exactly two characters and no terminator.
    std::vector<char> block{'1', '2'};
    const Outcome fb = runFallback(block.data(), block.data() + 1);
    EXPECT_EQ(std::errc{}, fb.ec);
    EXPECT_EQ(1, fb.consumed);
    EXPECT_DOUBLE_EQ(1.0, fb.value);
    // The whole two-character block is still an ordinary parse.
    const Outcome whole = runFallback(block.data(), block.data() + 2);
    EXPECT_EQ(std::errc{}, whole.ec);
    EXPECT_EQ(2, whole.consumed);
    EXPECT_DOUBLE_EQ(12.0, whole.value);
}

TEST(PortableFromCharsRangeTests, FloatOverloadIsBoundedToo) {
    // Probe case P14: the float instantiation is a separate template specialisation and needs its
    // own coverage.
    const char* text = "12";
    float v = -1.0f;
    const std::from_chars_result r = PortableFromCharsFloat(text, text + 1, v);
    EXPECT_EQ(std::errc{}, r.ec);
    EXPECT_EQ(1, r.ptr - text);
    EXPECT_FLOAT_EQ(1.0f, v);
}

// ---------------------------------------------------------------------------
// The bounded-parse matrix. Acceptance, consumption, value and status are asserted separately.
// ---------------------------------------------------------------------------

TEST(PortableFromCharsMatrixTests, EmptyRange) {
    const char* text = "5";
    const Outcome fb = runFallback(text, text);
    EXPECT_EQ(std::errc::invalid_argument, fb.ec);
    EXPECT_EQ(0, fb.consumed);
    EXPECT_FALSE(fb.wrote);
}

TEST(PortableFromCharsMatrixTests, SingleCharacterAndShortestValidInput) {
    expectAgreesWithNative("0", 1, "\"0\"");
    expectAgreesWithNative("7", 1, "\"7\"");
    const Outcome one = runFallback("7", "7" + 1);
    EXPECT_EQ(std::errc{}, one.ec);
    EXPECT_EQ(1, one.consumed);
    EXPECT_DOUBLE_EQ(7.0, one.value);
}

TEST(PortableFromCharsMatrixTests, ExactLongestValidInputIsFullyConsumed) {
    const std::string longest = "-1.7976931348623157e+308";  // -DBL_MAX, written out in full
    const Outcome fb = runFallback(longest.data(), longest.data() + longest.size());
    EXPECT_EQ(std::errc{}, fb.ec);
    EXPECT_EQ(static_cast<std::ptrdiff_t>(longest.size()), fb.consumed);
    EXPECT_DOUBLE_EQ(-std::numeric_limits<double>::max(), fb.value);
}

TEST(PortableFromCharsMatrixTests, MalformedPrefixIsRejectedWithoutConsuming) {
    for (const char* bad : {"abc", "e5", ".", "-", "-.", "--5", "x1"}) {
        const Outcome fb = runFallback(bad, bad + std::strlen(bad));
        EXPECT_EQ(std::errc::invalid_argument, fb.ec) << bad;
        EXPECT_EQ(0, fb.consumed) << bad;
        EXPECT_FALSE(fb.wrote) << bad;
    }
}

TEST(PortableFromCharsMatrixTests, MalformedSuffixStopsAtTheJunkAndReportsHowMuchItRead) {
    // std::from_chars deliberately permits a trailing remainder and reports it via `ptr`; both
    // in-repository callers require ptr == last, so the contract is the caller's to enforce.
    expectAgreesWithNative("1.5abc", 6, "\"1.5abc\"");
    const Outcome fb = runFallback("1.5abc", "1.5abc" + 6);
    EXPECT_EQ(std::errc{}, fb.ec);
    EXPECT_EQ(3, fb.consumed);
    EXPECT_DOUBLE_EQ(1.5, fb.value);
    expectAgreesWithNative("1.5e", 4, "\"1.5e\" -- an exponent with no digits");
    expectAgreesWithNative("1.5e+", 5, "\"1.5e+\"");
}

TEST(PortableFromCharsMatrixTests, EmbeddedNulInsideTheDeclaredRange) {
    // The C parser stops at the embedded NUL, so the range is not fully consumed and the caller's
    // `ptr == last` check rejects it -- which is also what std::from_chars does.
    const char storage[7] = {'1', '.', '5', '\0', ' ', '9', '\0'};
    const Outcome fb = runFallback(storage, storage + 6);
    EXPECT_EQ(std::errc{}, fb.ec);
    EXPECT_EQ(3, fb.consumed);
    EXPECT_DOUBLE_EQ(1.5, fb.value);
    expectAgreesWithNative(std::string_view(storage, 6), 6, "\"1.5\\0 9\"");
}

TEST(PortableFromCharsMatrixTests, LeadingSignAsymmetryMatchesFromChars) {
    // A minus sign is parsed; a plus sign is not. That asymmetry is std::from_chars's, and the
    // fallback has always corrected strtod for it -- this pins it.
    expectAgreesWithNative("-1.5", 4, "\"-1.5\"");
    const Outcome plus = runFallback("+1.5", "+1.5" + 4);
    EXPECT_EQ(std::errc::invalid_argument, plus.ec);
    EXPECT_EQ(0, plus.consumed);
    EXPECT_FALSE(plus.wrote);
}

TEST(PortableFromCharsMatrixTests, SurroundingWhitespaceIsNeverSkipped) {
    for (const char* ws : {" 1.5", "\t1.5", "\n1.5", "\r1.5", "\f1.5", "\v1.5"}) {
        const Outcome fb = runFallback(ws, ws + std::strlen(ws));
        EXPECT_EQ(std::errc::invalid_argument, fb.ec) << ws;
        EXPECT_EQ(0, fb.consumed) << ws;
    }
    // Trailing whitespace is ordinary unconsumed remainder, not an error.
    expectAgreesWithNative("1.5 ", 4, "\"1.5 \"");
}

TEST(PortableFromCharsMatrixTests, VeryLongDigitRunTakesTheHeapPathAndStaysExact) {
    // Longer than the stack buffer, so the nothrow heap path runs. Truncating instead of
    // allocating would change the value, which is why the implementation does not truncate.
    std::string big(700, '9');
    const Outcome fb = runFallback(big.data(), big.data() + big.size());
    const Outcome nat = runNative(big.data(), big.data() + big.size());
    EXPECT_EQ(static_cast<int>(nat.ec), static_cast<int>(fb.ec));
    EXPECT_EQ(nat.consumed, fb.consumed);
    // 700 nines overflows double, so both report out of range and neither writes the output.
    EXPECT_EQ(std::errc::result_out_of_range, fb.ec);
    EXPECT_FALSE(fb.wrote);

    // A long run that is still representable must come out exactly right.
    std::string longButFine = "1." + std::string(600, '0') + "5";
    const Outcome ok = runFallback(longButFine.data(), longButFine.data() + longButFine.size());
    EXPECT_EQ(std::errc{}, ok.ec);
    EXPECT_EQ(static_cast<std::ptrdiff_t>(longButFine.size()), ok.consumed);
    EXPECT_DOUBLE_EQ(1.0, ok.value);

    // and the boundary around the stack buffer itself
    for (std::size_t pad : {std::size_t{509}, std::size_t{510}, std::size_t{511},
                            std::size_t{512}, std::size_t{513}}) {
        std::string s = "1." + std::string(pad, '0') + "5";
        const Outcome b = runFallback(s.data(), s.data() + s.size());
        EXPECT_EQ(std::errc{}, b.ec) << "length " << s.size();
        EXPECT_EQ(static_cast<std::ptrdiff_t>(s.size()), b.consumed) << "length " << s.size();
        EXPECT_DOUBLE_EQ(1.0, b.value) << "length " << s.size();
    }
}

TEST(PortableFromCharsMatrixTests, AllZeroInput) {
    expectAgreesWithNative("0000", 4, "\"0000\"");
    expectAgreesWithNative("0.000", 5, "\"0.000\"");
    expectAgreesWithNative("-0", 2, "\"-0\"");
    const Outcome negZero = runFallback("-0", "-0" + 2);
    EXPECT_EQ(std::errc{}, negZero.ec);
    EXPECT_TRUE(std::signbit(negZero.value)) << "the sign of a negative zero must survive";
}

TEST(PortableFromCharsMatrixTests, MaximumRepresentableAndOneAbove) {
    expectAgreesWithNative("1.7976931348623157e308", 22, "DBL_MAX");
    const Outcome max = runFallback("1.7976931348623157e308", "1.7976931348623157e308" + 22);
    EXPECT_EQ(std::errc{}, max.ec);
    EXPECT_DOUBLE_EQ(std::numeric_limits<double>::max(), max.value);

    // One decade above: out of range, output untouched, and `ptr` still at the end of the match.
    const char* over = "1e309";
    const Outcome fb = runFallback(over, over + 5);
    EXPECT_EQ(std::errc::result_out_of_range, fb.ec);
    EXPECT_EQ(5, fb.consumed);
    EXPECT_FALSE(fb.wrote);
}

TEST(PortableFromCharsMatrixTests, MinimumNegativeAndOneBelow) {
    const char* min = "-1.7976931348623157e308";
    const Outcome fb = runFallback(min, min + std::strlen(min));
    EXPECT_EQ(std::errc{}, fb.ec);
    EXPECT_DOUBLE_EQ(-std::numeric_limits<double>::max(), fb.value);

    const char* below = "-1e309";
    const Outcome under = runFallback(below, below + 6);
    EXPECT_EQ(std::errc::result_out_of_range, under.ec);
    EXPECT_FALSE(under.wrote);

    // Underflow reports the same status, in the other direction.
    const char* tiny = "1e-400";
    const Outcome small = runFallback(tiny, tiny + 6);
    EXPECT_EQ(std::errc::result_out_of_range, small.ec);
    EXPECT_FALSE(small.wrote);
}

TEST(PortableFromCharsMatrixTests, RepeatedMissingAndUnexpectedDelimiters) {
    // "delimiter" here means the two the grammar has: the decimal point and the exponent marker.
    expectAgreesWithNative("1..5", 4, "repeated decimal point");
    expectAgreesWithNative("1ee5", 4, "repeated exponent marker");
    expectAgreesWithNative("1e", 2, "exponent marker with no digits");
    expectAgreesWithNative("15", 2, "no delimiter at all");
    expectAgreesWithNative(".5", 2, "leading point, no integer part");
    expectAgreesWithNative("5.", 2, "trailing point, no fraction");
    expectAgreesWithNative("1,5", 3, "an unexpected delimiter -- grouping is the caller's job");
}

TEST(PortableFromCharsMatrixTests, FailureLeavesTheOutputUntouched) {
    // A rejection must not publish a partial value.
    constexpr double sentinel = 42.5;
    for (const char* bad : {"", "+1", " 1", "abc", "-"}) {
        double v = sentinel;
        const std::from_chars_result r =
            PortableFromCharsFloat(bad, bad + std::strlen(bad), v);
        EXPECT_NE(std::errc{}, r.ec) << bad;
        EXPECT_DOUBLE_EQ(sentinel, v) << bad;
        EXPECT_EQ(bad, r.ptr) << bad;
    }
}

// ---------------------------------------------------------------------------
// Contract properties of the helper itself.
// ---------------------------------------------------------------------------

TEST(PortableFromCharsContractTests, BothEntryPointsAreNoexceptLikeStdFromChars) {
    // ADDED, not dropped. std::from_chars is noexcept, and Single::tryParseCore is too, so on the
    // fallback platform a throwing helper would have called std::terminate.
    const char* text = "1.5";
    double d = 0.0;
    float f = 0.0f;
    static_assert(noexcept(PortableFromCharsFloat(text, text + 3, d)));
    static_assert(noexcept(PortableFromCharsFloat(text, text + 3, f)));
    static_assert(noexcept(FromCharsFloat(text, text + 3, d)));
    static_assert(noexcept(FromCharsFloat(text, text + 3, f)));
    SUCCEED();
}

TEST(PortableFromCharsContractTests, DispatchingWrapperAgreesWithTheFallbackOnOrdinaryInput) {
    // On this platform FromCharsFloat selects the native overload; the point of the assertion is
    // that the two paths cannot disagree for the inputs the repository actually parses.
    for (const char* text : {"0", "1.5", "-1.5", "1e10", "-2.25e-3", "123456789"}) {
        const std::size_t n = std::strlen(text);
        double viaWrapper = 0.0;
        double viaFallback = 0.0;
        const std::from_chars_result a = FromCharsFloat(text, text + n, viaWrapper);
        const std::from_chars_result b = PortableFromCharsFloat(text, text + n, viaFallback);
        EXPECT_EQ(static_cast<int>(a.ec), static_cast<int>(b.ec)) << text;
        EXPECT_EQ(a.ptr - text, b.ptr - text) << text;
        EXPECT_DOUBLE_EQ(viaWrapper, viaFallback) << text;
    }
}

TEST(PortableFromCharsContractTests, TheTrimmedSubrangeSingleAndDoubleActuallyPassIsHandledRight) {
    // The header used to claim every call site passes a whole NUL-terminated std::string. It does
    // not: Single::tryParseCore and Double::tryParseCore trim surrounding ASCII whitespace before
    // forming the range. This reproduces that exact range and pins the result.
    const std::string s = " 1.5 ";
    std::string_view sv{s};
    auto isws = [](char c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
    };
    while (!sv.empty() && isws(sv.front())) sv.remove_prefix(1);
    while (!sv.empty() && isws(sv.back())) sv.remove_suffix(1);

    ASSERT_NE(sv.data() + sv.size(), s.data() + s.size())
        << "the premise of this test is that `last` is NOT the string terminator";

    const Outcome fb = runFallback(sv.data(), sv.data() + sv.size());
    EXPECT_EQ(std::errc{}, fb.ec);
    EXPECT_EQ(3, fb.consumed);
    EXPECT_DOUBLE_EQ(1.5, fb.value);
}

// ---------------------------------------------------------------------------
// Ticket #2222 -- a second, different divergence in the same helper, found by inventory while
// reproducing SR-AUD-180 and deliberately not folded into it: strtof/strtod accept C99
// hexadecimal floating literals and std::from_chars with chars_format::general does not.
// This is a GRAMMAR cause, not the range cause, so it carries no SR-AUD identifier.
//
// Measured before the fix: PortableFromCharsFloat("0x10") returned ec=0, ptr at offset 4, value
// 16, where std::from_chars returns ec=0, ptr at offset 1, value 0.
// ---------------------------------------------------------------------------

TEST(PortableFromCharsGrammarTests, HexadecimalPrefixStopsAtTheXExactlyAsFromCharsDoes) {
    for (const char* text : {"0x10", "-0x10", "0X1p3", "0x", "-0x", "0xg", "0x0", "0X"}) {
        const std::size_t n = std::strlen(text);
        expectAgreesWithNative(std::string_view(text, n), n, text);
    }
    // Spelled out for the headline case, so the intended answer is readable and not only implied.
    const Outcome fb = runFallback("0x10", "0x10" + 4);
    EXPECT_EQ(std::errc{}, fb.ec);
    EXPECT_EQ(1, fb.consumed);
    EXPECT_DOUBLE_EQ(0.0, fb.value);
    // The sign is still consumed and carried, so "-0x10" yields a negative zero over two
    // characters.
    const Outcome neg = runFallback("-0x10", "-0x10" + 5);
    EXPECT_EQ(std::errc{}, neg.ec);
    EXPECT_EQ(2, neg.consumed);
    EXPECT_TRUE(std::signbit(neg.value));
}

TEST(PortableFromCharsGrammarTests, DecimalSpellingsThatStartWithZeroKeepWorking) {
    // The guard must key on an 'x' immediately after the FIRST digit, and nothing else.
    for (const char* text : {"0", "-0", "0.5", "-0.5", "0e3", "0.0", "0000", "0.25e-3"}) {
        const std::size_t n = std::strlen(text);
        expectAgreesWithNative(std::string_view(text, n), n, text);
    }
}

TEST(PortableFromCharsGrammarTests, AnXThatIsNotAHexPrefixIsLeftToTheOrdinaryParser) {
    // "00x1" and "0.0x1" have no hexadecimal prefix -- the 'x' does not follow the first digit --
    // and the C parser already stops in the right place, so the guard must not touch them.
    for (const char* text : {"00x1", "0.0x1", "1x", "10x2", "-00x1"}) {
        const std::size_t n = std::strlen(text);
        expectAgreesWithNative(std::string_view(text, n), n, text);
    }
}

TEST(PortableFromCharsGrammarTests, TheHexGuardDoesNotDisturbTheRangeBound) {
    // The two repairs compose: a hexadecimal prefix split by the range boundary must still obey
    // `last`, and must not read the character that would have completed the prefix.
    expectAgreesWithNative("0x10", 1, "\"0x10\" over [0,1)");
    expectAgreesWithNative("0x10", 2, "\"0x10\" over [0,2)");
    expectAgreesWithNative("0x10", 3, "\"0x10\" over [0,3)");
    std::vector<char> block{'0', 'x', '1'};
    const Outcome fb = runFallback(block.data(), block.data() + 1);
    EXPECT_EQ(std::errc{}, fb.ec);
    EXPECT_EQ(1, fb.consumed);
    EXPECT_DOUBLE_EQ(0.0, fb.value);
}
