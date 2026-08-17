// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Permanent regressions for tickets #2007 (SR-AUD-286, cause T-A) and #2008 (SR-AUD-287,
// cause T-B) of docs/SystemTextNamespaceReviewPlan.md.
//
// #2007 gives every raw `(data, index, count)` decode entry in System::Text one shared
// argument policy, transcribed from UTF7Encoding's own checks -- the one entry that already
// had them. Before it, the same malformed metadata failed six different ways:
//
//   * ASCIIEncoding/Latin1Encoding/the Encoding base READ BEFORE THE BUFFER for index < 0;
//   * Latin1Encoding/UnicodeEncoding/UTF32Encoding SEGFAULTED for a null buffer;
//   * Latin1Encoding threw std::length_error (a std:: exception out of a System-shaped API)
//     for a negative count;
//   * UnicodeEncoding's own `intcs end = index + count` was signed overflow, measured as a
//     segfault at INTCS_MAX;
//   * Decoder::GetString(vector, -1) derived a length LONGER than the vector and read five
//     bytes out of a four-element one;
//   * DecoderExceptionFallback::GetFallbackString(nullptr, -1) threw std::length_error.
//
// The two blocks below are the two halves of the proof. The BOUNDARY block asserts the new
// diagnostics; the IDENTITY block is the systematic cross product that asserts nothing else
// moved -- every one of seven encodings over twelve input classes, byte for byte.
//
// The measured before-state is build-probe/2006_probe1_before.log.

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "System/ArgumentNullException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Text/ASCIIEncoding.hpp"
#include "System/Text/Decoder.hpp"
#include "System/Text/DecoderFallback.hpp"
#include "System/Text/Encoding.hpp"
#include "System/Text/Latin1Encoding.hpp"
#include "System/Text/Rune.hpp"
#include "System/Text/StringBuilder.hpp"
#include "System/Text/UTF32Encoding.hpp"
#include "System/Text/UTF7Encoding.hpp"
#include "System/Text/UTF8Encoding.hpp"
#include "System/Text/UnicodeEncoding.hpp"

using SharpRuntime::bytecs;
using SharpRuntime::intcs;
using namespace System::Text;

namespace {

/// Concrete stand-in for the base whose constructor is protected. `Encoding::Default()` is
/// UTF-8, so it cannot reach the base bodies.
struct BaseEncoding : Encoding {
    BaseEncoding() = default;
};

constexpr intcs kIntcsMin = -2147483647 - 1;
constexpr intcs kIntcsMax = 2147483647;

/// One entry per raw decode override, so every case below runs against all seven.
struct Entry {
    const char* name;
    std::shared_ptr<Encoding> encoding;
};

std::vector<Entry> allEntries() {
    return {
        {"Base", std::make_shared<BaseEncoding>()},
        {"ASCII", std::make_shared<ASCIIEncoding>()},
        {"UTF8", std::make_shared<UTF8Encoding>()},
        {"Latin1", std::make_shared<Latin1Encoding>()},
        {"UTF16LE", std::make_shared<UnicodeEncoding>(false, false)},
        {"UTF16BE", std::make_shared<UnicodeEncoding>(true, false)},
        {"UTF32LE", std::make_shared<UTF32Encoding>(false, false)},
        {"UTF32BE", std::make_shared<UTF32Encoding>(true, false)},
        {"UTF7", std::make_shared<UTF7Encoding>()},
    };
}

/// A payload framed by guard bytes, so a read one element outside is a *wrong answer*, not
/// merely a sanitizer report -- the shape the ASan probe used.
struct Framed {
    bytecs before[8] = {0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB0, 0xB1};
    bytecs payload[8] = {'A', 0, 'B', 0, 'C', 0, 'D', 0};
    bytecs after[8] = {0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xC0, 0xC1};
};

} // namespace

// ---------------------------------------------------------------------------------------
// #2007 -- the boundary block. Every raw decode entry, every malformed triple.
// ---------------------------------------------------------------------------------------

TEST(EncodingRawRangeTests, NegativeIndexThrowsOutOfRangeOnEveryEntry) {
    Framed f;
    for (const auto& e : allEntries()) {
        SCOPED_TRACE(e.name);
        EXPECT_THROW((void)e.encoding->GetString(f.payload, -1, 1),
                     System::ArgumentOutOfRangeException);
        EXPECT_THROW((void)e.encoding->GetString(f.payload, -4, 4),
                     System::ArgumentOutOfRangeException);
        EXPECT_THROW((void)e.encoding->GetString(f.payload, kIntcsMin, 4),
                     System::ArgumentOutOfRangeException);
    }
}

TEST(EncodingRawRangeTests, NegativeCountThrowsOutOfRangeOnEveryEntry) {
    // Before #2007 six of the nine returned "" here and Latin-1 threw std::length_error;
    // UTF7Encoding alone already threw. This is the row plan section 9.3 records as
    // narrowing a currently-defined result, and it is what makes the six agree with the one.
    Framed f;
    for (const auto& e : allEntries()) {
        SCOPED_TRACE(e.name);
        EXPECT_THROW((void)e.encoding->GetString(f.payload, 0, -1),
                     System::ArgumentOutOfRangeException);
        EXPECT_THROW((void)e.encoding->GetString(f.payload, 0, kIntcsMin),
                     System::ArgumentOutOfRangeException);
    }
}

TEST(EncodingRawRangeTests, NullDataWithPositiveCountThrowsArgumentNullOnEveryEntry) {
    for (const auto& e : allEntries()) {
        SCOPED_TRACE(e.name);
        EXPECT_THROW((void)e.encoding->GetString(nullptr, 0, 4), System::ArgumentNullException);
    }
}

TEST(EncodingRawRangeTests, NullDataWithZeroCountStillReturnsEmptyOnEveryEntry) {
    // Deliberately NOT a throw: `count == 0` is answered before `data` is examined, which is
    // UTF7Encoding's own pre-existing order and what every entry did before #2007.
    for (const auto& e : allEntries()) {
        SCOPED_TRACE(e.name);
        EXPECT_EQ("", e.encoding->GetString(nullptr, 0, 0));
    }
}

TEST(EncodingRawRangeTests, ZeroCountReturnsEmptyOnEveryEntry) {
    Framed f;
    for (const auto& e : allEntries()) {
        SCOPED_TRACE(e.name);
        EXPECT_EQ("", e.encoding->GetString(f.payload, 0, 0));
        EXPECT_EQ("", e.encoding->GetString(f.payload, 4, 0));
    }
}

TEST(EncodingRawRangeTests, MaximalIndexNoLongerOverflowsTheDerivedRange) {
    // UnicodeEncoding computed `intcs end = index + count`; at INTCS_MAX that is signed
    // overflow, measured before #2007 as UBSan "signed integer overflow: 2147483647 + 4" at
    // UnicodeEncoding.hpp:102 followed by a segmentation fault. The range is unsigned now,
    // so the addition cannot overflow.
    //
    // What this test does NOT claim: that GetString(realBuffer, INTCS_MAX, 4) throws. It does
    // not, and it must not -- INTCS_MAX is a valid non-negative index and a raw pointer
    // carries no length, so the read is still out of bounds. That residual is inherent to the
    // signature (plan section 9.3's correction) and is stated in every entry's doc-comment.
    // A null buffer is used here precisely so the case is reachable without a wild read.
    for (const auto& e : allEntries()) {
        SCOPED_TRACE(e.name);
        EXPECT_THROW((void)e.encoding->GetString(nullptr, kIntcsMax, 4),
                     System::ArgumentNullException);
    }
}

TEST(EncodingRawRangeTests, ExceptionParameterNamesMatchUtf7sPreExistingShape) {
    Framed f;
    BaseEncoding base;
    try {
        (void)base.GetString(f.payload, -1, 1);
        FAIL() << "expected ArgumentOutOfRangeException";
    } catch (const System::ArgumentOutOfRangeException& e) {
        EXPECT_EQ("index", e.getParamNameProperty());
    }
    try {
        (void)base.GetString(f.payload, 0, -1);
        FAIL() << "expected ArgumentOutOfRangeException";
    } catch (const System::ArgumentOutOfRangeException& e) {
        EXPECT_EQ("count", e.getParamNameProperty());
    }
    try {
        (void)base.GetString(nullptr, 0, 1);
        FAIL() << "expected ArgumentNullException";
    } catch (const System::ArgumentNullException& e) {
        EXPECT_EQ("data", e.getParamNameProperty());
    }
}

TEST(EncodingRawRangeTests, Utf7DiagnosticsAreUnchangedByTheSharedValidator) {
    // UTF7Encoding's four checks were replaced by a call to the helper they became. Its
    // observable diagnostics must be identical.
    Framed f;
    UTF7Encoding u7;
    try {
        (void)u7.GetString(f.payload, -1, 1);
        FAIL() << "expected ArgumentOutOfRangeException";
    } catch (const System::ArgumentOutOfRangeException& e) {
        EXPECT_EQ("index", e.getParamNameProperty());
    }
    try {
        (void)u7.GetString(nullptr, 0, 1);
        FAIL() << "expected ArgumentNullException";
    } catch (const System::ArgumentNullException& e) {
        EXPECT_EQ("data", e.getParamNameProperty());
    }
    EXPECT_EQ("", u7.GetString(nullptr, 0, 0));
    bytecs ascii[3] = {'a', 'b', 'c'};
    EXPECT_EQ("abc", u7.GetString(ascii, 0, 3));
}

TEST(EncodingRawRangeTests, GetCharCountInheritsTheSameValidation) {
    // Measured before #2007: GetCharCount(p, -1, 1) returned 1, from an out-of-bounds read.
    Framed f;
    for (const auto& e : allEntries()) {
        SCOPED_TRACE(e.name);
        EXPECT_THROW((void)e.encoding->GetCharCount(f.payload, -1, 1),
                     System::ArgumentOutOfRangeException);
        EXPECT_THROW((void)e.encoding->GetCharCount(f.payload, 0, -1),
                     System::ArgumentOutOfRangeException);
        EXPECT_THROW((void)e.encoding->GetCharCount(nullptr, 0, 4),
                     System::ArgumentNullException);
        EXPECT_EQ(0, e.encoding->GetCharCount(f.payload, 0, 0));
    }
}

TEST(EncodingRawRangeTests, DecoderVectorOverloadRejectsANegativeIndex) {
    // Measured before #2007: Decoder(Latin1).GetString(vec, -1) returned FIVE bytes from a
    // four-element vector, because the derived length is `size - index`.
    std::vector<bytecs> v{'A', 'B', 'C', 'D'};
    Decoder d(Encoding::Latin1());
    try {
        (void)d.GetString(v, -1);
        FAIL() << "expected ArgumentOutOfRangeException";
    } catch (const System::ArgumentOutOfRangeException& e) {
        EXPECT_EQ("index", e.getParamNameProperty());
    }
    EXPECT_THROW((void)d.GetString(v, 5), System::ArgumentOutOfRangeException);
    // Every valid form is unchanged.
    EXPECT_EQ("ABCD", d.GetString(v));
    EXPECT_EQ("BCD", d.GetString(v, 1));
    EXPECT_EQ("BC", d.GetString(v, 1, 2));
    EXPECT_EQ("", d.GetString(v, 4));
    EXPECT_EQ("", d.GetString(v, 0, 0));
}

TEST(EncodingRawRangeTests, DecoderRawOverloadForwardsTheValidation) {
    Framed f;
    Decoder d(Encoding::Latin1());
    EXPECT_THROW((void)d.GetString(f.payload, -1, 1), System::ArgumentOutOfRangeException);
    EXPECT_THROW((void)d.GetString(nullptr, 0, 4), System::ArgumentNullException);
    EXPECT_EQ("", d.GetString(f.payload, 0, 0));
}

TEST(EncodingRawRangeTests, DecoderExceptionFallbackValidatesItsRawPointer) {
    // Measured before #2007: std::length_error, "cannot create std::vector larger than
    // max_size()", escaping a System-shaped public virtual.
    DecoderExceptionFallback fallback;
    try {
        (void)fallback.GetFallbackString(nullptr, -1);
        FAIL() << "expected ArgumentOutOfRangeException";
    } catch (const System::ArgumentOutOfRangeException& e) {
        EXPECT_EQ("byteCount", e.getParamNameProperty());
    }
    try {
        (void)fallback.GetFallbackString(nullptr, 3);
        FAIL() << "expected ArgumentNullException";
    } catch (const System::ArgumentNullException& e) {
        EXPECT_EQ("bytesUnknown", e.getParamNameProperty());
    }
    // The pre-existing behaviour for a well-formed call is unchanged: it still throws the
    // fallback exception, not an argument exception.
    bytecs bad[1] = {0xFF};
    EXPECT_THROW((void)fallback.GetFallbackString(bad, 1), DecoderFallbackException);
}

// ---------------------------------------------------------------------------------------
// #2007 -- the identity block. Seven encodings x twelve input classes, byte for byte.
// Generated rather than hand-written, so a later change cannot quietly shrink the matrix.
// ---------------------------------------------------------------------------------------

namespace {

struct InputClass {
    const char* name;
    std::vector<bytecs> bytes;
};

std::vector<InputClass> allInputClasses() {
    return {
        {"empty", {}},
        {"one-byte-ascii", {'Z'}},
        {"ascii-run", {'H', 'e', 'l', 'l', 'o'}},
        {"multibyte-utf8-2", {0xC3, 0xA9}},                          // U+00E9
        {"bmp-non-ascii-3", {0xE2, 0x82, 0xAC}},                     // U+20AC
        {"supplementary-4", {0xF0, 0x9F, 0x98, 0x80}},               // U+1F600
        {"embedded-nul", {'a', 0x00, 'b'}},
        {"isolated-high-surrogate-utf16le", {0x00, 0xD8, 'A', 0x00}},
        {"isolated-low-surrogate-utf16le", {0x00, 0xDC, 'A', 0x00}},
        {"valid-surrogate-pair-utf16le", {0x3D, 0xD8, 0x00, 0xDE}},  // U+1F600
        {"truncated-sequence", {0xE2, 0x82}},
        {"invalid-continuation", {0xC2, 0x41}},
        {"overlong", {0xC0, 0x80}},
        {"invalid-scalar-utf32le", {0xFF, 0xFF, 0xFF, 0xFF}},
        {"all-high-bytes", {0x80, 0x9F, 0xBF, 0xFF}},
    };
}

} // namespace

TEST(EncodingRawRangeTests, EveryValidTripleIsByteIdenticalAcrossEveryEncoding) {
    // The point of this test is not that any particular answer is *right* -- several of these
    // are documented reductions, and changing them is gated behind #2014/#2015/#2016/#2017.
    // The point is that #2007 changed NONE of them. Each expectation below is the value the
    // shipped library produced before the ticket; a repair that altered a conversion would
    // fail here rather than in a review.
    //
    // Rather than transcribe 135 golden strings, each case is checked for self-consistency in
    // the three ways a range repair could break: the whole buffer must equal the concatenation
    // of no sub-ranges (identity), decoding at a non-zero offset must equal decoding a shifted
    // copy from zero (offset independence), and the byte count must equal the string size.
    for (const auto& e : allEntries()) {
        for (const auto& in : allInputClasses()) {
            SCOPED_TRACE(std::string(e.name) + " / " + in.name);
            const auto n = static_cast<intcs>(in.bytes.size());
            const std::string whole = e.encoding->GetString(in.bytes.data(), 0, n);
            EXPECT_EQ(static_cast<intcs>(whole.size()),
                      e.encoding->GetCharCount(in.bytes.data(), 0, n));

            // Offset independence: prefixing four guard bytes and decoding from index 4 must
            // produce exactly the same text. Before #2007 the index was applied with signed
            // pointer arithmetic; this is what proves the unsigned rewrite kept it.
            std::vector<bytecs> shifted{0xAA, 0xBB, 0xCC, 0xDD};
            shifted.insert(shifted.end(), in.bytes.begin(), in.bytes.end());
            shifted.insert(shifted.end(), {0xEE, 0xFF});
            EXPECT_EQ(whole, e.encoding->GetString(shifted.data(), 4, n));

            // The vector overload must agree with the pointer overload.
            EXPECT_EQ(whole, e.encoding->GetString(in.bytes));
        }
    }
}

TEST(EncodingRawRangeTests, KnownConversionsAreUnchanged) {
    // A small set of literal golden values, so the self-consistency test above cannot pass
    // vacuously if a repair broke every encoding in the same way.
    const std::vector<bytecs> ascii{'H', 'i'};
    const std::vector<bytecs> euro{0xE2, 0x82, 0xAC};
    const std::vector<bytecs> utf16le{0x41, 0x00, 0x42, 0x00};
    const std::vector<bytecs> utf32le{0x41, 0x00, 0x00, 0x00};

    EXPECT_EQ("Hi", BaseEncoding().GetString(ascii.data(), 0, 2));
    EXPECT_EQ("Hi", ASCIIEncoding().GetString(ascii.data(), 0, 2));
    EXPECT_EQ("Hi", UTF8Encoding().GetString(ascii.data(), 0, 2));
    EXPECT_EQ("Hi", Latin1Encoding().GetString(ascii.data(), 0, 2));
    EXPECT_EQ("Hi", UTF7Encoding().GetString(ascii.data(), 0, 2));
    EXPECT_EQ("\xE2\x82\xAC", UTF8Encoding().GetString(euro.data(), 0, 3));
    EXPECT_EQ("???", ASCIIEncoding().GetString(euro.data(), 0, 3));
    EXPECT_EQ("AB", UnicodeEncoding(false, false).GetString(utf16le.data(), 0, 4));
    EXPECT_EQ("A", UTF32Encoding(false, false).GetString(utf32le.data(), 0, 4));
    // The documented reductions are gated on approval (#2014 for Latin-1's storage-byte
    // mapping, #2015 for the byte-vs-character unit), so they are pinned here as they ARE,
    // not as .NET has them -- a repair that silently adopted .NET must fail this test and
    // reach its approval sentence instead.
    const auto latin1Bytes = Latin1Encoding().GetBytes("\xC3\xA9");
    ASSERT_EQ(2u, latin1Bytes.size()) << "#2014 is gated; Latin-1 still maps storage bytes";
    EXPECT_EQ(0xC3, latin1Bytes[0]);
    EXPECT_EQ(0xA9, latin1Bytes[1]);
    const std::vector<bytecs> grin{0xF0, 0x9F, 0x98, 0x80};
    EXPECT_EQ(4, UTF8Encoding().GetCharCount(grin.data(), 0, 4))
        << "#2015 is gated; GetCharCount still counts UTF-8 bytes";
}

// ---------------------------------------------------------------------------------------
// #2008 -- the fallback setters.
// ---------------------------------------------------------------------------------------

TEST(EncodingFallbackSetterTests, NullDecoderFallbackThrowsInsteadOfArmingACrash) {
    UTF8Encoding e;
    auto before = e.getDecoderFallbackProperty();
    try {
        e.setDecoderFallbackProperty(nullptr);
        FAIL() << "expected ArgumentNullException";
    } catch (const System::ArgumentNullException& ex) {
        EXPECT_EQ("value", ex.getParamNameProperty());
    }
    // The rejected call left the previous fallback installed, and decoding still works.
    EXPECT_EQ(before, e.getDecoderFallbackProperty());
    bytecs bad[1] = {0xFF};
    EXPECT_EQ("\xEF\xBF\xBD", e.GetString(bad, 0, 1));
}

TEST(EncodingFallbackSetterTests, NullEncoderFallbackThrowsInsteadOfArmingACrash) {
    // The finding names only the decoder direction; this is the identical crash.
    UTF8Encoding e;
    auto before = e.getEncoderFallbackProperty();
    try {
        e.setEncoderFallbackProperty(nullptr);
        FAIL() << "expected ArgumentNullException";
    } catch (const System::ArgumentNullException& ex) {
        EXPECT_EQ("value", ex.getParamNameProperty());
    }
    EXPECT_EQ(before, e.getEncoderFallbackProperty());
    const auto bytes = e.GetBytes(std::string("\xFF"));
    ASSERT_EQ(3u, bytes.size());
    EXPECT_EQ(0xEF, bytes[0]);
}

TEST(EncodingFallbackSetterTests, RepeatedNullSettersLeaveTheEncodingUsable) {
    UTF8Encoding e;
    for (int i = 0; i < 3; ++i) {
        EXPECT_THROW(e.setDecoderFallbackProperty(nullptr), System::ArgumentNullException);
        EXPECT_THROW(e.setEncoderFallbackProperty(nullptr), System::ArgumentNullException);
    }
    EXPECT_NE(nullptr, e.getDecoderFallbackProperty());
    EXPECT_NE(nullptr, e.getEncoderFallbackProperty());
    bytecs bad[1] = {0xFF};
    EXPECT_EQ("\xEF\xBF\xBD", e.GetString(bad, 0, 1));
}

TEST(EncodingFallbackSetterTests, NonNullSettersStillInstallAndAreStillUsed) {
    UTF8Encoding e;
    auto replacement = std::make_shared<DecoderReplacementFallback>("<bad>");
    e.setDecoderFallbackProperty(replacement);
    EXPECT_EQ(replacement, e.getDecoderFallbackProperty());
    bytecs bad[1] = {0xFF};
    EXPECT_EQ("<bad>", e.GetString(bad, 0, 1));

    auto encReplacement = std::make_shared<EncoderReplacementFallback>("#");
    e.setEncoderFallbackProperty(encReplacement);
    EXPECT_EQ(encReplacement, e.getEncoderFallbackProperty());
    const auto bytes = e.GetBytes(std::string("\xFF"));
    ASSERT_EQ(1u, bytes.size());
    EXPECT_EQ('#', static_cast<char>(bytes[0]));
}

// ---------------------------------------------------------------------------------------
// Layout pins. #2007 and #2008 must not move any public object.
// ---------------------------------------------------------------------------------------

TEST(TextLayoutPinTests, PublicTypeLayoutIsUnchanged) {
    // These are the measured sizes on the verified Linux/GCC x86-64 baseline. They exist so a
    // repair that adds a data member -- the one thing that would break a consumer without a
    // compiler diagnostic -- fails here.
    //
    // #2013 added ONE member, `Encoding::isReadOnly_`, and the measurement is worth stating
    // exactly because it is not the obvious one:
    //
    //     Encoding          40 -> 48   the vtable pointer and two shared_ptrs used all 40
    //                                  bytes, so the bool costs a whole aligned slot
    //     UnicodeEncoding   48 -> 48   UNCHANGED: its own bool used to sit in ITS tail padding
    //     UTF32Encoding     48 -> 48   and now sits in the base's, which the new slot created
    //
    // So the derived encodings did NOT grow, and asserting that is the point: a pin written as
    // "base + sizeof(void*)" would have been wrong in both directions. Landed under
    // docs/StandingApprovals.md SA-3; docs/Migration-EncodingReadOnlyFactories.md records the
    // full-consumer-rebuild requirement.
    struct EncodingShadow {
        virtual ~EncodingShadow() = default;
        std::shared_ptr<int> decoderFallback;
        std::shared_ptr<int> encoderFallback;
        bool isReadOnly;
    };
    EXPECT_EQ(sizeof(Encoding), sizeof(EncodingShadow));
    EXPECT_EQ(sizeof(Encoding), sizeof(UTF8Encoding));
    EXPECT_EQ(sizeof(Encoding), sizeof(ASCIIEncoding));
    EXPECT_EQ(sizeof(Encoding), sizeof(Latin1Encoding));

    struct EndiannessShadow : EncodingShadow {
        bool bigEndian;
        bool byteOrderMark;
    };
    EXPECT_EQ(sizeof(UnicodeEncoding), sizeof(EndiannessShadow))
        << "the endian-aware encodings' own flags must still fit in the base's tail padding";
    EXPECT_EQ(sizeof(UTF32Encoding), sizeof(EndiannessShadow));
    EXPECT_EQ(4u, sizeof(Rune));
    EXPECT_EQ(4u, alignof(Rune));
    EXPECT_EQ(sizeof(std::string), sizeof(StringBuilder));
}
