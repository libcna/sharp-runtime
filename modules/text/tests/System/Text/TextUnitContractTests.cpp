// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Permanent pins for ticket #2012 (SR-AUD-290 + SR-AUD-296 disclosure half, and SR-AUD-289's
// doc-comment, cause T-I of docs/SystemTextNamespaceReviewPlan.md).
//
// #2012 changes no executable behaviour: it makes three headers' contracts true where they
// promised managed character units the code does not deliver. What it adds executably is
// THIS FILE -- the measured current units, pinned, so that the approval-gated semantic
// changes (#2014 Latin-1 scalars, #2015 the byte-vs-character unit) cannot land silently.
//
// Every expectation below is deliberately the PORT's answer, not .NET's. A future ticket
// that adopts .NET must fail here and reach its approval sentence
// (plan sections 14.2 and 14.3) rather than pass unnoticed.

#include <gtest/gtest.h>
#include "System/Text/EncoderFallback.hpp"
#include "System/InvalidOperationException.hpp"

#include <string>
#include <vector>

#include "System/Text/Encoding.hpp"
#include "System/Text/Latin1Encoding.hpp"
#include "System/Text/StringBuilder.hpp"
#include "System/Text/UTF8Encoding.hpp"
#include "System/Text/UnicodeEncoding.hpp"
#include "System/Text/UTF32Encoding.hpp"

using SharpRuntime::bytecs;
using System::Text::Encoding;
using System::Text::Latin1Encoding;
using System::Text::StringBuilder;
using System::Text::UTF8Encoding;

TEST(TextUnitContractTests, GetCharCountReportsUtf8BytesNotUtf16CodeUnits) {
    // U+1F600 GRINNING FACE: four UTF-8 bytes, two UTF-16 code units.
    const std::vector<bytecs> grin{0xF0, 0x9F, 0x98, 0x80};
    EXPECT_EQ(4, Encoding::UTF8()->GetCharCount(grin.data(), 0, 4))
        << "gated by #2015 (plan section 14.3): .NET reports 2";
    EXPECT_EQ(4u, Encoding::UTF8()->GetString(grin).size());

    // U+00E9: two UTF-8 bytes, one UTF-16 code unit.
    const std::vector<bytecs> eacute{0xC3, 0xA9};
    EXPECT_EQ(2, Encoding::UTF8()->GetCharCount(eacute.data(), 0, 2));
    EXPECT_EQ(2, Encoding::UTF8()->GetByteCount("\xC3\xA9"));

    // ASCII is the range where the two units coincide, which is why the divergence is easy
    // to miss: every pre-existing test in this component uses ASCII text.
    const std::vector<bytecs> ascii{'a', 'b', 'c'};
    EXPECT_EQ(3, Encoding::UTF8()->GetCharCount(ascii.data(), 0, 3));
}

TEST(TextUnitContractTests, StringBuilderLengthIsBytes) {
    EXPECT_EQ(3, StringBuilder("\xC3\xA9" "A").getLengthProperty())
        << "gated by #2015 (plan section 14.3): .NET reports 2";
    EXPECT_EQ(4, StringBuilder("\xF0\x9F\x98\x80").getLengthProperty())
        << "gated by #2015: .NET reports 2 for a supplementary scalar";
    EXPECT_EQ(3, StringBuilder("abc").getLengthProperty());
}

TEST(TextUnitContractTests, StringBuilderIndexedMutationCanSplitACharacter) {
    // The measured consequence the class doc-comment now states outright.
    StringBuilder sb("\xC3\xA9" "A");
    sb.Remove(1, 1);
    const std::string result = sb.ToString();
    ASSERT_EQ(2u, result.size());
    EXPECT_EQ('\xC3', result[0]);
    EXPECT_EQ('A', result[1]);

    StringBuilder sb2("\xC3\xA9" "A");
    sb2.Insert(1, "X");
    EXPECT_EQ(std::string("\xC3" "X" "\xA9" "A"), sb2.ToString());
}

TEST(TextUnitContractTests, Fix2014_Latin1MapsCODEPOINTSNotStorageBytes) {
    // #2014 LANDED 2026-08-17, and this pin is INVERTED. Latin1Encoding was the ONLY encoding in
    // this component that did not decode the UTF-8 storage representation into scalars first --
    // ASCIIEncoding, UnicodeEncoding and UTF32Encoding all do -- so it mapped storage bytes and
    // called the result ISO-8859-1.
    Latin1Encoding l;
    const auto bytes = l.GetBytes("\xC3\xA9");           // U+00E9, two UTF-8 storage bytes
    ASSERT_EQ(1u, bytes.size()) << "ISO-8859-1 encodes U+00E9 as the single byte e9";
    EXPECT_EQ(0xE9, bytes[0]);

    const std::vector<bytecs> e9{0xE9};
    const std::string decoded = l.GetString(e9.data(), 0, 1);
    ASSERT_EQ(2u, decoded.size()) << "the UTF-8 representation of U+00E9 is c3 a9";
    EXPECT_EQ('\xC3', decoded[0]);
    EXPECT_EQ('\xA9', decoded[1]);

    // ASCII round-trips in both directions, exactly as it did before -- the repair must not have
    // moved the range that was already right.
    const auto hi = l.GetBytes("Hi");
    EXPECT_EQ("Hi", l.GetString(hi.data(), 0, static_cast<SharpRuntime::intcs>(hi.size())));

    // And the whole ISO-8859-1 range now round-trips, which is the property that was missing.
    for (int b = 0; b < 256; ++b) {
        const std::vector<bytecs> one{static_cast<bytecs>(b)};
        const std::string text = l.GetString(one.data(), 0, 1);
        const auto back = l.GetBytes(text);
        ASSERT_EQ(1u, back.size()) << "byte " << b;
        EXPECT_EQ(b, back[0]) << "byte " << b << " did not round-trip";
    }
}

TEST(TextUnitContractTests, Fix2014_UnrepresentableScalarsBecomeQuestionMarks) {
    // ISO-8859-1 covers U+0000..U+00FF and nothing else, so a scalar above that cannot be
    // represented. .NET's Latin1Encoding uses the replacement fallback "?" for it.
    //
    // CORRECTED BY #2355 (2026-08-19). This asserted that a supplementary-plane scalar produces
    // TWO '?', "matching the two UTF-16 code units .NET would encode it from". Measured against
    // the reference, that is wrong: .NET delivers a supplementary scalar to the fallback as a
    // surrogate PAIR through ONE call, and EncoderReplacementFallback's pair overload sets
    // `_fallbackCount = _strDefault.Length` -- the replacement string ONCE
    // (EncoderReplacementFallback.cs:117-138). So Encoding.ASCII.GetBytes("\U0001F600") is one
    // '?', not two. The doubling here existed only because the fallback parameter was a `char`
    // and could not carry a supplementary scalar; #2355 widened it, and the workaround went with
    // the limitation it worked around.
    Latin1Encoding l;
    EXPECT_EQ(std::vector<bytecs>{static_cast<bytecs>('?')}, l.GetBytes("\xE2\x82\xAC"));  // U+20AC
    const auto grin = l.GetBytes("\xF0\x9F\x98\x80");                                     // U+1F600
    EXPECT_EQ(1u, grin.size());
    EXPECT_EQ('?', grin[0]);
}

TEST(TextUnitContractTests, Fix2013_TheFactoryEncodingsAreSharedAndREADONLY) {
    // #2013 LANDED 2026-08-17, and this pin is INVERTED. The seven factory encodings are still
    // ONE shared object each -- that part was never the defect and .NET does the same -- but a
    // caller can no longer mutate it, so it can no longer change what every other caller in the
    // process decodes. .NET's answer is exactly this: ASCIIEncoding.s_default and its siblings
    // are read-only and their fallback setters throw InvalidOperationException
    // (Encoding.cs:485-497), with SR.InvalidOperation_ReadOnly = "Instance is read-only."
    EXPECT_EQ(Encoding::UTF8().get(), Encoding::UTF8().get());
    EXPECT_EQ(Encoding::ASCII().get(), Encoding::ASCII().get());

    for (const auto& factory : {Encoding::UTF8(), Encoding::ASCII(), Encoding::Unicode(),
                                Encoding::BigEndianUnicode(), Encoding::UTF32(),
                                Encoding::UTF7(), Encoding::Latin1()}) {
        EXPECT_TRUE(factory->getIsReadOnlyProperty());
        EXPECT_THROW(factory->setDecoderFallbackProperty(
                         std::make_shared<System::Text::DecoderReplacementFallback>("<X>")),
                     System::InvalidOperationException);
        EXPECT_THROW(factory->setEncoderFallbackProperty(
                         System::Text::EncoderFallback::ExceptionFallback()),
                     System::InvalidOperationException);
    }

    // ...and the shared instance still decodes the way it always did, because nothing reached it.
    const std::vector<bytecs> bad{0xFF};
    EXPECT_EQ("\xEF\xBF\xBD", Encoding::UTF8()->GetString(bad.data(), 0, 1));
}

TEST(TextUnitContractTests, Fix2013_TheReadOnlyTestPrecedesTheNullTest) {
    // Order matters and is .NET's: Encoding.cs:490-494 checks IsReadOnly BEFORE
    // ArgumentNullException.ThrowIfNull, so a null handed to a shared factory instance reports
    // the read-only violation rather than the null one.
    EXPECT_THROW(Encoding::UTF8()->setDecoderFallbackProperty(nullptr),
                 System::InvalidOperationException);
    EXPECT_THROW(Encoding::UTF8()->setEncoderFallbackProperty(nullptr),
                 System::InvalidOperationException);
}

TEST(TextUnitContractTests, Fix2013_AnEncodingTheCallerConstructedIsStillConfigurable) {
    // The control, and the migration path. Making the factories read-only must not have made
    // the fallback setters useless: an encoding the caller constructed is writable, and its
    // configuration is its own -- it does not leak into the shared instance.
    auto own = std::make_shared<System::Text::UTF8Encoding>();
    EXPECT_FALSE(own->getIsReadOnlyProperty());
    own->setDecoderFallbackProperty(
        std::make_shared<System::Text::DecoderReplacementFallback>("<X>"));

    const std::vector<bytecs> bad{0xFF};
    EXPECT_EQ("<X>", own->GetString(bad.data(), 0, 1));
    EXPECT_EQ("\xEF\xBF\xBD", Encoding::UTF8()->GetString(bad.data(), 0, 1))
        << "configuring a caller-owned encoding reached the shared factory instance";
}

TEST(TextUnitContractTests, TheGatedBomAndFallbackBehavioursAreStillWhatTheyWere) {
    // #2016 LANDED: no factory prepends a byte-order mark to GetBytes any more; the mark is
    // what GetPreamble() reports. The two encodings now agree, which they did not before.
    const auto utf32 = Encoding::UTF32()->GetBytes("A");
    ASSERT_EQ(4u, utf32.size());
    EXPECT_EQ('A', utf32[0]);
    const auto utf16 = Encoding::Unicode()->GetBytes("A");
    ASSERT_EQ(2u, utf16.size());
    EXPECT_EQ('A', utf16[0]);

    // #2017 LANDED: a configured fallback reaches every encoding, and a truncated trailing
    // fixed-width unit is substituted rather than dropped. Asserted in full by
    // TextGatedBehaviourPinTests.Fix2017_*; the rows kept here are the DEFAULT-configuration
    // ones, which is what this contract file is for.
    const std::vector<bytecs> loneSurrogate{0x00, 0xD8};
    EXPECT_EQ("\xEF\xBF\xBD",
              System::Text::UnicodeEncoding(false, false).GetString(loneSurrogate.data(), 0, 2));

    const std::vector<bytecs> oddLength{'A', 0x00, 'B'};
    EXPECT_EQ("A\xEF\xBF\xBD",
              System::Text::UnicodeEncoding(false, false).GetString(oddLength.data(), 0, 3));
}


// ===========================================================================================
// #2015 / SR-AUD-290 + SR-AUD-296 (cause T-I) — DECIDED 2026-08-17 as a DECLARED DEVIATION
//
// Plan §14.3 offered three options for what a public index means in System::Text: (A) UTF-16
// code units, (B) scalar counts, (C) keep UTF-8 storage bytes and declare it. The ticket's own
// constraint decides it: the unit "should not be decided without deciding it for System::String,
// which carries the same adaptation" -- and System::String IS a UTF-8 std::string throughout
// this runtime, so options A and B are not a change to System::Text, they are a re-architecture
// of every index in the port.
//
// What makes (C) the FAITHFUL answer rather than merely the cheap one is the measurement below:
// a byte index into UTF-8 is the exact analogue of a code-unit index into UTF-16, INCLUDING the
// ability to split a character. .NET's StringBuilder.Remove validates only the numeric range
// (StringBuilder.cs:1024-1042) -- there is no surrogate-pair guard -- so removing one unit of a
// surrogate pair leaves a LONE SURROGATE and an ill-formed string. This port removing one byte
// of a two-byte sequence leaves an ill-formed UTF-8 string. Same hazard, same absence of a
// guard, different unit. Adopting .NET's UNIT would not have removed the hazard; it would have
// moved it to a different character.
//
// These cases pin the declared contract so it cannot drift into an undeclared one.
// ===========================================================================================

TEST(TextUnitContractTests, Decl2015_PublicIndicesAndCountsAreUtf8StorageBytes) {
    // U+1F600 is four UTF-8 bytes here and two UTF-16 code units in .NET.
    const std::vector<bytecs> grin{0xF0, 0x9F, 0x98, 0x80};
    EXPECT_EQ(4, System::Text::UTF8Encoding().GetCharCount(grin.data(), 0, 4))
        << "declared: GetCharCount counts UTF-8 storage bytes, .NET counts UTF-16 code units";

    System::Text::StringBuilder sb;
    sb.Append("\xC3\xA9");                 // U+00E9, two storage bytes
    EXPECT_EQ(2, sb.getLengthProperty())
        << "declared: StringBuilder::Length is a byte count";
}

TEST(TextUnitContractTests, Decl2015_SplittingACharacterIsPossibleHereAndInDotNetAlike) {
    // The hazard the unit choice does NOT remove, in either runtime. Removing one byte of a
    // two-byte sequence leaves ill-formed UTF-8 -- exactly as removing one unit of a surrogate
    // pair leaves a lone surrogate in .NET, whose StringBuilder.Remove has no guard either.
    System::Text::StringBuilder sb;
    sb.Append("\xC3\xA9");   // U+00E9
    sb.Append("A");
    sb.Remove(1, 1);
    const std::string result = sb.ToString();
    ASSERT_EQ(2u, result.size());
    EXPECT_EQ('\xC3', result[0]) << "the leading byte of the split sequence survives";
    EXPECT_EQ('A', result[1]);

    // Stated plainly so the pin cannot be mistaken for an endorsement of the OUTPUT: this is
    // ill-formed UTF-8, it is reachable through a documented public API, and it is reachable
    // through .NET's equivalent too. What is asserted is that the behaviour is the DECLARED one.
}

TEST(TextUnitContractTests, Decl2015_TheUnitIsCONSISTENTAcrossTheComponent) {
    // The property that actually matters for a caller, and the one an undeclared unit would
    // break: every index-taking member agrees about what an index is. A mixture would be far
    // worse than either unit consistently applied.
    System::Text::StringBuilder sb;
    sb.Append("\xC3\xA9");   // 2 bytes
    sb.Append("\xE2\x82\xAC");   // U+20AC, 3 bytes
    ASSERT_EQ(5, sb.getLengthProperty());

    sb.Insert(2, "X");        // at the boundary between the two characters
    EXPECT_EQ("\xC3\xA9" "X" "\xE2\x82\xAC", sb.ToString());
    EXPECT_EQ(6, sb.getLengthProperty());

    const auto bytes = System::Text::UTF8Encoding().GetBytes(sb.ToString());
    EXPECT_EQ(static_cast<std::size_t>(sb.getLengthProperty()), bytes.size())
        << "Length and GetBytes must agree about the unit";
}
