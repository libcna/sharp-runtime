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

TEST(TextUnitContractTests, Latin1MapsStorageBytesNotCodePoints) {
    Latin1Encoding l;
    const auto bytes = l.GetBytes("\xC3\xA9");
    ASSERT_EQ(2u, bytes.size()) << "gated by #2014 (plan section 14.2): ISO-8859-1 is one byte, e9";
    EXPECT_EQ(0xC3, bytes[0]);
    EXPECT_EQ(0xA9, bytes[1]);

    const std::vector<bytecs> e9{0xE9};
    const std::string decoded = l.GetString(e9.data(), 0, 1);
    ASSERT_EQ(1u, decoded.size()) << "gated by #2014: the UTF-8 answer is c3 a9";
    EXPECT_EQ('\xE9', decoded[0]);

    // ASCII round-trips correctly in both directions, which is the whole of what works.
    const auto hi = l.GetBytes("Hi");
    EXPECT_EQ("Hi", l.GetString(hi.data(), 0, static_cast<SharpRuntime::intcs>(hi.size())));
}

TEST(TextUnitContractTests, TheFactoryEncodingsAreStillOneSharedMutableObject) {
    // Pinned for #2013 (plan section 14.1): .NET's factory encodings are read-only, and this
    // port's are not. A repair that makes the setter throw must fail here and reach its
    // approval sentence.
    EXPECT_EQ(Encoding::UTF8().get(), Encoding::UTF8().get());
    EXPECT_EQ(Encoding::ASCII().get(), Encoding::ASCII().get());

    auto saved = Encoding::UTF8()->getDecoderFallbackProperty();
    Encoding::UTF8()->setDecoderFallbackProperty(
        std::make_shared<System::Text::DecoderReplacementFallback>("<X>"));
    const std::vector<bytecs> bad{0xFF};
    EXPECT_EQ("<X>", Encoding::UTF8()->GetString(bad.data(), 0, 1))
        << "gated by #2013: a caller mutating the shared factory instance changes what every "
           "other caller decodes";
    Encoding::UTF8()->setDecoderFallbackProperty(saved);
    EXPECT_EQ("\xEF\xBF\xBD", Encoding::UTF8()->GetString(bad.data(), 0, 1));
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
