// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Permanent pins for ticket #2022 — the approval-package verification of #2013 … #2021
// (docs/SystemTextApprovalPackage.md; docs/SystemTextNamespaceReviewPlan.md §14 and §25).
//
// #2022 changes no production code. It closes a gap the #2006 batch left: that batch
// promised every approval-gated System::Text behaviour was "pinned by a permanent test so
// none can land silently", and `TextUnitContractTests` delivers that for #2013, #2014,
// #2015, #2016 (UTF-32 only) and #2017 (decoder half) — but **two blocked findings had no
// pin at all**, and three more were pinned only in part:
//
//   SR-AUD-294 / #2018  Rune's ASCII-only classification — the only Rune tests in the
//                       repository (tests/integration/.../TextRemainingTests.cpp) use ASCII
//                       exclusively, so they pass identically before and after a Unicode
//                       repair. Nothing failed if #2018 landed unapproved.
//   SR-AUD-299 / #2021  EncodingInfo::GetEncoding — no test existed anywhere.
//   SR-AUD-291 / #2016  only Encoding::UTF32() was pinned; Encoding::BigEndianUnicode()
//                       emits a byte-order mark as payload too, and the DECODE direction
//                       silently consumes one, neither of which plan §14.4 mentions.
//   SR-AUD-292 / #2017  only the decoder direction was pinned; a configured ENCODER
//                       replacement is ignored by ASCIIEncoding, which hard-codes '?'.
//   SR-AUD-298 / #2020  the three narrowing rows were pinned; the row where adopting the
//                       shared grammar WIDENS acceptance was not.
//
// Every expectation below is deliberately the PORT's measured answer, not .NET's
// (build-probe/2022_probe1_verify.log). A future ticket that adopts .NET must FAIL here and
// reach its approval sentence rather than pass unnoticed.

#include <gtest/gtest.h>
#include "System/Text/DecoderFallback.hpp"
#include "System/ArgumentException.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "System/FormatException.hpp"
#include "System/Text/ASCIIEncoding.hpp"
#include "System/Text/CompositeFormat.hpp"
#include "System/Text/EncoderFallback.hpp"
#include "System/Text/Encoding.hpp"
#include "System/Text/EncodingInfo.hpp"
#include "System/Text/Rune.hpp"
#include "System/Text/UTF32Encoding.hpp"
#include "System/Text/UTF8Encoding.hpp"
#include "System/Text/UnicodeEncoding.hpp"

using SharpRuntime::bytecs;
using System::Text::CompositeFormat;
using System::Text::Encoding;
using System::Text::EncodingInfo;
using System::Text::Rune;

// ---------------------------------------------------------------------------------------
// SR-AUD-294 / #2018 (cause T-L) — Rune's category and case members are ASCII-only.
// ---------------------------------------------------------------------------------------

TEST(TextGatedBehaviourPinTests, Fix2018_RuneCategoryMembersAreUnicodeAware) {
    // INVERTED by #2018. Every row below asserted the divergence the finding recorded and now
    // asserts the value the finding said .NET returns.
    const Rune eAcuteLower{uint32_t(0x00E9)};  // LATIN SMALL LETTER E WITH ACUTE
    const Rune eAcuteUpper{uint32_t(0x00C9)};  // LATIN CAPITAL LETTER E WITH ACUTE
    const Rune arabicZero{uint32_t(0x0660)};   // ARABIC-INDIC DIGIT ZERO
    const Rune greekAlpha{uint32_t(0x03B1)};   // GREEK SMALL LETTER ALPHA

    EXPECT_TRUE(Rune::IsLetter(eAcuteLower));
    EXPECT_TRUE(Rune::IsLetter(eAcuteUpper));
    EXPECT_TRUE(Rune::IsLetter(greekAlpha));
    EXPECT_TRUE(Rune::IsDigit(arabicZero));
    EXPECT_TRUE(Rune::IsLetterOrDigit(eAcuteLower));
    EXPECT_TRUE(Rune::IsUpper(eAcuteUpper));
    EXPECT_TRUE(Rune::IsLower(eAcuteLower));

    // ASCII is unchanged -- the widening must not have moved the range that already worked.
    EXPECT_TRUE(Rune::IsLetter(Rune('A')));
    EXPECT_TRUE(Rune::IsDigit(Rune('7')));
    EXPECT_TRUE(Rune::IsUpper(Rune('A')));
    EXPECT_TRUE(Rune::IsLower(Rune('a')));
    EXPECT_FALSE(Rune::IsLetter(Rune('7')));
    EXPECT_FALSE(Rune::IsDigit(Rune('A')));

    // IsLetter is the FIVE letter categories, not just the cased two -- the row that fails if
    // it is written as IsUpper || IsLower.
    EXPECT_TRUE(Rune::IsLetter(Rune(uint32_t(0x01C5))));   // Lt DZ WITH CARON
    EXPECT_TRUE(Rune::IsLetter(Rune(uint32_t(0x02B0))));   // Lm MODIFIER LETTER SMALL H
    EXPECT_TRUE(Rune::IsLetter(Rune(uint32_t(0x05D0))));   // Lo HEBREW LETTER ALEF
    EXPECT_FALSE(Rune::IsUpper(Rune(uint32_t(0x01C5))));   // titlecase is not uppercase
    EXPECT_FALSE(Rune::IsLower(Rune(uint32_t(0x01C5))));

    // IsDigit is DecimalDigitNumber only, not "has a numeric value" -- so a Roman numeral and
    // a circled digit are both false, which is .NET's distinction and not an accident.
    EXPECT_FALSE(Rune::IsDigit(Rune(uint32_t(0x216B))));   // Nl ROMAN NUMERAL TWELVE
    EXPECT_FALSE(Rune::IsDigit(Rune(uint32_t(0x2460))));   // No CIRCLED DIGIT ONE
    EXPECT_TRUE(Rune::IsLetterOrDigit(Rune(uint32_t(0x0660))));
    EXPECT_FALSE(Rune::IsLetterOrDigit(Rune(uint32_t(0x2460))));

    // Supplementary code points work, which is the half a char-based table could not do.
    EXPECT_TRUE(Rune::IsLetter(Rune(uint32_t(0x10000))));   // LINEAR B SYLLABLE B008 A
    EXPECT_TRUE(Rune::IsDigit(Rune(uint32_t(0x1D7CE))));    // MATHEMATICAL BOLD DIGIT ZERO
    EXPECT_TRUE(Rune::IsUpper(Rune(uint32_t(0x10400))));    // DESERET CAPITAL LETTER LONG I
    EXPECT_TRUE(Rune::IsLower(Rune(uint32_t(0x10428))));    // DESERET SMALL LETTER LONG I
}

TEST(TextGatedBehaviourPinTests, Fix2018_RuneCasingUsesTheSimpleMappingTable) {
    // INVERTED by #2018: each of these was a no-op and now maps.
    EXPECT_EQ(0x00C9u, Rune::ToUpper(Rune(uint32_t(0x00E9))).getValueProperty());
    EXPECT_EQ(0x00E9u, Rune::ToLower(Rune(uint32_t(0x00C9))).getValueProperty());
    EXPECT_EQ(0x0391u, Rune::ToUpper(Rune(uint32_t(0x03B1))).getValueProperty());

    EXPECT_EQ(static_cast<uint32_t>('A'), Rune::ToUpper(Rune('a')).getValueProperty());
    EXPECT_EQ(static_cast<uint32_t>('a'), Rune::ToLower(Rune('A')).getValueProperty());

    // Supplementary casing, which is where the plane-preserving mask earns its keep: a 16-bit
    // delta cannot cross a plane, so the high half is carried rather than added.
    EXPECT_EQ(0x10428u, Rune::ToLower(Rune(uint32_t(0x10400))).getValueProperty());
    EXPECT_EQ(0x10400u, Rune::ToUpper(Rune(uint32_t(0x10428))).getValueProperty());

    // THE SIMPLE MAPPING IS ALL THERE IS, and that is .NET's constraint too rather than a
    // shortcut: a full mapping can produce more than one Rune and the return type is one Rune.
    EXPECT_EQ(0x00DFu, Rune::ToUpper(Rune(uint32_t(0x00DF))).getValueProperty())
        << "SHARP S has no single-code-point uppercase; .NET leaves it alone as well";
    // ...and a character with no case at all is returned unchanged rather than perturbed.
    EXPECT_EQ(0x0660u, Rune::ToUpper(Rune(uint32_t(0x0660))).getValueProperty());
    EXPECT_EQ(0x4E00u, Rune::ToLower(Rune(uint32_t(0x4E00))).getValueProperty());

    // Round-trip over every uppercase BMP scalar: lower it, raise it back, and it must land
    // where it started -- except where Unicode itself says otherwise. Asserted as the EXACT
    // SET rather than a count, because the five exceptions turn out to have one explanation
    // and a bare number would have hidden it.
    //
    // Four of the five are Unicode's own DUPLICATE letters, which lowercase into the canonical
    // letter whose uppercase is the canonical capital, not the duplicate. The fifth is SHARP S.
    // None of them is a defect in this table; a repair that "fixed" the round trip would be
    // disagreeing with the UCD.
    std::vector<uint32_t> notRoundTripped;
    for (uint32_t cp = 0; cp <= 0xFFFF; ++cp) {
        if (cp >= 0xD800 && cp <= 0xDFFF) continue;          // surrogates are not scalars
        const Rune r{cp};
        if (!Rune::IsUpper(r)) continue;
        if (Rune::ToUpper(Rune::ToLower(r)).getValueProperty() != cp) notRoundTripped.push_back(cp);
    }
    EXPECT_EQ(notRoundTripped, (std::vector<uint32_t>{
        0x03F4,   // GREEK CAPITAL THETA SYMBOL -> U+03B8 -> U+0398 GREEK CAPITAL LETTER THETA
        0x1E9E,   // LATIN CAPITAL LETTER SHARP S -> U+00DF, which has no single-scalar upper
        0x2126,   // OHM SIGN -> U+03C9 -> U+03A9 GREEK CAPITAL LETTER OMEGA
        0x212A,   // KELVIN SIGN -> U+006B -> U+004B LATIN CAPITAL LETTER K
        0x212B,   // ANGSTROM SIGN -> U+00E5 -> U+00C5 LATIN CAPITAL LETTER A WITH RING ABOVE
    }));
}

TEST(TextGatedBehaviourPinTests, RuneIsWhiteSpaceIsUnicodeAwareAndIncludesOneScalarDotNetDoesNot) {
    // The self-contradiction SR-AUD-294 rests on: this one classifier IS Unicode-aware while
    // its six siblings are not.
    EXPECT_TRUE(Rune::IsWhiteSpace(Rune(uint32_t(0x00A0))));  // NO-BREAK SPACE
    EXPECT_TRUE(Rune::IsWhiteSpace(Rune(uint32_t(0x2003))));  // EM SPACE
    EXPECT_TRUE(Rune::IsWhiteSpace(Rune(uint32_t(0x3000))));  // IDEOGRAPHIC SPACE
    EXPECT_TRUE(Rune::IsWhiteSpace(Rune(' ')));

    // Measured by #2022 and NOT named by SR-AUD-294: the table this port ships also contains
    // U+FEFF, which .NET removed from its white-space set — `Char.IsWhiteSpace('﻿')` is
    // false there. Recorded as a post-audit observation folded into #2018 (no SR-AUD
    // identifier issued; numbering stays frozen at 364), and pinned here so the repair that
    // adopts real Unicode tables has to decide it deliberately.
    EXPECT_TRUE(Rune::IsWhiteSpace(Rune(uint32_t(0xFEFF))))
        << "gated by #2018: .NET reports false for U+FEFF";
    // The complement: a scalar .NET also reports false for, and this port already does.
    EXPECT_FALSE(Rune::IsWhiteSpace(Rune(uint32_t(0x180E))));
}

// ---------------------------------------------------------------------------------------
// SR-AUD-299 / #2021 (cause T-F) — EncodingInfo::GetEncoding ignores its declared code page.
// ---------------------------------------------------------------------------------------

TEST(TextGatedBehaviourPinTests, Fix2021_EncodingInfoResolvesItsOwnCodePage) {
    // #2021 LANDED 2026-08-17, and this pin is INVERTED. GetEncoding() returned
    // Encoding::UTF8() unconditionally, so EncodingInfo(20127, "us-ascii", ...) handed back an
    // object whose getCodePageProperty() was 65001 and which encoded "é" as UTF-8 -- an object
    // that REPORTS one code page and BEHAVES as another, which is worse than one that refuses.
    // .NET's EncodingInfo.GetEncoding() is Provider?.GetEncoding(CodePage) ??
    // Encoding.GetEncoding(CodePage) (EncodingInfo.cs:56) -- it resolves its own code page.
    const EncodingInfo ascii(20127, "us-ascii", "US-ASCII");
    EXPECT_EQ(20127, ascii.getCodePageProperty());
    EXPECT_EQ("us-ascii", ascii.getNameProperty());
    EXPECT_EQ("US-ASCII", ascii.getDisplayNameProperty());

    const auto resolved = ascii.GetEncoding();
    ASSERT_NE(nullptr, resolved);
    EXPECT_EQ(20127, resolved->getCodePageProperty());
    EXPECT_EQ(Encoding::ASCII().get(), resolved.get());

    // It does not merely report the right code page -- it behaves as that encoding.
    const auto bytes = resolved->GetBytes("\xC3\xA9");
    ASSERT_EQ(1u, bytes.size()) << "ASCII emits one '?' byte for a non-ASCII scalar";
    EXPECT_EQ('?', bytes[0]);

    // Every code page this component implements resolves to its own encoding.
    EXPECT_EQ(65001, EncodingInfo(65001, "utf-8", "Unicode (UTF-8)").GetEncoding()->getCodePageProperty());
    EXPECT_EQ(1200,  EncodingInfo(1200, "utf-16", "Unicode").GetEncoding()->getCodePageProperty());
    EXPECT_EQ(1201,  EncodingInfo(1201, "utf-16BE", "Unicode (Big-Endian)").GetEncoding()->getCodePageProperty());
    EXPECT_EQ(12000, EncodingInfo(12000, "utf-32", "Unicode (UTF-32)").GetEncoding()->getCodePageProperty());
    EXPECT_EQ(12001, EncodingInfo(12001, "utf-32BE", "Unicode (UTF-32 Big-Endian)").GetEncoding()->getCodePageProperty());
    EXPECT_EQ(65000, EncodingInfo(65000, "utf-7", "Unicode (UTF-7)").GetEncoding()->getCodePageProperty());
    EXPECT_EQ(28591, EncodingInfo(28591, "iso-8859-1", "Western European (ISO)").GetEncoding()->getCodePageProperty());
}

TEST(TextGatedBehaviourPinTests, Fix2021_AnUnimplementedCodePageIsRejectedRatherThanSubstituted) {
    // The half that matters most. This port has no EncodingProvider registry, so a code page it
    // does not implement cannot be resolved -- and returning SOMETHING is exactly the defect.
    // .NET rejects with ArgumentException and Argument_EncodingNotSupported.
    const EncodingInfo shiftJis(932, "shift_jis", "Japanese (Shift-JIS)");
    EXPECT_THROW((void)shiftJis.GetEncoding(), System::ArgumentException);
    try {
        (void)shiftJis.GetEncoding();
        FAIL() << "expected ArgumentException";
    } catch (const System::ArgumentException& e) {
        EXPECT_NE(std::string(e.what()).find("is not a supported encoding name"), std::string::npos)
            << e.what();
    }
}

// ---------------------------------------------------------------------------------------
// SR-AUD-291 / #2016 (cause T-J) — the halves plan §14.4 does not name.
// ---------------------------------------------------------------------------------------

TEST(TextGatedBehaviourPinTests, Fix2016_GetBytesEmitsNoByteOrderMarkOnAnyFactory) {
    // #2016 LANDED 2026-08-17, and this pin is INVERTED. plan §14.4 named Encoding::UTF32()
    // only; #2022 measured that BigEndianUnicode() is constructed as UnicodeEncoding(true, true)
    // and emitted a mark as payload too, so the repair had to cover BOTH default factories.
    //
    // .NET keeps the mark out of GetBytes entirely: it is what GetPreamble() returns
    // (UTF32Encoding.cs:1113-1128). A mark emitted as payload is concatenated into strings,
    // counted in lengths, and written a second time by anything that also writes a preamble.
    const auto be = Encoding::BigEndianUnicode()->GetBytes("A");
    ASSERT_EQ(2u, be.size());
    EXPECT_EQ(0x00, be[0]);
    EXPECT_EQ('A', be[1]);

    EXPECT_EQ(4u, Encoding::UTF32()->GetBytes("A").size());
    EXPECT_EQ(2u, Encoding::Unicode()->GetBytes("A").size());

    // The per-instance BOM setting no longer changes GetBytes at all -- it only changes what
    // GetPreamble() reports, which is the whole point of the split.
    EXPECT_EQ(2u, System::Text::UnicodeEncoding(false, false).GetBytes("A").size());
    EXPECT_EQ(2u, System::Text::UnicodeEncoding(false, true).GetBytes("A").size());
    EXPECT_EQ(4u, System::Text::UTF32Encoding(false, false).GetBytes("A").size());
    EXPECT_EQ(4u, System::Text::UTF32Encoding(false, true).GetBytes("A").size());
}

TEST(TextGatedBehaviourPinTests, Fix2016_TheMarkIsAvailableFromGetPreambleInstead) {
    // The repair must not have DELETED the byte-order mark, only moved it to where a caller can
    // ask for it -- otherwise code that legitimately wants a BOM would have no way to get one.
    EXPECT_EQ((std::vector<bytecs>{0xFF, 0xFE}),
              System::Text::UnicodeEncoding(false, true).GetPreamble());
    EXPECT_EQ((std::vector<bytecs>{0xFE, 0xFF}),
              System::Text::UnicodeEncoding(true, true).GetPreamble());
    EXPECT_EQ((std::vector<bytecs>{0xFF, 0xFE, 0x00, 0x00}),
              System::Text::UTF32Encoding(false, true).GetPreamble());
    EXPECT_EQ((std::vector<bytecs>{0x00, 0x00, 0xFE, 0xFF}),
              System::Text::UTF32Encoding(true, true).GetPreamble());

    // ...and an encoding configured without a mark reports none.
    EXPECT_TRUE(System::Text::UnicodeEncoding(false, false).GetPreamble().empty());
    EXPECT_TRUE(System::Text::UTF32Encoding(false, false).GetPreamble().empty());
}

TEST(TextGatedBehaviourPinTests, TheDecodeDirectionStillConsumesALeadingByteOrderMark) {
    // Not named by SR-AUD-291 and not mentioned in plan §14.4: GetString strips a leading
    // U+FEFF, so a real ZERO WIDTH NO-BREAK SPACE at the start of a document is lost. .NET's
    // UnicodeEncoding.GetString does not strip it. Measured by #2022 and folded into #2016 —
    // no SR-AUD identifier issued; numbering stays frozen at 364.
    const std::vector<bytecs> u16{0xFF, 0xFE, 'A', 0x00};
    const std::string decoded16 =
        System::Text::UnicodeEncoding(false, false).GetString(u16.data(), 0, 4);
    EXPECT_EQ("A", decoded16) << "gated by #2016: the U+FEFF is dropped, not decoded";

    const std::vector<bytecs> u32{0xFF, 0xFE, 0x00, 0x00, 'A', 0x00, 0x00, 0x00};
    const std::string decoded32 = System::Text::UTF32Encoding(false, false).GetString(u32.data(), 0, 8);
    EXPECT_EQ("A", decoded32) << "gated by #2016: the U+FEFF is dropped, not decoded";

    // Why no existing test caught either half: a round trip cancels them out exactly.
    const auto roundTrip = Encoding::UTF32()->GetBytes("A");
    EXPECT_EQ("A", Encoding::UTF32()->GetString(roundTrip));
}

// ---------------------------------------------------------------------------------------
// SR-AUD-292 / #2017 (cause T-K) — the encoder direction of the inert fallback policy.
// ---------------------------------------------------------------------------------------

TEST(TextGatedBehaviourPinTests, Fix2017_AConfiguredEncoderReplacementIsHonouredByAscii) {
    // #2017 LANDED 2026-08-17, and this pin is INVERTED. ASCIIEncoding hard-coded '?' for an
    // unencodable scalar, so a configured EncoderReplacementFallback("!") was accepted, stored,
    // and then ignored -- a policy the caller could set and never observe.
    System::Text::ASCIIEncoding ascii;
    ascii.setEncoderFallbackProperty(std::make_shared<System::Text::EncoderReplacementFallback>("!"));
    const auto bytes = ascii.GetBytes("\xC3\xA9");   // U+00E9, not representable in ASCII
    ASSERT_EQ(1u, bytes.size());
    EXPECT_EQ('!', bytes[0]);

    // The DEFAULT is unchanged, which is what keeps this a repair rather than a behaviour swap:
    // ASCIIEncoding's default is the replacement fallback with "?" (ASCIIEncoding.cs:56-61).
    EXPECT_EQ('?', System::Text::ASCIIEncoding().GetBytes("\xC3\xA9")[0]);
}

TEST(TextGatedBehaviourPinTests, Fix2017_AConfiguredExceptionFallbackReachesEveryEncoding) {
    // The headline. A configured EXCEPTION decoder fallback threw only in UTF-8; UTF-16, UTF-32,
    // ASCII and Latin-1 substituted directly and never consulted it.
    const std::vector<bytecs> loneSurrogate{0x00, 0xD8};
    System::Text::UnicodeEncoding u16(false, false);
    u16.setDecoderFallbackProperty(System::Text::DecoderFallback::ExceptionFallback());
    EXPECT_THROW((void)u16.GetString(loneSurrogate.data(), 0, 2),
                 System::Text::DecoderFallbackException);

    const std::vector<bytecs> outOfRange{0x00, 0x00, 0x11, 0x00};   // > U+10FFFF, little-endian
    System::Text::UTF32Encoding u32(false, false);
    u32.setDecoderFallbackProperty(System::Text::DecoderFallback::ExceptionFallback());
    EXPECT_THROW((void)u32.GetString(outOfRange.data(), 0, 4),
                 System::Text::DecoderFallbackException);

    const std::vector<bytecs> highByte{0xE9};
    System::Text::ASCIIEncoding ascii;
    ascii.setDecoderFallbackProperty(System::Text::DecoderFallback::ExceptionFallback());
    EXPECT_THROW((void)ascii.GetString(highByte.data(), 0, 1),
                 System::Text::DecoderFallbackException);
}

TEST(TextGatedBehaviourPinTests, Fix2017_ATruncatedTrailingUnitReachesTheFallback) {
    // #2017's second finding. A truncated trailing unit was DISCARDED outright:
    // UTF16LE.GetString(3 bytes) returned one character and the odd byte vanished with no
    // diagnostic of any kind. It is undecodable input like any other.
    const std::vector<bytecs> oddLength{'A', 0x00, 'B'};
    EXPECT_EQ("A\xEF\xBF\xBD",
              System::Text::UnicodeEncoding(false, false).GetString(oddLength.data(), 0, 3))
        << "the trailing byte must be substituted, not dropped";

    System::Text::UnicodeEncoding throwing(false, false);
    throwing.setDecoderFallbackProperty(System::Text::DecoderFallback::ExceptionFallback());
    EXPECT_THROW((void)throwing.GetString(oddLength.data(), 0, 3),
                 System::Text::DecoderFallbackException)
        << "an exception fallback must be able to report the truncation";

    const std::vector<bytecs> sixBytes{'A', 0x00, 0x00, 0x00, 'B', 0x00};
    EXPECT_EQ("A\xEF\xBF\xBD",
              System::Text::UTF32Encoding(false, false).GetString(sixBytes.data(), 0, 6));
}

// ---------------------------------------------------------------------------------------
// SR-AUD-298 / #2020 (cause T-N) — LANDED. This pin used to hold the gate shut; it now holds
// the answer, and the answer is not the one the plan proposed.
// ---------------------------------------------------------------------------------------

TEST(TextGatedBehaviourPinTests, Fix2020_ParseWidensExactlyWhereDotNetWidens) {
    // #2022 measured (build-probe/2022_probe1_verify.log §A) that adopting the shared grammar
    // would not only narrow: "{0 }" — a trailing space inside the item — was a FormatException
    // and the shared scanner accepts it. That measurement was right, and #2020 landed the
    // widening rather than treating it as a reason to stay put: it is .NET's own rule
    // (CompositeFormat.cs:211-217 consumes spaces after the index).
    EXPECT_EQ(1, CompositeFormat::Parse("{0 }").getMinimumArgumentCountProperty());
    EXPECT_EQ(1, CompositeFormat::Parse("{0  ,5}").getMinimumArgumentCountProperty());

    // The index ceiling, however, is not where plan §14.8 puts it and not where #2022's
    // correction (i) puts it either. Both readings assumed Parse must adopt
    // AppendFormatHelper's IndexLimit; .NET's Parse HAS NO INDEX LIMIT — TryParseLiterals
    // writes `while (char.IsAsciiDigit(ch))` (CompositeFormat.cs:201) against the formatter's
    // `&& index < IndexLimit` (ValueStringBuilder.AppendFormat.cs:99). So all four of these
    // keep the answers #2010 gave them, and they are now confirmed against the reference
    // rather than pinned pending an approval.
    EXPECT_EQ(1000001, CompositeFormat::Parse("{1000000}").getMinimumArgumentCountProperty());
    EXPECT_EQ(10000000, CompositeFormat::Parse("{9999999}").getMinimumArgumentCountProperty());
    EXPECT_EQ(10000001, CompositeFormat::Parse("{10000000}").getMinimumArgumentCountProperty());
    EXPECT_EQ(2147483647, CompositeFormat::Parse("{2147483646}").getMinimumArgumentCountProperty());
}
