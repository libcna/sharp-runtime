// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Permanent regressions for tickets #2010 (SR-AUD-298 diagnostics half, cause T-D, CCF-012)
// and #2011 (SR-AUD-297 diagnostics half, cause T-E) of
// docs/SystemTextNamespaceReviewPlan.md.
//
// One shared cause: `std::stoi` and `std::string::substr` were used as parsers on public
// input, so malformed text either escaped as a `std::` exception out of a `System`-shaped API
// or produced a silently wrong answer. This is the class #1882 removed from `String::Format`,
// in two more components.
//
// Measured before (build-probe/2006_probe1_before.log sections L and M):
//   CompositeFormat::Parse("{2147483648}")  -> std::out_of_range (stoi)
//   CompositeFormat::Parse("{2147483647}")  -> minArgCount = -2147483648   <-- silent, worse
//   UrlEncoder::Decode("%zz")               -> std::invalid_argument (stoi)
//   UrlEncoder::Decode("%-1")               -> byte 0xff                   <-- silent
//   UrlEncoder::Decode("% 1")               -> byte 0x01                   <-- silent
//   UrlEncoder::Decode("%+f")               -> byte 0x0f                   <-- silent
//   HtmlEncoder::Default().Encode("abc",-1,2) -> std::out_of_range (substr)
//   HtmlEncoder::Default().Encode("abc",0,99) -> "abc"                     <-- silent clamp
//
// The tests that pin what is DELIBERATELY UNCHANGED matter as much as the ones that pin the
// repairs. Both grammar halves have since landed -- #2019 (the Web encoders' Basic Latin
// allow-list) and #2020 (one shared composite-format scanner) -- so the pins that used to hold
// those gates shut now hold their answers instead, and in #2020's case the answer contradicts
// the plan: .NET's CompositeFormat.Parse has no index limit, and this port's was already right.

#include <gtest/gtest.h>

#include <string>

#include "System/ArgumentOutOfRangeException.hpp"
#include "System/FormatException.hpp"
#include "System/String.hpp"
#include "System/Text/CompositeFormat.hpp"
#include "System/Text/Encodings/Web/HtmlEncoder.hpp"
#include "System/Text/Encodings/Web/JavaScriptEncoder.hpp"
#include "System/Text/Encodings/Web/UrlEncoder.hpp"

using System::Text::CompositeFormat;
using System::Text::Encodings::Web::HtmlEncoder;
using System::Text::Encodings::Web::JavaScriptEncoder;
using System::Text::Encodings::Web::UrlEncoder;

// ---------------------------------------------------------------------------------------
// #2010 -- CompositeFormat::Parse
// ---------------------------------------------------------------------------------------

TEST(CompositeFormatBoundaryTests, MaximalIndexNoLongerReturnsANegativeCount) {
    // The defect the finding does not name: `maxIdx + 1` on an intcs already holding
    // INT32_MAX is undefined behaviour, and it returned -2147483648.
    EXPECT_THROW((void)CompositeFormat::Parse("{2147483647}"), System::FormatException);
}

TEST(CompositeFormatBoundaryTests, OversizedIndexRaisesFormatExceptionNotStdOutOfRange) {
    for (const char* fmt : {"{2147483648}", "{4294967296}", "{99999999999999999999}",
                            "{000000000000000000000000009999999999}"}) {
        SCOPED_TRACE(fmt);
        EXPECT_THROW((void)CompositeFormat::Parse(fmt), System::FormatException);
    }
}

TEST(CompositeFormatBoundaryTests, NoStdExceptionEscapesForAnyInput) {
    // The whole point of the ticket: whatever the answer, it is a System exception or a
    // value -- never a std:: type leaking out of a System-shaped API.
    for (const char* fmt : {"", "{", "}", "{}", "{{", "}}", "{0", "0}", "{0:", "{0,",
                            "{ 0 }", "{-1}", "{+1}", "{0x1}", "{2147483648}",
                            "{99999999999999999999}", "{0}{1}{2}", "{{{0}}}"}) {
        SCOPED_TRACE(fmt);
        try {
            (void)CompositeFormat::Parse(fmt);
        } catch (const System::FormatException&) {
            // expected for the malformed ones
        } catch (const std::exception& e) {
            FAIL() << "a std:: exception escaped: " << e.what();
        }
    }
}

TEST(CompositeFormatBoundaryTests, LargestRepresentableIndexStillParses) {
    // The bound is INT32_MAX - 1, the largest index whose `+ 1` is representable, so the
    // repair narrows by exactly one value.
    auto cf = CompositeFormat::Parse("{2147483646}");
    EXPECT_EQ(2147483647, cf.getMinimumArgumentCountProperty());
}

TEST(CompositeFormatBoundaryTests, EveryPreviouslyAcceptedFormatIsUnchanged) {
    EXPECT_EQ(0, CompositeFormat::Parse("").getMinimumArgumentCountProperty());
    EXPECT_EQ(0, CompositeFormat::Parse("no items here").getMinimumArgumentCountProperty());
    EXPECT_EQ(1, CompositeFormat::Parse("{0}").getMinimumArgumentCountProperty());
    EXPECT_EQ(2, CompositeFormat::Parse("{0} {1}").getMinimumArgumentCountProperty());
    EXPECT_EQ(2, CompositeFormat::Parse("{1} {0}").getMinimumArgumentCountProperty());
    EXPECT_EQ(0, CompositeFormat::Parse("{{0}}").getMinimumArgumentCountProperty());
    EXPECT_EQ(1, CompositeFormat::Parse("{0:X}").getMinimumArgumentCountProperty());
    EXPECT_EQ(1, CompositeFormat::Parse("{0,10}").getMinimumArgumentCountProperty());
    EXPECT_EQ(1, CompositeFormat::Parse("{0,-10:D3}").getMinimumArgumentCountProperty());
    EXPECT_EQ("{0} {1}", CompositeFormat::Parse("{0} {1}").getFormatProperty());
    // A large but representable index keeps its defined answer.
    EXPECT_EQ(1500001, CompositeFormat::Parse("{1500000}").getMinimumArgumentCountProperty());
}

TEST(CompositeFormatBoundaryTests, EveryPreviouslyRejectedFormatIsStillRejected) {
    for (const char* fmt : {"Hello {", "a } b", "{ 0 }", "{-1}", "{}", "{0:{1}}", "{abc}"}) {
        SCOPED_TRACE(fmt);
        EXPECT_THROW((void)CompositeFormat::Parse(fmt), System::FormatException);
    }
}

// ---------------------------------------------------------------------------------------
// #2020 -- the grammar half. The hand-written scanner is gone; Parse now shares
// System::detail::scanCompositeFormat with String::Format and FormattableString::ToString.
// ---------------------------------------------------------------------------------------

TEST(CompositeFormatBoundaryTests, Fix2020_AnAlignmentThatIsNotANumberIsRejected) {
    // The defect: the old scanner skipped everything between the index and the closing
    // brace, so an alignment component was never read at all. .NET requires an ASCII digit
    // after the optional minus sign -- `width = ch - '0'; if ((uint)width >= 10u) goto
    // FailureExpectedAsciiDigit;` (CompositeFormat.cs:249-253) -- and so does String::Format
    // here, which is the divergence CCF-012 names.
    for (const char* fmt : {"{0,not-a-width}", "{0,-}", "{0,}", "{0,- 5}", "{0,x}", "{0,+5}"}) {
        SCOPED_TRACE(fmt);
        EXPECT_THROW((void)CompositeFormat::Parse(fmt), System::FormatException);
    }
}

TEST(CompositeFormatBoundaryTests, Fix2020_TheSpacesDotNetAllowsInsideAnItemAreNowAccepted) {
    // Adoption is not only a narrowing. .NET consumes spaces after the index
    // (CompositeFormat.cs:211-217), after the alignment comma (228-235) and after the
    // alignment digits (269-275); the old scanner rejected every one of them because a
    // trailing space made its index substring non-numeric.
    for (const char* fmt : {"{0 }", "{0  ,5}", "{0 :X}", "{0,5 }", "{0, -5}", "{0,  5  }"}) {
        SCOPED_TRACE(fmt);
        EXPECT_EQ(1, CompositeFormat::Parse(fmt).getMinimumArgumentCountProperty());
    }
}

TEST(CompositeFormatBoundaryTests, Fix2020_ALeadingSpaceIsStillRejected) {
    // The asymmetry is .NET's, not an oversight: the FIRST character after `{` is read as a
    // digit before any whitespace rule applies (`int index = ch - '0'; if ((uint)index >= 10u)`
    // -- CompositeFormat.cs:186-190), so a space may follow the index but never precede it.
    for (const char* fmt : {"{ 0}", "{ 0 }", "{ }"}) {
        SCOPED_TRACE(fmt);
        EXPECT_THROW((void)CompositeFormat::Parse(fmt), System::FormatException);
    }
}

TEST(CompositeFormatBoundaryTests, Fix2020_ParseHasNoIndexLimitAndTheFindingSaidItShould) {
    // #2020's description asserts that Parse("{1500000}") should be rejected "where .NET's
    // AppendFormatHelper index limit is 1,000,000". It is measurably wrong, and this is the
    // test that keeps it wrong. .NET has TWO composite-format grammars and they differ on
    // exactly this: TryParseLiterals writes `while (char.IsAsciiDigit(ch))`
    // (CompositeFormat.cs:201) where AppendFormatHelper writes
    // `while (char.IsAsciiDigit(ch) && index < IndexLimit)`
    // (ValueStringBuilder.AppendFormat.cs:99). Adopting the formatter's limit here would have
    // introduced a NEW divergence while claiming to remove one.
    EXPECT_EQ(1000001, CompositeFormat::Parse("{1000000}").getMinimumArgumentCountProperty());
    EXPECT_EQ(1500001, CompositeFormat::Parse("{1500000}").getMinimumArgumentCountProperty());
    EXPECT_EQ(10000000, CompositeFormat::Parse("{9999999}").getMinimumArgumentCountProperty());
    EXPECT_EQ(10000001, CompositeFormat::Parse("{10000000}").getMinimumArgumentCountProperty());
    EXPECT_EQ(2147483647, CompositeFormat::Parse("{2147483646}").getMinimumArgumentCountProperty());

    // And the alignment has no limit either, so a digit run no int could hold is ACCEPTED --
    // .NET wraps the value, this port saturates it, and neither exposes it.
    EXPECT_EQ(1, CompositeFormat::Parse("{0,99999999999999999999}")
                     .getMinimumArgumentCountProperty());
}

TEST(CompositeFormatBoundaryTests, Fix2020_ParseAndFormatNowAgreeOnEveryBraceRule) {
    // The CCF-012 closure, stated as an assertion rather than a claim: for every format
    // string whose indices are small enough that the argument list cannot be the reason,
    // Parse accepts exactly what String::Format accepts. Before #2020 these were two
    // hand-written grammars and six of these rows disagreed.
    for (const char* fmt : {"", "no items", "{0}", "{0} {1}", "{1} {0}", "{{0}}", "{0:X}",
                            "{0,10}", "{0,-10:D3}", "{{{0}}}", "{0 }", "{0  ,5}", "{0 :X}",
                            "{0,5 }", "{0, -5}", "{0,- 5}", "{ 0 }", "{0,not-a-width}",
                            "{0,-}", "{0,}", "{0:{1}}", "Hello {", "a } b", "{}", "{-1}",
                            "{abc}", "}}", "{{", "{0:}", "{0::}", "{0:a}b{1}"}) {
        SCOPED_TRACE(fmt);
        bool parseThrew = false, formatThrew = false;
        try {
            (void)CompositeFormat::Parse(fmt);
        } catch (const System::FormatException&) { parseThrew = true; }
        try {
            (void)System::String::Format(fmt, std::string("x"), std::string("y"));
        } catch (const System::FormatException&) { formatThrew = true; }
        EXPECT_EQ(parseThrew, formatThrew)
            << "the two doors disagree on this format string, which is CCF-012 exactly";
    }
}

// ---------------------------------------------------------------------------------------
// #2011 -- UrlEncoder::Decode
// ---------------------------------------------------------------------------------------

TEST(UrlEncoderDecodeTests, ValidPercentEscapesAreUnchanged) {
    EXPECT_EQ("A", UrlEncoder::Decode("%41"));
    EXPECT_EQ("z", UrlEncoder::Decode("%7a"));
    EXPECT_EQ("z", UrlEncoder::Decode("%7A"));
    EXPECT_EQ(std::string("\0", 1), UrlEncoder::Decode("%00" "x").substr(0, 1));
    EXPECT_EQ("a b", UrlEncoder::Decode("a+b"));
    EXPECT_EQ("", UrlEncoder::Decode(""));
    EXPECT_EQ("plain", UrlEncoder::Decode("plain"));
    EXPECT_EQ("a/b?c=d", UrlEncoder::Decode("a%2Fb%3Fc%3Dd"));
    // The full round trip through the sibling encoder in the same file.
    const std::string original = "a b/c?d=e&f~g-h_i.j";
    EXPECT_EQ(original, UrlEncoder::Decode(UrlEncoder::Encode(original)));
}

TEST(UrlEncoderDecodeTests, NonHexadecimalEscapesRaiseFormatExceptionNotAWrongByte) {
    // "%zz" used to throw std::invalid_argument; the other three used to SUCCEED with a
    // wrong byte, which the finding does not name.
    for (const char* text : {"%zz", "%-1", "% 1", "%+f", "%g0", "%0g", "%  ", "x%zzy"}) {
        SCOPED_TRACE(text);
        EXPECT_THROW((void)UrlEncoder::Decode(text), System::FormatException);
    }
}

TEST(UrlEncoderDecodeTests, TruncatedPercentSequencesKeepTheirLiteralPassThrough) {
    // Unchanged: fewer than two bytes follow the '%', so no escape is attempted at all.
    EXPECT_EQ("a%4", UrlEncoder::Decode("a%4"));
    EXPECT_EQ("%", UrlEncoder::Decode("%"));
    EXPECT_EQ("%4", UrlEncoder::Decode("%4"));
    EXPECT_EQ("ab%", UrlEncoder::Decode("ab%"));
}

TEST(UrlEncoderDecodeTests, EmbeddedNulSurvives) {
    const std::string in("a%00b", 5);
    const std::string out = UrlEncoder::Decode(in);
    ASSERT_EQ(3u, out.size());
    EXPECT_EQ('a', out[0]);
    EXPECT_EQ('\0', out[1]);
    EXPECT_EQ('b', out[2]);
}

TEST(UrlEncoderDecodeTests, NoStdExceptionEscapesForAnyInput) {
    for (const char* text : {"%", "%%", "%%%", "%zz", "%-1", "%ff", "+", "%2B", "%FF%FE"}) {
        SCOPED_TRACE(text);
        try {
            (void)UrlEncoder::Decode(text);
        } catch (const System::FormatException&) {
        } catch (const std::exception& e) {
            FAIL() << "a std:: exception escaped: " << e.what();
        }
    }
}

// ---------------------------------------------------------------------------------------
// #2011 -- HtmlEncoder's substring overload
// ---------------------------------------------------------------------------------------

TEST(HtmlEncoderRangeTests, NegativeArgumentsRaiseArgumentOutOfRange) {
    try {
        (void)HtmlEncoder::Default().Encode("abc", -1, 2);
        FAIL() << "expected ArgumentOutOfRangeException";
    } catch (const System::ArgumentOutOfRangeException& e) {
        EXPECT_EQ("startIndex", e.getParamNameProperty());
    }
    try {
        (void)HtmlEncoder::Default().Encode("abc", 0, -1);
        FAIL() << "expected ArgumentOutOfRangeException";
    } catch (const System::ArgumentOutOfRangeException& e) {
        EXPECT_EQ("characterCount", e.getParamNameProperty());
    }
}

TEST(HtmlEncoderRangeTests, AnOverLongRangeIsRejectedInsteadOfSilentlyClamped) {
    try {
        (void)HtmlEncoder::Default().Encode("abc", 0, 99);
        FAIL() << "expected ArgumentOutOfRangeException";
    } catch (const System::ArgumentOutOfRangeException& e) {
        EXPECT_EQ("characterCount", e.getParamNameProperty());
    }
    EXPECT_THROW((void)HtmlEncoder::Default().Encode("abc", 2, 2),
                 System::ArgumentOutOfRangeException);
    EXPECT_THROW((void)HtmlEncoder::Default().Encode("abc", 4, 0),
                 System::ArgumentOutOfRangeException);
}

TEST(HtmlEncoderRangeTests, MaximalArgumentsDoNotOverflowTheRangeCheck) {
    EXPECT_THROW((void)HtmlEncoder::Default().Encode("abc", 2147483647, 2147483647),
                 System::ArgumentOutOfRangeException);
}

TEST(HtmlEncoderRangeTests, EveryInRangeCallIsUnchanged) {
    EXPECT_EQ("bc", HtmlEncoder::Default().Encode("abc", 1, 2));
    EXPECT_EQ("abc", HtmlEncoder::Default().Encode("abc", 0, 3));
    EXPECT_EQ("", HtmlEncoder::Default().Encode("abc", 3, 0));
    EXPECT_EQ("", HtmlEncoder::Default().Encode("", 0, 0));
    EXPECT_EQ("&lt;b&gt;", HtmlEncoder::Default().Encode("x<b>y", 1, 3));
}

// #2019 LANDED 2026-08-17, and this pin is INVERTED. The default encoders passed every
// non-ASCII scalar through unchanged; .NET's defaults are built with an ALLOW-LIST and escape
// everything outside it -- DefaultHtmlEncoder.BasicLatinSingleton and its JavaScript sibling are
// both `new Default…Encoder(new TextEncoderSettings(UnicodeRanges.BasicLatin))`
// (DefaultHtmlEncoder.cs:13, DefaultJavaScriptEncoder.cs:11), i.e. U+0000..U+007F and nothing
// else. The target set was the half the old approval package recorded as unverifiable; the
// reference tree settles it.
// #2044's other half. `System::Net::WebUtility::HtmlEncode` and this encoder use DIFFERENT
// escape sets, deliberately, exactly as .NET's two HTML encoders do: WebUtility emits DECIMAL
// references for U+00A0..U+00FF and passes U+20AC through, while this one escapes everything
// above Basic Latin as uppercase hex. The counterpart pin is
// NetGatedBehaviourPinTests.Fix2044_ThisEncoderDIFFERSFromHtmlEncoderAndThatIsDotNets, and the
// two are kept in their own suites on purpose -- asserting the comparison in one place would
// need a component edge between Net and Text for no gain.
TEST(HtmlEncoderRangeTests, Fix2019_TheDefaultEncodersEscapeOutsideBasicLatin) {
    // &#x + the SCALAR in uppercase hex, no padding, then ';' (DefaultHtmlEncoder.cs:98-126) --
    // one escape for the whole scalar, not one per surrogate half.
    EXPECT_EQ("&#xE9;", HtmlEncoder::Encode("\xC3\xA9"));            // U+00E9
    EXPECT_EQ("&#x20AC;", HtmlEncoder::Encode("\xE2\x82\xAC"));      // U+20AC
    EXPECT_EQ("&#x1F600;", HtmlEncoder::Encode("\xF0\x9F\x98\x80")); // U+1F600

    // \uXXXX with four uppercase hex digits, and a SURROGATE PAIR for a supplementary scalar,
    // because that is what a JavaScript string literal can express
    // (DefaultJavaScriptEncoder.cs:129-150).
    EXPECT_EQ("\\u00E9", JavaScriptEncoder::Encode("\xC3\xA9"));
    EXPECT_EQ("\\uD83D\\uDE00", JavaScriptEncoder::Encode("\xF0\x9F\x98\x80"));

    // The five ASCII escapes the encoder already implemented are unchanged, and Basic Latin
    // still passes through -- an allow-list that escaped everything would be useless.
    EXPECT_EQ("&amp;&lt;&gt;&quot;&#x27;", HtmlEncoder::Encode("&<>\"'"));
    EXPECT_EQ("abcXYZ 0189~", HtmlEncoder::Encode("abcXYZ 0189~"));
    EXPECT_EQ("abcXYZ 0189~", JavaScriptEncoder::Encode("abcXYZ 0189~"));
}

TEST(HtmlEncoderRangeTests, Fix2019_TheDefaultIsConservativeOnPurpose) {
    // Stated as a test rather than only in a comment: this is a NARROWING, and it is the safe
    // direction. An allow-list escapes a character nobody thought about; a deny-list ships it.
    // A caller who needs non-ASCII text to survive unescaped needs a relaxed encoder, which .NET
    // also requires and which this port does not yet provide.
    for (const char* text : {"\xC3\xA9", "\xE2\x82\xAC", "\xF0\x9F\x98\x80",
                             "\xEF\xBB\xBF", "\xC2\xA0"}) {
        const std::string encoded = HtmlEncoder::Encode(text);
        EXPECT_NE(encoded, text) << "[" << text << "] survived the allow-list";
        for (char c : encoded) {
            EXPECT_LT(static_cast<unsigned char>(c), 0x80u)
                << "the encoded form must itself be Basic Latin";
        }
    }
}
