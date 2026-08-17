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
// repairs: the grammar halves of both findings are approval-gated (#2020 and #2019), and a
// later change that quietly adopted them must fail here rather than pass unnoticed.

#include <gtest/gtest.h>

#include <string>

#include "System/ArgumentOutOfRangeException.hpp"
#include "System/FormatException.hpp"
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

TEST(CompositeFormatBoundaryTests, TheGatedGrammarRowsAreDeliberatelyStillAccepted) {
    // #2020 would make these throw. #2010 must NOT, and this test is what stops the
    // narrowing landing without its approval sentence (plan §14.8).
    EXPECT_EQ(1, CompositeFormat::Parse("{0,not-a-width}").getMinimumArgumentCountProperty());
    EXPECT_EQ(1, CompositeFormat::Parse("{0,-}").getMinimumArgumentCountProperty());
    EXPECT_EQ(1000001, CompositeFormat::Parse("{1000000}").getMinimumArgumentCountProperty())
        << "adopting System::detail::kCompositeIndexLimit here is #2020, not #2010";
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
