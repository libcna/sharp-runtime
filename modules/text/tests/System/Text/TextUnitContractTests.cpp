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
    // represented. .NET's Latin1Encoding uses the replacement fallback "?" for it, and this
    // component's ASCIIEncoding already did the same for the same reason -- including the part
    // that is easy to miss: a supplementary-plane scalar produces TWO '?', matching the two
    // UTF-16 code units .NET would encode it from.
    Latin1Encoding l;
    EXPECT_EQ(std::vector<bytecs>{static_cast<bytecs>('?')}, l.GetBytes("\xE2\x82\xAC"));  // U+20AC
    const auto grin = l.GetBytes("\xF0\x9F\x98\x80");                                     // U+1F600
    EXPECT_EQ(2u, grin.size());
    EXPECT_EQ('?', grin[0]);
    EXPECT_EQ('?', grin[1]);
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
    // #2016 (plan section 14.4): the DEFAULT UTF-32 factory prepends a byte-order mark as
    // payload, which .NET states GetBytes does not do; the UTF-16 factory does not.
    const auto utf32 = Encoding::UTF32()->GetBytes("A");
    ASSERT_EQ(8u, utf32.size()) << "gated by #2016";
    EXPECT_EQ(0xFF, utf32[0]);
    EXPECT_EQ(0xFE, utf32[1]);
    const auto utf16 = Encoding::Unicode()->GetBytes("A");
    ASSERT_EQ(2u, utf16.size());
    EXPECT_EQ('A', utf16[0]);

    // #2017 (plan section 14.5): a configured EXCEPTION decoder fallback is inert outside
    // UTF8Encoding, and a truncated trailing fixed-width unit is dropped without reaching a
    // fallback at all.
    System::Text::UnicodeEncoding u16(false, false);
    u16.setDecoderFallbackProperty(System::Text::DecoderFallback::ExceptionFallback());
    const std::vector<bytecs> loneSurrogate{0x00, 0xD8};
    EXPECT_NO_THROW((void)u16.GetString(loneSurrogate.data(), 0, 2)) << "gated by #2017";

    const std::vector<bytecs> oddLength{'A', 0x00, 'B'};
    EXPECT_EQ("A", System::Text::UnicodeEncoding(false, false).GetString(oddLength.data(), 0, 3))
        << "gated by #2017: the trailing byte is dropped, not substituted";
}
