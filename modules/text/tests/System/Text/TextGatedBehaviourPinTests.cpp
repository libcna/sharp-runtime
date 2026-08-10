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

TEST(TextGatedBehaviourPinTests, RuneCategoryMembersAreStillAsciiOnly) {
    const Rune eAcuteLower{uint32_t(0x00E9)};  // é  LATIN SMALL LETTER E WITH ACUTE
    const Rune eAcuteUpper{uint32_t(0x00C9)};  // É  LATIN CAPITAL LETTER E WITH ACUTE
    const Rune arabicZero{uint32_t(0x0660)};   // ٠  ARABIC-INDIC DIGIT ZERO
    const Rune greekAlpha{uint32_t(0x03B1)};   // α  GREEK SMALL LETTER ALPHA

    EXPECT_FALSE(Rune::IsLetter(eAcuteLower)) << "gated by #2018 (plan §14.6): .NET reports true";
    EXPECT_FALSE(Rune::IsLetter(eAcuteUpper)) << "gated by #2018: .NET reports true";
    EXPECT_FALSE(Rune::IsLetter(greekAlpha)) << "gated by #2018: .NET reports true";
    EXPECT_FALSE(Rune::IsDigit(arabicZero)) << "gated by #2018: .NET reports true";
    EXPECT_FALSE(Rune::IsLetterOrDigit(eAcuteLower)) << "gated by #2018";
    EXPECT_FALSE(Rune::IsUpper(eAcuteUpper)) << "gated by #2018: .NET reports true";
    EXPECT_FALSE(Rune::IsLower(eAcuteLower)) << "gated by #2018: .NET reports true";

    // The ASCII range is the whole of what works, which is why the pre-existing ASCII-only
    // Rune tests cannot detect the divergence in either direction.
    EXPECT_TRUE(Rune::IsLetter(Rune('A')));
    EXPECT_TRUE(Rune::IsDigit(Rune('7')));
    EXPECT_TRUE(Rune::IsUpper(Rune('A')));
    EXPECT_TRUE(Rune::IsLower(Rune('a')));
}

TEST(TextGatedBehaviourPinTests, RuneCasingIsStillANoOpOutsideAscii) {
    EXPECT_EQ(0x00E9u, Rune::ToUpper(Rune(uint32_t(0x00E9))).getValueProperty())
        << "gated by #2018 (plan §14.6): .NET maps é to É (U+00C9)";
    EXPECT_EQ(0x00C9u, Rune::ToLower(Rune(uint32_t(0x00C9))).getValueProperty())
        << "gated by #2018: .NET maps É to é (U+00E9)";
    EXPECT_EQ(0x03B1u, Rune::ToUpper(Rune(uint32_t(0x03B1))).getValueProperty())
        << "gated by #2018: .NET maps α to Α (U+0391)";

    EXPECT_EQ(static_cast<uint32_t>('A'), Rune::ToUpper(Rune('a')).getValueProperty());
    EXPECT_EQ(static_cast<uint32_t>('a'), Rune::ToLower(Rune('A')).getValueProperty());
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

TEST(TextGatedBehaviourPinTests, EncodingInfoStillIgnoresItsOwnCodePage) {
    const EncodingInfo ascii(20127, "us-ascii", "US-ASCII");
    EXPECT_EQ(20127, ascii.getCodePageProperty());
    EXPECT_EQ("us-ascii", ascii.getNameProperty());
    EXPECT_EQ("US-ASCII", ascii.getDisplayNameProperty());

    const auto resolved = ascii.GetEncoding();
    ASSERT_NE(nullptr, resolved);
    EXPECT_EQ(65001, resolved->getCodePageProperty())
        << "gated by #2021 (plan §14.9): the resolved encoding should be code page 20127";

    // It does not merely report the wrong code page — it encodes as UTF-8.
    const auto bytes = resolved->GetBytes("\xC3\xA9");
    ASSERT_EQ(2u, bytes.size()) << "gated by #2021: ASCII would emit one '?' byte";
    EXPECT_EQ(0xC3, bytes[0]);
    EXPECT_EQ(0xA9, bytes[1]);

    // And the object it hands out is the shared, publicly mutable factory instance #2013
    // owns — so #2021 and #2013 must be decided together, which plan §14.9 does not say.
    EXPECT_EQ(Encoding::UTF8().get(), resolved.get())
        << "gated by #2021 + #2013: EncodingInfo hands out the shared mutable singleton";

    // Every other code page resolves to the same object today, including one this port does
    // not implement at all.
    EXPECT_EQ(Encoding::UTF8().get(), EncodingInfo(1200, "utf-16", "Unicode").GetEncoding().get());
    EXPECT_EQ(Encoding::UTF8().get(), EncodingInfo(28591, "iso-8859-1", "Latin 1").GetEncoding().get());
    EXPECT_EQ(Encoding::UTF8().get(), EncodingInfo(437, "ibm437", "OEM US").GetEncoding().get())
        << "gated by #2021: an unimplemented code page would start throwing";
}

// ---------------------------------------------------------------------------------------
// SR-AUD-291 / #2016 (cause T-J) — the halves plan §14.4 does not name.
// ---------------------------------------------------------------------------------------

TEST(TextGatedBehaviourPinTests, BothBomEmittingFactoriesArePinnedNotOnlyUtf32) {
    // plan §14.4 names Encoding::UTF32() only. Measured (#2022): BigEndianUnicode() is
    // constructed as UnicodeEncoding(true, true) and emits a byte-order mark as payload too,
    // so the approval covers TWO default factories, not one.
    const auto be = Encoding::BigEndianUnicode()->GetBytes("A");
    ASSERT_EQ(4u, be.size()) << "gated by #2016: without the BOM this is 2 bytes";
    EXPECT_EQ(0xFE, be[0]);
    EXPECT_EQ(0xFF, be[1]);
    EXPECT_EQ(0x00, be[2]);
    EXPECT_EQ('A', be[3]);

    const auto le32 = Encoding::UTF32()->GetBytes("A");
    ASSERT_EQ(8u, le32.size()) << "gated by #2016";
    EXPECT_EQ(0xFF, le32[0]);
    EXPECT_EQ(0xFE, le32[1]);

    // The little-endian UTF-16 factory does not, which is what makes the inconsistency
    // visible from inside the library.
    EXPECT_EQ(2u, Encoding::Unicode()->GetBytes("A").size());

    // The non-default constructors decide it per instance; only the factories are gated.
    EXPECT_EQ(2u, System::Text::UnicodeEncoding(false, false).GetBytes("A").size());
    EXPECT_EQ(4u, System::Text::UnicodeEncoding(false, true).GetBytes("A").size());
    EXPECT_EQ(4u, System::Text::UTF32Encoding(false, false).GetBytes("A").size());
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

TEST(TextGatedBehaviourPinTests, AConfiguredEncoderReplacementIsStillIgnoredByAscii) {
    System::Text::ASCIIEncoding ascii;
    ascii.setEncoderFallbackProperty(std::make_shared<System::Text::EncoderReplacementFallback>("!"));
    const auto bytes = ascii.GetBytes("\xC3\xA9");
    ASSERT_EQ(1u, bytes.size());
    EXPECT_EQ('?', bytes[0]) << "gated by #2017 (plan §14.5): the configured \"!\" is ignored; "
                                "ASCIIEncoding::GetBytes hard-codes '?'";

    // The exception fallback is inert in the same direction, for the same reason.
    ascii.setEncoderFallbackProperty(System::Text::EncoderFallback::ExceptionFallback());
    EXPECT_NO_THROW((void)ascii.GetBytes("\xC3\xA9")) << "gated by #2017";

    // UTF8Encoding is the one encoding that does route through the configured object, which
    // is what makes this an inconsistency rather than a uniform reduction.
    System::Text::UTF8Encoding utf8;
    utf8.setEncoderFallbackProperty(std::make_shared<System::Text::EncoderReplacementFallback>("!"));
    const auto viaUtf8 = utf8.GetBytes("\xFF");
    ASSERT_EQ(1u, viaUtf8.size());
    EXPECT_EQ('!', viaUtf8[0]);
}

// ---------------------------------------------------------------------------------------
// SR-AUD-298 / #2020 (cause T-N) — the row where the shared grammar WIDENS acceptance.
// ---------------------------------------------------------------------------------------

TEST(TextGatedBehaviourPinTests, CompositeFormatParseStillRejectsAnItemTheSharedGrammarAccepts) {
    // Measured (#2022, build-probe/2022_probe1_verify.log §A): adopting
    // System::detail::runCompositeFormat in #2020 does not only narrow. "{0 }" — a trailing
    // space inside the item — is a FormatException here today and is ACCEPTED by the shared
    // grammar, which skips spaces after the index. plan §14.8's approval sentence describes
    // narrowing only, and this row is the counter-example it has to name.
    EXPECT_THROW((void)CompositeFormat::Parse("{0 }"), System::FormatException)
        << "gated by #2020: the shared grammar accepts this and would return minArgCount 1";
    EXPECT_THROW((void)CompositeFormat::Parse("{0  ,5}"), System::FormatException)
        << "gated by #2020: the shared grammar accepts spaces before the alignment comma too";

    // And the index ceiling is not where §14.8 says it is: the shared grammar stops consuming
    // digits once the index reaches 1,000,000, so {1000000} … {9999999} are ACCEPTED there,
    // while {10000000} and {2147483646} are not. Both are pinned as currently accepted here.
    EXPECT_EQ(1000001, CompositeFormat::Parse("{1000000}").getMinimumArgumentCountProperty());
    EXPECT_EQ(10000000, CompositeFormat::Parse("{9999999}").getMinimumArgumentCountProperty());
    EXPECT_EQ(10000001, CompositeFormat::Parse("{10000000}").getMinimumArgumentCountProperty())
        << "gated by #2020: the shared grammar rejects this one";
    EXPECT_EQ(2147483647, CompositeFormat::Parse("{2147483646}").getMinimumArgumentCountProperty())
        << "gated by #2020: the shared grammar rejects this one";
}
